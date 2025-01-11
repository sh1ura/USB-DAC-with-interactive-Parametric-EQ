/*
  USB DAC with ineractive Parametric EQ

  by @shiura
 */

#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/usb_device.h"
#include "pico/audio.h"
#include "pico/audio_i2s.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "lufa/AudioClassCommon.h"

#include "pico/util/datetime.h"
#include "DEV_Config.h"

///////// add filtering
#include "draw.h"
#include "eq.h"

static struct {
  uint32_t freq;
  int16_t volume;
  int32_t vol_mul;
  bool mute;
} audio_state = {
  .freq = 48000,
};

#define DEBUG_FEEDBACK 0 // for debuggin USB audio rate feedback

#define BUFFER_NUM 8 // buffer for I2S (corresponds to msec)
#if DEBUG_FEEDBACK
int numFreeBuf;
#endif

// RP2040 : EQ_CH <= 4   RP2350 : EQ_CH <= 8
#define EQ_CH 4 // channnel of parametric equalizer

#if EQ_CH == 2
const int defaultFreq[] = {100, 1000};
#elif EQ_CH == 3
const int defaultFreq[] = {100, 500, 3000};
#elif EQ_CH == 4
const int defaultFreq[] = {100, 300, 1000, 3000};
#else
const int defaultFreq[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000};
#endif
  
#define DEFAULT_BW 3
#define DEFAULT_GAIN 0
#define GAIN_STEP 0.2
#define BW_STEP 1.18920711 // 1/4 octave

int currentFreq = 48000;

typedef struct {
  double freqCenter[EQ_CH];
  double bandWidth[EQ_CH]; // FWHM (full width at half maximum) in octave
  double gain[EQ_CH]; // in dB
  double totalGain;
} Settings;

Settings setting;

#define SHIFT_C 20 // fixed point for coeffs
#define SHIFT_V 16 // fixed point for value

BiQuadCoeffs coeff[EQ_CH];
typedef struct {
  int64_t a1, a2, b0, b1, b2; // filter fixed point coeffs
} BinCoeffs;
BinCoeffs bc[EQ_CH]; // binary coefficient
int64_t gainBC = (1 << SHIFT_V);

volatile bool sw = true; // switch of filter on/off

static void drawResponse(void);

static void calcBinCoeffs(void) {
  for(int i = 0; i < EQ_CH; i++) {
    coeff[i] = calcCoeffs((double)currentFreq, setting.freqCenter[i] , setting.bandWidth[i], setting.gain[i]);
    bc[i].a1 = coeff[i].a1 * (1 << SHIFT_C) + 0.5;
    bc[i].a2 = coeff[i].a2 * (1 << SHIFT_C) + 0.5;
    bc[i].b0 = coeff[i].b0 * (1 << SHIFT_C) + 0.5;
    bc[i].b1 = coeff[i].b1 * (1 << SHIFT_C) + 0.5;
    bc[i].b2 = coeff[i].b2 * (1 << SHIFT_C) + 0.5;
  }
  gainBC = (int64_t) ((double)(1 << SHIFT_V) / pow(10, setting.totalGain/20));
}

static int32_t filterL(int64_t in) {
  static int64_t in1[EQ_CH], in2[EQ_CH], out1[EQ_CH], out2[EQ_CH], out;
    
  if(!sw) {
    return in;
  }
  in <<= SHIFT_V;
  for(int i = 0; i < EQ_CH; i++) {
    out = bc[i].b0 * in + bc[i].b1 * in1[i] + bc[i].b2 * in2[i] - bc[i].a1 * out1[i] - bc[i].a2 * out2[i];
    out >>= SHIFT_C;
    in2[i] = in1[i];    in1[i] = in;
    out2[i] = out1[i];  out1[i] = out;
    in = out;
  }
  return out / gainBC;
}

static int32_t filterR(int64_t in) {
  static int64_t in1[EQ_CH], in2[EQ_CH], out1[EQ_CH], out2[EQ_CH], out;
    
  if(!sw) {
    return in;
  }
  in <<= SHIFT_V;
  for(int i = 0; i < EQ_CH; i++) {
    out = bc[i].b0 * in + bc[i].b1 * in1[i] + bc[i].b2 * in2[i] - bc[i].a1 * out1[i] - bc[i].a2 * out2[i];
    out >>= SHIFT_C;
    in2[i] = in1[i];    in1[i] = in;
    out2[i] = out1[i];  out1[i] = out;
    in = out;
  }
  return out / gainBC;
}

#include <hardware/flash.h>
#include <hardware/watchdog.h>
#include <string.h>

#define SAVE_FLAG EQ_CH

// W25Q16JVの最終ブロック(Block31)のセクタ0の先頭アドレス = 0x1F0000
const uint32_t FLASH_TARGET_OFFSET = 0x1F0000;
static void core1_worker();

// from https://www.prototype00.com/2022/07/raspberry-pi-picoflash-cc.html
static void save_setting_to_flash(void) {
  uint8_t write_data[FLASH_PAGE_SIZE];

  write_data[0] = SAVE_FLAG; // flag
  memcpy(write_data + 1, &setting, sizeof(Settings));

  uint32_t ints = save_and_disable_interrupts();
  multicore_reset_core1();

  flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
  flash_range_program(FLASH_TARGET_OFFSET, write_data, FLASH_PAGE_SIZE);

  watchdog_reboot(0, 0, 10); // reset itself
  for(;;)
    ;
}

void load_setting_from_flash(void) {
  const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);
    
  if(flash_target_contents[0] != SAVE_FLAG) {
    return;
  }
  memcpy(&setting, flash_target_contents + 1, sizeof(Settings));
}

#define LCD_KEY_LT 2
#define LCD_KEY_LB 17
#define LCD_KEY_RT 3
#define LCD_KEY_RB 15
#define LCD_BACKLIGHT_PIN 13

#define LCDTIME 10000000 // LCD turn off timer (10sec)
#define CHATTER_WAIT 200000 // wait for avoid chattering
#define FIRST_CLICK_WAIT 500000 // wait of repeating key-in
volatile int64_t keyOnTime;
volatile bool lcdStatus = false;

void lcdOn(void) {
  gpio_put(LCD_BACKLIGHT_PIN, true);
  lcdStatus = true;
  keyOnTime = time_us_64();
}


void initSettings(void) {
  for(int i = 0; i < EQ_CH; i++) {
    setting.freqCenter[i] = defaultFreq[i];
    setting.bandWidth[i] = DEFAULT_BW;
    setting.gain[i] = DEFAULT_GAIN;
  }
  setting.totalGain = 0;
}

void myInit(void) {
  if(DEV_Module_Init()!=0){
    return;
  }

  initSettings();
  load_setting_from_flash();
  
  /*LCD Init*/
  LCD_2IN_Init(HORIZONTAL);
  LCD_2IN_Clear(WHITE);

  gpio_init(LCD_BACKLIGHT_PIN);
  gpio_set_dir(LCD_BACKLIGHT_PIN, GPIO_OUT);
    
  gpio_init(LCD_KEY_LT);
  gpio_set_dir(LCD_KEY_LT, GPIO_IN);
  gpio_pull_up(LCD_KEY_LT);
  gpio_init(LCD_KEY_LB);
  gpio_set_dir(LCD_KEY_LB, GPIO_IN);
  gpio_pull_up(LCD_KEY_LB);
  gpio_init(LCD_KEY_RT);
  gpio_set_dir(LCD_KEY_RT, GPIO_IN);
  gpio_pull_up(LCD_KEY_RT);
  gpio_init(LCD_KEY_RB);
  gpio_set_dir(LCD_KEY_RB, GPIO_IN);
  gpio_pull_up(LCD_KEY_RB);

  calcBinCoeffs();
  drawResponse();
  sendImage();
  lcdOn();
}

#define GRAPH_FREQ_MIN  20
#define GRAPH_FREQ_MAX  20000
#define GRAPH_GAIN_MAX  20 // dB
#define GRAPH_GAIN_MIN -20 // dB

#define MARGIN_LEFT 40
#define MARGIN_TOP 20
#define MARGIN_BOTTOM 40

#define COL(R,G,B) (((R)<<11)+((G)<<6)+B) // 5, 6, 5bits each
#define FREQ_TO_X(freq) (MARGIN_LEFT + (LCD_WIDTH - MARGIN_LEFT) * (log(freq) - log(GRAPH_FREQ_MIN)) / (log(GRAPH_FREQ_MAX) - log(GRAPH_FREQ_MIN)))
#define X_TO_FREQ(x) exp(((double)(x-MARGIN_LEFT)/(LCD_WIDTH-MARGIN_LEFT)) * (log(GRAPH_FREQ_MAX) - log(GRAPH_FREQ_MIN)) + log(GRAPH_FREQ_MIN))
#define GAIN_TO_Y(gain) (MARGIN_TOP + (LCD_HEIGHT - (MARGIN_TOP + MARGIN_BOTTOM)) * (1 - (((double)(gain) - GRAPH_GAIN_MIN) / (GRAPH_GAIN_MAX - GRAPH_GAIN_MIN))))

#define MODENUM (EQ_CH * 3 + 2)
#define MODE_SETTING (MODENUM - 2)
#define MODE_TOTALGAIN (MODENUM - 1)
int mode = MODENUM - 1;

char *modeStr[] = {
  " EQX FREQ",
  " EQX GAIN",
  " EQX WIDTH",
  " SETTINGS",
  "TOTAL GAIN"
};

static void drawAxis(void) {
  char str[100];

  clearImage(BLACK);

  for(int y = 20; y < LCD_HEIGHT - 20; y++){
    for(int x = 0; x < LCD_WIDTH; x++){
      image[y][x] = 0xFFFF;
    }
  }
  for(int y = MARGIN_TOP; y < LCD_HEIGHT - MARGIN_BOTTOM; y++) {
    drawPoint(FREQ_TO_X(   20), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X(   40), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X(   60), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X(   80), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X(  100), y, LGRAY);
    drawPoint(FREQ_TO_X(  200), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X(  400), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X(  600), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X(  800), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X( 1000), y, LGRAY);
    drawPoint(FREQ_TO_X( 2000), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X( 4000), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X( 6000), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X( 8000), y, COL(28, 28, 28));
    drawPoint(FREQ_TO_X(10000), y, LGRAY);
    drawPoint(FREQ_TO_X(20000), y, COL(28, 28, 28));
  }
  drawText(FREQ_TO_X(100) - 17, LCD_HEIGHT - 38, "100", BLACK);
  drawText(FREQ_TO_X(1000) - 11, LCD_HEIGHT - 38, "1k", BLACK);
  drawText(FREQ_TO_X(10000) - 17, LCD_HEIGHT - 38, "10k", BLACK);

  for(int x = MARGIN_LEFT; x < LCD_WIDTH; x++) {
    drawPoint(x, GAIN_TO_Y(-20), COL(25, 25, 25));
    drawPoint(x, GAIN_TO_Y(-15), COL(28, 28, 28));
    drawPoint(x, GAIN_TO_Y(-10), COL(25, 25, 25));
    drawPoint(x, GAIN_TO_Y( -5), COL(28, 28, 28));
    drawPoint(x, GAIN_TO_Y(  0), BLACK);
    drawPoint(x, GAIN_TO_Y(  5), COL(28, 28, 28));
    drawPoint(x, GAIN_TO_Y( 10), COL(25, 25, 25));
    drawPoint(x, GAIN_TO_Y( 15), COL(28, 28, 28));
  }
  drawText(2, GAIN_TO_Y(  0) - 7, "  0", BLACK);
  drawText(2, GAIN_TO_Y( 10) - 7, " 10", BLACK);
  drawText(2, GAIN_TO_Y(-10) - 7, "-10", BLACK);
  drawText(2, GAIN_TO_Y(-20) - 7, "-20", BLACK);
  
  drawText(0, 1, "EQ", WHITE);
  if(sw) {
    drawText(12 * 3, 1, "ON", RED);
    drawText(12 * 5, 1, "/OFF", WHITE);
  }
  else {
    drawText(12 * 3, 1, "ON/", WHITE);
    drawText(12 * 6, 1, "OFF", BLUE);
  }
  drawText(0, LCD_HEIGHT - 15, "SEL", WHITE);
  drawText(LCD_WIDTH-61, 1, mode == MODE_SETTING ? " SAVE" : "   UP", WHITE);
  drawText(LCD_WIDTH-61, LCD_HEIGHT - 15, mode == MODE_SETTING ? "RESET" : " DOWN", WHITE);

#if DEBUG_FEEDBACK
  sprintf(str, "%d", audio_state.volume);
  //  sprintf(str, "freeBuf=%d", numFreeBuf);
  drawText(120, 1, str, RED);
#else
  if(mode < EQ_CH * 3) {
    char p[20];
    strcpy(p, modeStr[mode % 3]);
    p[3] = '1' + mode / 3;
    drawText(LCD_WIDTH/2-60, LCD_HEIGHT - 15, p, GREEN);
    for(int y = MARGIN_TOP; y < LCD_HEIGHT - MARGIN_BOTTOM; y++) {
      drawPoint(FREQ_TO_X(setting.freqCenter[mode / 3]), y, GREEN);
    }
    int ch = mode / 3;
    switch(mode % 3) {
    case 0: // freq
      if(setting.freqCenter[ch] < 1000.0) {
	sprintf(str, "%dHz", (int)(setting.freqCenter[ch] + 0.001));
      }
      else {
	sprintf(str, "%.1fkHz", setting.freqCenter[ch] / 1000 + 0.001);
      }
      drawText(LCD_WIDTH/2-20, 1, str, CYAN);
      break;
    case 1: // gain
      sprintf(str, "%.1fdB", setting.gain[ch] + 0.001);
      drawText(LCD_WIDTH/2-20, 1, str, CYAN);
      break;
    case 2: // bandwidth
      sprintf(str, "%.2foct", setting.bandWidth[ch] + 0.001);
      drawText(LCD_WIDTH/2-20, 1, str, CYAN);
      break;
    }
  }
  else {
    drawText(LCD_WIDTH/2-60, LCD_HEIGHT - 15, modeStr[mode % 3 + 3], GREEN);
    if(mode == MODE_TOTALGAIN) {
      sprintf(str, "%.1fdB", setting.totalGain + 0.001);
      drawText(LCD_WIDTH/2-20, 1, str, CYAN);
    }
    else if(mode == MODE_SETTING) {
      sprintf(str, "%.1fkHz", audio_state.freq / 1000.0);
      drawText(LCD_WIDTH/2-20, 1, str, WHITE);
    }
  }
#endif
}  

static void drawResponse(void) {
  double g;
  Complex r;

  drawAxis();
  
  for(int x = MARGIN_LEFT; x < LCD_WIDTH; x++) {
    double freq = X_TO_FREQ(x);

    r.re = 1;
    r.im = 0;
    for(int i = 0; i < EQ_CH; i++) {
      r = mul(r, calcResponse(coeff[i], freq, (double)currentFreq));
    }
    g = 10 * log10(r.re * r.re + r.im * r.im);
    g += setting.totalGain;
    int y = GAIN_TO_Y(g) + 0.5;
      
    drawPoint(x, y, sw ? RED : BLUE);
  }
}

double freqStep(double f) {
  if(f < 100) {
    return 1.0;
  }
  else if(f < 300) {
    return 10.0;
  }
  else if(f < 1000) {
    return 20.0;
  }
  else if(f < 3000) {
    return 100.0;
  }
  else if(f < 10000) {
    return 200.0;
  }
  return 1000.0;
}

static void sense(void) {
  static int repeatCount = 0;
  int64_t curTime;
  
  int lt = !gpio_get(LCD_KEY_LT);
  int lb = !gpio_get(LCD_KEY_LB);
  int rt = !gpio_get(LCD_KEY_RT);
  int rb = !gpio_get(LCD_KEY_RB);
  
  if((lt || lb || rt || rb) == false) { // key release
    repeatCount = 0;
    if(lcdStatus && time_us_64() > keyOnTime + LCDTIME) {
      // LCD on timer is over
      gpio_put(LCD_BACKLIGHT_PIN, false);
      lcdStatus = false;
    }
    return;
  }

  // key press
  if(!lcdStatus) { //when blackout
    lcdOn();
    return;
  }

  curTime = time_us_64();
  if(repeatCount == 0) {
    if(curTime < keyOnTime + CHATTER_WAIT) { // avoid chattering
      return;
    }
  }
  else if(repeatCount == 1) {
    if(curTime < keyOnTime + FIRST_CLICK_WAIT) {
      return;
    }
  }
  repeatCount++;
  
  if(lt) {
    sw = !sw; //toggle
  }
  else if(lb) {
    mode = (mode + 1) % MODENUM;
  }
  else if(rt) {
    if(mode == MODE_TOTALGAIN) {
      setting.totalGain += GAIN_STEP;
    }
    else if(mode == MODE_SETTING) {
      save_setting_to_flash(); // save data (auto reset)
    }
    else {
      int ch = mode / 3;
      switch(mode % 3) {
      case 0:
	setting.freqCenter[ch] += freqStep(setting.freqCenter[ch]);
	break;
      case 1:
	setting.gain[ch] += GAIN_STEP;
	break;
      case  2:
	setting.bandWidth[ch] *= BW_STEP;
	break;
      }
    }
  }
  else if(rb) {
    if(mode == MODE_TOTALGAIN) {
      setting.totalGain -= GAIN_STEP;
    }
    else if(mode == MODE_SETTING) {
      // restore original setting
      initSettings();
    }
    else {
      int ch = mode / 3;
      switch(mode % 3) {
      case 0:
	setting.freqCenter[ch] -= freqStep(setting.freqCenter[ch] - 1);
	break;
      case 1:
	setting.gain[ch] -= GAIN_STEP;
	break;
      case 2:
	setting.bandWidth[ch] /= BW_STEP;
	break;
      }
    }
  }
  calcBinCoeffs();
  drawResponse();
  sendImage();
  lcdOn();
}


//////////////////////// filtering mod end

// todo forget why this is using core 1 for sound: presumably not necessary
// todo noop when muted

CU_REGISTER_DEBUG_PINS(audio_timing)

// ---- select at most one ---
//CU_SELECT_DEBUG_PINS(audio_timing)

// todo make descriptor strings should probably belong to the configs
     static char *descriptor_strings[] =
  {
    "Raspberry Pi",
    "Pico Sound Card with EQ",
    "0123456789AB"
  };

// todo fix these
#define VENDOR_ID   0x2e8au
#define PRODUCT_ID  0xfeddu

#define AUDIO_OUT_ENDPOINT  0x01U
#define AUDIO_IN_ENDPOINT   0x82U

#undef AUDIO_SAMPLE_FREQ
#define AUDIO_SAMPLE_FREQ(frq) (uint8_t)(frq), (uint8_t)((frq >> 8)), (uint8_t)((frq >> 16))

// if USB-audio rate feedback is applied, the packet size will increase occasionally
//#define AUDIO_MAX_PACKET_SIZE(freq) (uint8_t)(((freq + 999) / 1000) * 4)
#define AUDIO_MAX_PACKET_SIZE(freq) (uint8_t)(((freq + 1999) / 1000) * 4) // for rate control

#define FEATURE_MUTE_CONTROL 1u
#define FEATURE_VOLUME_CONTROL 2u

#define ENDPOINT_FREQ_CONTROL 1u

struct audio_device_config {
  struct usb_configuration_descriptor descriptor;
  struct usb_interface_descriptor ac_interface;
  struct __packed {
    USB_Audio_StdDescriptor_Interface_AC_t core;
    USB_Audio_StdDescriptor_InputTerminal_t input_terminal;
    USB_Audio_StdDescriptor_FeatureUnit_t feature_unit;
    USB_Audio_StdDescriptor_OutputTerminal_t output_terminal;
  } ac_audio;
  struct usb_interface_descriptor as_zero_interface;
  struct usb_interface_descriptor as_op_interface;
  struct __packed {
    USB_Audio_StdDescriptor_Interface_AS_t streaming;
    struct __packed {
      USB_Audio_StdDescriptor_Format_t core;
      USB_Audio_SampleFreq_t freqs[2];
    } format;
  } as_audio;
  struct __packed {
    struct usb_endpoint_descriptor_long core;
    USB_Audio_StdDescriptor_StreamEndpoint_Spc_t audio;
  } ep1;
  struct usb_endpoint_descriptor_long ep2;
};

static const struct audio_device_config audio_device_config = {
  .descriptor = {
    .bLength             = sizeof(audio_device_config.descriptor),
    .bDescriptorType     = DTYPE_Configuration,
    .wTotalLength        = sizeof(audio_device_config),
    .bNumInterfaces      = 2,
    .bConfigurationValue = 0x01,
    .iConfiguration      = 0x00,
    .bmAttributes        = 0x80,
    .bMaxPower           = 0x32,
  },
  .ac_interface = {
    .bLength            = sizeof(audio_device_config.ac_interface),
    .bDescriptorType    = DTYPE_Interface,
    .bInterfaceNumber   = 0x00,
    .bAlternateSetting  = 0x00,
    .bNumEndpoints      = 0x00,
    .bInterfaceClass    = AUDIO_CSCP_AudioClass,
    .bInterfaceSubClass = AUDIO_CSCP_ControlSubclass,
    .bInterfaceProtocol = AUDIO_CSCP_ControlProtocol,
    .iInterface         = 0x00,
  },
  .ac_audio = {
    .core = {
      .bLength = sizeof(audio_device_config.ac_audio.core),
      .bDescriptorType = AUDIO_DTYPE_CSInterface,
      .bDescriptorSubtype = AUDIO_DSUBTYPE_CSInterface_Header,
      .bcdADC = VERSION_BCD(1, 0, 0),
      .wTotalLength = sizeof(audio_device_config.ac_audio),
      .bInCollection = 1,
      .bInterfaceNumbers = 1,
    },
    .input_terminal = {
      .bLength = sizeof(audio_device_config.ac_audio.input_terminal),
      .bDescriptorType = AUDIO_DTYPE_CSInterface,
      .bDescriptorSubtype = AUDIO_DSUBTYPE_CSInterface_InputTerminal,
      .bTerminalID = 1,
      .wTerminalType = AUDIO_TERMINAL_STREAMING,
      .bAssocTerminal = 0,
      .bNrChannels = 2,
      .wChannelConfig = AUDIO_CHANNEL_LEFT_FRONT | AUDIO_CHANNEL_RIGHT_FRONT,
      .iChannelNames = 0,
      .iTerminal = 0,
    },
    .feature_unit = {
      .bLength = sizeof(audio_device_config.ac_audio.feature_unit),
      .bDescriptorType = AUDIO_DTYPE_CSInterface,
      .bDescriptorSubtype = AUDIO_DSUBTYPE_CSInterface_Feature,
      .bUnitID = 2,
      .bSourceID = 1,
      .bControlSize = 1,
      .bmaControls = {AUDIO_FEATURE_MUTE | AUDIO_FEATURE_VOLUME, 0, 0},
      .iFeature = 0,
    },
    .output_terminal = {
      .bLength = sizeof(audio_device_config.ac_audio.output_terminal),
      .bDescriptorType = AUDIO_DTYPE_CSInterface,
      .bDescriptorSubtype = AUDIO_DSUBTYPE_CSInterface_OutputTerminal,
      .bTerminalID = 3,
      .wTerminalType = AUDIO_TERMINAL_OUT_SPEAKER,
      .bAssocTerminal = 0,
      .bSourceID = 2,
      .iTerminal = 0,
    },
  },
  .as_zero_interface = {
    .bLength            = sizeof(audio_device_config.as_zero_interface),
    .bDescriptorType    = DTYPE_Interface,
    .bInterfaceNumber   = 0x01,
    .bAlternateSetting  = 0x00,
    .bNumEndpoints      = 0x00,
    .bInterfaceClass    = AUDIO_CSCP_AudioClass,
    .bInterfaceSubClass = AUDIO_CSCP_AudioStreamingSubclass,
    .bInterfaceProtocol = AUDIO_CSCP_ControlProtocol,
    .iInterface         = 0x00,
  },
  .as_op_interface = {
    .bLength            = sizeof(audio_device_config.as_op_interface),
    .bDescriptorType    = DTYPE_Interface,
    .bInterfaceNumber   = 0x01,
    .bAlternateSetting  = 0x01,
    .bNumEndpoints      = 0x02,
    .bInterfaceClass    = AUDIO_CSCP_AudioClass,
    .bInterfaceSubClass = AUDIO_CSCP_AudioStreamingSubclass,
    .bInterfaceProtocol = AUDIO_CSCP_ControlProtocol,
    .iInterface         = 0x00,
  },
  .as_audio = {
    .streaming = {
      .bLength = sizeof(audio_device_config.as_audio.streaming),
      .bDescriptorType = AUDIO_DTYPE_CSInterface,
      .bDescriptorSubtype = AUDIO_DSUBTYPE_CSInterface_General,
      .bTerminalLink = 1,
      .bDelay = 1,
      .wFormatTag = 1, // PCM
    },
    .format = {
      .core = {
	.bLength = sizeof(audio_device_config.as_audio.format),
	.bDescriptorType = AUDIO_DTYPE_CSInterface,
	.bDescriptorSubtype = AUDIO_DSUBTYPE_CSInterface_FormatType,
	.bFormatType = 1,
	.bNrChannels = 2,
	.bSubFrameSize = 2,
	.bBitResolution = 16,
	.bSampleFrequencyType = count_of(audio_device_config.as_audio.format.freqs),
      },
      .freqs = {
	AUDIO_SAMPLE_FREQ(44100),
	AUDIO_SAMPLE_FREQ(48000)
      },
    },
  },
  .ep1 = {
    .core = {
      .bLength          = sizeof(audio_device_config.ep1.core),
      .bDescriptorType  = DTYPE_Endpoint,
      .bEndpointAddress = AUDIO_OUT_ENDPOINT,
      .bmAttributes     = 5,
      .wMaxPacketSize   = AUDIO_MAX_PACKET_SIZE(AUDIO_FREQ_MAX),
      .bInterval        = 1,
      .bRefresh         = 0,
      .bSyncAddr        = AUDIO_IN_ENDPOINT,
    },
    .audio = {
      .bLength = sizeof(audio_device_config.ep1.audio),
      .bDescriptorType = AUDIO_DTYPE_CSEndpoint,
      .bDescriptorSubtype = AUDIO_DSUBTYPE_CSEndpoint_General,
      .bmAttributes = 1,
      .bLockDelayUnits = 0,
      .wLockDelay = 0,
    }
  },
  .ep2 = {
    .bLength          = sizeof(audio_device_config.ep2),
    .bDescriptorType  = 0x05,
    .bEndpointAddress = AUDIO_IN_ENDPOINT,
    .bmAttributes     = 0x11,
    .wMaxPacketSize   = 3,
    .bInterval        = 0x01,
    .bRefresh         = 2,
    .bSyncAddr        = 0,
  },
};

static struct usb_interface ac_interface;
static struct usb_interface as_op_interface;
static struct usb_endpoint ep_op_out, ep_op_sync;

static const struct usb_device_descriptor boot_device_descriptor = {
  .bLength            = 18,
  .bDescriptorType    = 0x01,
  .bcdUSB             = 0x0110,
  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
  .bMaxPacketSize0    = 0x40,
  .idVendor           = VENDOR_ID,
  .idProduct          = PRODUCT_ID,
  .bcdDevice          = 0x0200,
  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,
  .bNumConfigurations = 0x01,
};

const char *_get_descriptor_string(uint index) {
  if (index <= count_of(descriptor_strings)) {
    return descriptor_strings[index - 1];
  } else {
    return "";
  }
}

static struct audio_buffer_pool *producer_pool;

//for USB-audio rate feedback, number of vacant buffers is used
static int countFreeBuffers(void) {
  int i = 0;
  
  audio_buffer_t *audio_buffer = producer_pool->free_list;
  while(audio_buffer != NULL) {
    audio_buffer = audio_buffer->next;
    i++;
  }
  return i;
}

#define FADE_SPEED1 200
#define FADE_SPEED2 80

static void _as_audio_packet(struct usb_endpoint *ep) {
  static int32_t vol2 = 0;

  struct usb_buffer *usb_buffer = usb_current_out_packet_buffer(ep);
  struct audio_buffer *audio_buffer = take_audio_buffer(producer_pool, true);
  audio_buffer->sample_count = usb_buffer->data_len / 4;
  int32_t vol_mul = audio_state.vol_mul;

  int16_t *out = (int16_t *) audio_buffer->buffer->bytes;
  int16_t *in = (int16_t *) usb_buffer->data;

  // add mute function with gentle fading
  if(audio_state.mute) {
    vol_mul = 0;
  }
    if(vol2 != vol_mul) {
    int32_t diff = (vol_mul - vol2) / FADE_SPEED1;
    if      (diff >  FADE_SPEED2) diff =  FADE_SPEED2;
    else if (diff < -FADE_SPEED2) diff = -FADE_SPEED2;
    vol2 += diff;
    if     (vol2 < vol_mul) vol2++;
    else if(vol2 > vol_mul) vol2--;
  }

  for (int i = 0; i < audio_buffer->sample_count * 2; i+=2) {
    out[i  ] = (filterR(in[i  ]) * vol2) >> 16u;
    out[i+1] = (filterL(in[i+1]) * vol2) >> 16u;
  }

  give_audio_buffer(producer_pool, audio_buffer);

  // keep on truckin'
  usb_grow_transfer(ep->current_transfer, 1);
  usb_packet_done(ep);
}

static void _as_sync_packet(struct usb_endpoint *ep) {
  assert(ep->current_transfer);
  DEBUG_PINS_SET(audio_timing, 2);
  DEBUG_PINS_CLR(audio_timing, 2);
  struct usb_buffer *buffer = usb_current_in_packet_buffer(ep);
  assert(buffer->data_max >= 3);
  buffer->data_len = 3;

  int freeBuf = countFreeBuffers();
  int feedbackvalue = (freeBuf - BUFFER_NUM / 2) * 5;
  
  //uint feedback = (audio_state.freq << 14u) / 1000u;
  uint feedback = ((audio_state.freq + feedbackvalue) << 14u) / 1000u;

  buffer->data[0] = feedback;
  buffer->data[1] = feedback >> 8u;
  buffer->data[2] = feedback >> 16u;

  // keep on truckin'
  usb_grow_transfer(ep->current_transfer, 1);
  usb_packet_done(ep);
}

static const struct usb_transfer_type as_transfer_type = {
  .on_packet = _as_audio_packet,
  .initial_packet_count = 1,
};

static const struct usb_transfer_type as_sync_transfer_type = {
  .on_packet = _as_sync_packet,
  .initial_packet_count = 1,
};

static struct usb_transfer as_transfer;
static struct usb_transfer as_sync_transfer;

static bool do_get_current(struct usb_setup_packet *setup) {
  usb_debug("AUDIO_REQ_GET_CUR\n");

  if ((setup->bmRequestType & USB_REQ_TYPE_RECIPIENT_MASK) == USB_REQ_TYPE_RECIPIENT_INTERFACE) {
    switch (setup->wValue >> 8u) {
    case FEATURE_MUTE_CONTROL: {
      usb_start_tiny_control_in_transfer(audio_state.mute, 1);
      return true;
    }
    case FEATURE_VOLUME_CONTROL: {
      /* Current volume. See UAC Spec 1.0 p.77 */
      usb_start_tiny_control_in_transfer(audio_state.volume, 2);
      return true;
    }
    }
  } else if ((setup->bmRequestType & USB_REQ_TYPE_RECIPIENT_MASK) == USB_REQ_TYPE_RECIPIENT_ENDPOINT) {
    if ((setup->wValue >> 8u) == ENDPOINT_FREQ_CONTROL) {
      /* Current frequency */
      usb_start_tiny_control_in_transfer(audio_state.freq, 3);
      return true;
    }
  }
  return false;
}

#define ENCODE_DB(x) ((uint16_t)(int16_t)((x)*256))

#define MIN_VOLUME           ENCODE_DB(-100.0)
#define DEFAULT_VOLUME       ENCODE_DB(0)
#define MAX_VOLUME           ENCODE_DB(0)
#define VOLUME_RESOLUTION    ENCODE_DB(1)

static bool do_get_minimum(struct usb_setup_packet *setup) {
  usb_debug("AUDIO_REQ_GET_MIN\n");
  if ((setup->bmRequestType & USB_REQ_TYPE_RECIPIENT_MASK) == USB_REQ_TYPE_RECIPIENT_INTERFACE) {
    switch (setup->wValue >> 8u) {
    case FEATURE_VOLUME_CONTROL: {
      usb_start_tiny_control_in_transfer(MIN_VOLUME, 2);
      return true;
    }
    }
  }
  return false;
}

static bool do_get_maximum(struct usb_setup_packet *setup) {
  usb_debug("AUDIO_REQ_GET_MAX\n");
  if ((setup->bmRequestType & USB_REQ_TYPE_RECIPIENT_MASK) == USB_REQ_TYPE_RECIPIENT_INTERFACE) {
    switch (setup->wValue >> 8u) {
    case FEATURE_VOLUME_CONTROL: {
      usb_start_tiny_control_in_transfer(MAX_VOLUME, 2);
      return true;
    }
    }
  }
  return false;
}

static bool do_get_resolution(struct usb_setup_packet *setup) {
  usb_debug("AUDIO_REQ_GET_RES\n");
  if ((setup->bmRequestType & USB_REQ_TYPE_RECIPIENT_MASK) == USB_REQ_TYPE_RECIPIENT_INTERFACE) {
    switch (setup->wValue >> 8u) {
    case FEATURE_VOLUME_CONTROL: {
      usb_start_tiny_control_in_transfer(VOLUME_RESOLUTION, 2);
      return true;
    }
    }
  }
  return false;
}

static struct audio_control_cmd {
  uint8_t cmd;
  uint8_t type;
  uint8_t cs;
  uint8_t cn;
  uint8_t unit;
  uint8_t len;
} audio_control_cmd_t;

static void _audio_reconfigure() {
  switch (audio_state.freq) {
  case 44100:
  case 48000:
    break;
  default:
    audio_state.freq = 44100;
  }
  // todo hack overwriting const
  ((struct audio_format *) producer_pool->format)->sample_freq = audio_state.freq;
}

static void audio_set_volume(int16_t volume) {
  if(volume > MAX_VOLUME) { // comm. error
    return;
  }
  audio_state.volume = volume;
  double vol = exp(volume / 2560.0); // 16q => dB => linear
  if(vol > 1.0) vol = 1.0;
  if(vol < 0.0) vol = 0.0;
  audio_state.vol_mul = (int32_t)(vol * (1 << 16));
}

static void audio_cmd_packet(struct usb_endpoint *ep) {
  assert(audio_control_cmd_t.cmd == AUDIO_REQ_SetCurrent);
  struct usb_buffer *buffer = usb_current_out_packet_buffer(ep);
  audio_control_cmd_t.cmd = 0;
  if (buffer->data_len >= audio_control_cmd_t.len) {
    if (audio_control_cmd_t.type == USB_REQ_TYPE_RECIPIENT_INTERFACE) {
      switch (audio_control_cmd_t.cs) {
      case FEATURE_MUTE_CONTROL: {
	audio_state.mute = buffer->data[0];
	usb_warn("Set Mute %d\n", buffer->data[0]);
	break;
      }
      case FEATURE_VOLUME_CONTROL: {
	audio_set_volume(*(int16_t *) buffer->data);
	break;
      }
      }

    } else if (audio_control_cmd_t.type == USB_REQ_TYPE_RECIPIENT_ENDPOINT) {
      if (audio_control_cmd_t.cs == ENDPOINT_FREQ_CONTROL) {
	uint32_t new_freq = (*(uint32_t *) buffer->data) & 0x00ffffffu;
	usb_warn("Set freq %d\n", new_freq == 0xffffffu ? -1 : (int) new_freq);

	if (audio_state.freq != new_freq) {
	  audio_state.freq = new_freq;
	  _audio_reconfigure();
	}
      }
    }
  }
  usb_start_empty_control_in_transfer_null_completion();
  // todo is there error handling?
}


static const struct usb_transfer_type _audio_cmd_transfer_type = {
  .on_packet = audio_cmd_packet,
  .initial_packet_count = 1,
};

static bool as_set_alternate(struct usb_interface *interface, uint alt) {
  assert(interface == &as_op_interface);
  usb_warn("SET ALTERNATE %d\n", alt);
  return alt < 2;
}

static bool do_set_current(struct usb_setup_packet *setup) {
#ifndef NDEBUG
  usb_warn("AUDIO_REQ_SET_CUR\n");
#endif

  if (setup->wLength && setup->wLength < 64) {
    audio_control_cmd_t.cmd = AUDIO_REQ_SetCurrent;
    audio_control_cmd_t.type = setup->bmRequestType & USB_REQ_TYPE_RECIPIENT_MASK;
    audio_control_cmd_t.len = (uint8_t) setup->wLength;
    audio_control_cmd_t.unit = setup->wIndex >> 8u;
    audio_control_cmd_t.cs = setup->wValue >> 8u;
    audio_control_cmd_t.cn = (uint8_t) setup->wValue;
    usb_start_control_out_transfer(&_audio_cmd_transfer_type);
    return true;
  }
  return false;
}

static bool ac_setup_request_handler(__unused struct usb_interface *interface, struct usb_setup_packet *setup) {
  setup = __builtin_assume_aligned(setup, 4);
  if (USB_REQ_TYPE_TYPE_CLASS == (setup->bmRequestType & USB_REQ_TYPE_TYPE_MASK)) {
    switch (setup->bRequest) {
    case AUDIO_REQ_SetCurrent:
      return do_set_current(setup);

    case AUDIO_REQ_GetCurrent:
      return do_get_current(setup);

    case AUDIO_REQ_GetMinimum:
      return do_get_minimum(setup);

    case AUDIO_REQ_GetMaximum:
      return do_get_maximum(setup);

    case AUDIO_REQ_GetResolution:
      return do_get_resolution(setup);

    default:
      break;
    }
  }
  return false;
}

bool _as_setup_request_handler(__unused struct usb_endpoint *ep, struct usb_setup_packet *setup) {
  setup = __builtin_assume_aligned(setup, 4);
  if (USB_REQ_TYPE_TYPE_CLASS == (setup->bmRequestType & USB_REQ_TYPE_TYPE_MASK)) {
    switch (setup->bRequest) {
    case AUDIO_REQ_SetCurrent:
      return do_set_current(setup);

    case AUDIO_REQ_GetCurrent:
      return do_get_current(setup);

    case AUDIO_REQ_GetMinimum:
      return do_get_minimum(setup);

    case AUDIO_REQ_GetMaximum:
      return do_get_maximum(setup);

    case AUDIO_REQ_GetResolution:
      return do_get_resolution(setup);

    default:
      break;
    }
  }
  return false;
}

void usb_sound_card_init() {
  //msd_interface.setup_request_handler = msd_setup_request_handler;
  usb_interface_init(&ac_interface, &audio_device_config.ac_interface, NULL, 0, true);
  ac_interface.setup_request_handler = ac_setup_request_handler;

  static struct usb_endpoint *const op_endpoints[] = {
    &ep_op_out, &ep_op_sync
  };
  usb_interface_init(&as_op_interface, &audio_device_config.as_op_interface, op_endpoints, count_of(op_endpoints),
		     true);
  as_op_interface.set_alternate_handler = as_set_alternate;
  ep_op_out.setup_request_handler = _as_setup_request_handler;
  as_transfer.type = &as_transfer_type;
  usb_set_default_transfer(&ep_op_out, &as_transfer);
  as_sync_transfer.type = &as_sync_transfer_type;
  usb_set_default_transfer(&ep_op_sync, &as_sync_transfer);

  static struct usb_interface *const boot_device_interfaces[] = {
    &ac_interface,
    &as_op_interface,
  };
  __unused struct usb_device *device = usb_device_init(&boot_device_descriptor, &audio_device_config.descriptor,
						       boot_device_interfaces, count_of(boot_device_interfaces),
						       _get_descriptor_string);
  assert(device);
  audio_set_volume(DEFAULT_VOLUME);
  _audio_reconfigure();
  //    device->on_configure = _on_configure;
  usb_device_start();
}

static void core1_worker() {
  audio_i2s_set_enabled(true);
}

int main(void) {
  //  set_sys_clock_48mhz();
    
  stdout_uart_init();

  //gpio_debug_pins_init();
  //    puts("USB SOUND CARD");

#ifndef NDEBUG
  for(uint i=0;i<count_of(audio_device_config.as_audio.format.freqs);i++) {
    uint freq = audio_device_config.as_audio.format.freqs[i].Byte1 |
      (audio_device_config.as_audio.format.freqs[i].Byte2 << 8u) |
      (audio_device_config.as_audio.format.freqs[i].Byte3 << 16u);
    assert(freq <= AUDIO_FREQ_MAX);
  }
#endif

    
  // initialize for 48k we allow changing later
  struct audio_format audio_format_48k = {
    .format = AUDIO_BUFFER_FORMAT_PCM_S16,
    .sample_freq = 48000,
    .channel_count = 2,
  };

  struct audio_buffer_format producer_format = {
    .format = &audio_format_48k,
    .sample_stride = 4
  };

  //  producer_pool = audio_new_producer_pool(&producer_format, 8, 48); // todo correct size
  producer_pool = audio_new_producer_pool(&producer_format, BUFFER_NUM,
					  AUDIO_MAX_PACKET_SIZE(AUDIO_FREQ_MAX) / 4);
  bool __unused ok;
  struct audio_i2s_config config = {
    .data_pin = PICO_AUDIO_I2S_DATA_PIN,
    .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
    .dma_channel = 0,
    .pio_sm = 0,
  };

  const struct audio_format *output_format;
  output_format = audio_i2s_setup(&audio_format_48k, &config);
  if (!output_format) {
    panic("PicoAudio: Unable to open audio device.\n");
  }

  myInit();

  // It is not clear what is meant by the number "96" but I fixed it as well as producer_pool.
  // ok = audio_i2s_connect_extra(producer_pool, false, 2, 96, NULL);
  ok = audio_i2s_connect_extra(producer_pool, false, 2,
  			       AUDIO_MAX_PACKET_SIZE(AUDIO_FREQ_MAX) / 2, NULL);
  assert(ok);

  usb_sound_card_init();
    
  multicore_launch_core1(core1_worker);
  //  printf("HAHA %04x %04x %04x %04x\n", MIN_VOLUME, DEFAULT_VOLUME, MAX_VOLUME, VOLUME_RESOLUTION);
  // MSD is irq driven
  while (1) {
    sense();
    if(audio_state.freq != currentFreq) { // freq changes
      currentFreq = audio_state.freq;
      calcBinCoeffs();
      mode = MODE_SETTING;
      drawResponse();
      sendImage();
      lcdOn();
    }
#if DEBUG_FEEDBACK
    numFreeBuf = countFreeBuffers();
    drawResponse();
    sendImage();
    lcdOn();
#endif
    //        __wfi();
  }
}

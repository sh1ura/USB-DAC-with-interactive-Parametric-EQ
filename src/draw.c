#include <stdio.h>
#include "eq.h"
#include "draw.h"

uint16_t image[LCD_HEIGHT][LCD_WIDTH];

#define COLCONV(c) ((c) >> 8 | ((c) & 0xff) << 8)

void clearImage(uint16_t c) {
  int x, y;
  uint16_t col = COLCONV(c);

  for(y = 0; y < LCD_HEIGHT; y++){
    for(x = 0; x < LCD_WIDTH; x++){
      image[y][x] = col;
    }
  }
}

void drawPoint(int x, int y, uint16_t c) {
  if(0 <= x && x < LCD_WIDTH && 0 <= y && y < LCD_HEIGHT) {
    image[y][x] = COLCONV(c);
  }
}

void sendImage(void) {
  LCD_2IN_SetWindows(0, 0, LCD_WIDTH, LCD_HEIGHT); // xstart, ystart, xend, yend
  DEV_Digital_Write(LCD_DC_PIN, 1);
  DEV_Digital_Write(LCD_CS_PIN, 0);
  DEV_SPI_Write_nByte(LCD_SPI_PORT, (uint8_t *)image, LCD_HEIGHT * LCD_WIDTH * 2);
  DEV_Digital_Write(LCD_CS_PIN, 1);
}

#include "font.c"
char table[] = "./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^-";

void drawFont(int x, int y, char c, uint16_t col) {
  int i, j;

  for(i = 0; ; i++) {
    if(table[i] == '\0') {
      return;
    }
    if(table[i] == c) {
      c = i;
      break;
    }
  }

  for(j = 0; j < 14; j++) {
    unsigned int f = vcr_font[c * 14 + j];
    for(i = 11; i >= 0; i--) {
      if(f & 1) {
	drawPoint(x + i, y + j, col);
      }
      f >>= 1;
    }
  }
}

void drawText(int x, int y, char *s, uint16_t col) {
  while(*s != '\0') {
    drawFont(x, y, *s, col);
    x += 12;
    s++;
  }
}

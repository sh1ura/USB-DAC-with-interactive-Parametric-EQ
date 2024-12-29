#include "LCD_2in.h"

#define LCD_WIDTH  LCD_2IN_HEIGHT
#define LCD_HEIGHT LCD_2IN_WIDTH

extern uint16_t image[LCD_HEIGHT][LCD_WIDTH];

void clearImage(uint16_t c);
void sendImage(void);
void drawPoint(int x, int y, uint16_t c);
void drawFont(int x, int y, char c, uint16_t col);
void drawText(int x, int y, char *s, uint16_t col);

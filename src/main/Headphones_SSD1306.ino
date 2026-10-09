#include "Headphones_SSD1306.h"

void Headphones_SSD1306::drawScrollingText(char text[], int16_t *x, int8_t start, uint8_t flags, 
                      uint8_t scroll_speed, uint32_t *nextTime) 
{
  minX = -12 * strlen(text); //12 = 6 pixels/character * text size 2
  setTextSize(2);
  setCursor(*x, start);
  print(text);

  if ( !(flags == T_WAIT && millis() < *nextTime && *x == 0)) {
    *x = *x - scroll_speed;
  }

  if (*x < minX) {
    *x = width();
    //setNextTime(WAIT_TIME_MILLIS);
  } else if (*x == 0 && millis() >= *nextTime) {
    *nextTime = millis() + WAIT_TIME_MILLIS;
  }
}
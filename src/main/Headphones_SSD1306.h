#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#ifndef HEADPHONES_SSD1306_
#define HEADPHONES_SSD1306_

#define SCR_W       128
#define SCR_H       64
#define OLED_RESET  -1
#define SCR_ADDR    0x3C
#define SCR_TIME    33

// Temporarily stop scrolling when the text reaches the end.
#define T_WAIT      0x01
#define WAIT_TIME_MILLIS  1000

class Headphones_SSD1306 : public Adafruit_SSD1306
{
  using Adafruit_SSD1306::Adafruit_SSD1306;
  public:
    void drawScrollingText(char text[], int16_t *x, int8_t start, uint8_t flags, 
                          uint8_t scroll_speed, uint32_t *nextTime);
};

#endif
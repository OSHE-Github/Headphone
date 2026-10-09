#include "Wire.h"
#include "AudioTools.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Headphones_SSD1306.h>
#include <Arduino.h>

#define I2C_SDA   21
#define I2C_SCL   22
#define I2C_DEV_ADDR 0x55

Headphones_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);
hw_timer_t *timer = NULL; // Hardware timer to update screen

int16_t tcur = 0;, acur = 0, alcur = 0;
uint32_t startTime, ttime, atime, altime;

// Audio has not yet been tested
#define I2S_BCK   27
#define I2S_WS    25
#define I2S_DATA  26

#define AUDIO_CHANNELS    2
#define AUDIO_SAMPLE_RATE 44100
#define BITS_PER_SAMPLE   16

AudioInfo info(AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, BITS_PER_SAMPLE);
SineGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
I2SStream out;
StreamCopy copier(out, tone);

void IRAM_ATTR updateScreen();

void setup() 
{
  Serial.begin(9600);
  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    while(1);
  }

  display.clearDisplay();

  // Set the start time for each scrolling line to the startTime
  startTime = millis();
  ttime = startTime;
  atime = startTime;
  altime = startTime;

  /*
   * Initialize the timer that will update the screen every 33ms (30Hz).
   * This allows the I2S system to run at its own speed and not worry about the screen.
  */
  scrtimer = timerBegin(0, 80, true);
  timerAttachInterrupt(scrtimer, &updateScreen, true);
  timerAlarmWrite(scrtimer, 33333, true);
  timerAlarmEnable(scrtimer);

  // Configure the I2S system
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.pin_bck = I2S_BCK;
  config.pin_ws = I2S_WS;
  config.pin_data = I2S_DATA;
  out.begin(config);

  sineWave.begin(info, N_B4);
}

void loop() 
{
  copier.copy();
}

void IRAM_ATTR updateScreen() {
  display.clearDisplay();


  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(2);
  display.setCursor(0, 0);

  display.println("OSHE");
  display.drawScrollingText("Title of the song", &tcur, 16, T_WAIT, 1, &ttime);
  display.drawScrollingText("Artist!!", &acur, 32, T_WAIT, 4, &atime);
  display.drawScrollingText("Album...", &alcur, 48, T_WAIT, 2, &altime);

  display.display();
}
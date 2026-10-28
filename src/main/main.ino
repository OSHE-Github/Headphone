#include "Wire.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Disk/AudioSourceSD.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Headphones_SSD1306.h>
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>

// OLED screen I2C pins
#define I2C_SDA   21
#define I2C_SCL   22
#define I2C_DEV_ADDR 0x55

// SD card SPI pins
#define SD_CS     13
#define SD_SCK    18
#define SD_MISO   19
#define SD_MOSI   23

Headphones_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);
//hw_timer_t *timer = NULL; // Hardware timer to update screen

int16_t tcur = 0, acur = 0, alcur = 0;
uint32_t startTime, ttime, atime, altime, lastscrtime;

// PCM5102A DAC I2S pins
#define I2S_BCK   27
#define I2S_WS    25
#define I2S_DATA  26

#define AUDIO_CHANNELS    2
#define AUDIO_SAMPLE_RATE 44100
#define BITS_PER_SAMPLE   16


AudioInfo info(AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, BITS_PER_SAMPLE);

File audioFile;
MetaDataOutput outMeta;
I2SStream i2s;
EncodedAudioStream out2dec(&i2s, new MP3DecoderHelix());
MultiOutput out;
StreamCopy copier(out, audioFile);

char* mp3title;

char* metadatalist[20];


//AudioSourceSD source("/", ".mp3", SD_CS);
//MP3DecoderHelix decoder;
//AudioPlayer player(source, out, decoder);


//SineWaveGenerator<int16_t> sineWave(32000);
//GeneratedSoundStream<int16_t> sound(sineWave);

void updateScreen();

uint16_t idx = 0;
void printMetaData(MetaDataType type, const char* str, int len) {
  Serial.println(F(str));
  metadatalist[idx] = (char*) str;
  idx++;
  if (idx == 20) {
    idx = 0;
  }
  mp3title = metadatalist[0];
}

String findFirstMP3() {
    File root = SD.open("/");
    if (!root) {
        Serial.println("Failed to open SD root directory");
        return "";
    }

    File entry;
    while ((entry = root.openNextFile())) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            name.toLowerCase();

            if (name.endsWith(".mp3")) {
                String path = "/";
                path += entry.name();

                entry.close();
                root.close();

                return path;
            }
        }

        entry.close();
    }

    root.close();
    return "";
}

void setup() 
{
  Serial.begin(9600);
  //AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  out.add(outMeta);
  out.add(out2dec);

  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    while(1);
  }

  display.clearDisplay();

  if (!SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS)) {
    Serial.println("SPI Failed");
    while (true) {
      delay(1000);
    }
  }

  delay(100);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD initialization FAILED!");
    while (true) {
      delay(1000);
    }
  }

  // Set the start time for each scrolling line to the startTime
  startTime = millis();
  ttime = startTime;
  atime = startTime;
  altime = startTime;
  lastscrtime = startTime;

  String mp3Path = findFirstMP3();

  if (mp3Path.isEmpty()) {
    Serial.println("No MP3 files found in root directory!");
    while (true) {
      delay(1000);
    }
  }

  audioFile = SD.open(mp3Path);

  outMeta.setCallback(printMetaData);
  outMeta.begin();

  // Configure the I2S system
  auto config = i2s.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.pin_bck = I2S_BCK;
  config.pin_ws = I2S_WS;
  config.pin_data = I2S_DATA;
  config.is_master = true;
  config.i2s_format = I2S_STD_FORMAT;
  i2s.begin(config);

  //out.setVolume(0.5);
  out2dec.begin();

  

  /*player.setVolume(0.5);
  player.setPath(mp3Path.c_str());
  Serial.println(F("Playing song name "));
  Serial.println(mp3Path.c_str());
  player.begin();*/
}

void loop() 
{
  uint32_t now = millis();
  if ((now - lastscrtime) >= 300) {
    lastscrtime = now;
    updateScreen();
  }
  
  copier.copy();
}

void updateScreen() {
  display.clearDisplay();
  

  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(2);
  display.setCursor(0, 0);

  display.println("WMTU");
  display.drawScrollingText(mp3title, &tcur, 16, T_WAIT, 1, &ttime);
  display.drawScrollingText("Artist!!", &acur, 32, T_WAIT, 4, &atime);
  display.drawScrollingText("Album...", &alcur, 48, T_WAIT, 2, &altime);

  display.display();
}
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

char* metadatalist[20];

void updateScreen();

uint16_t idx = 0;
struct TrackInfo {
  char title[128];
  char artist[128];
  char album[128];
  char path[128];
};

TrackInfo current;   // the only song you display

void printMetaData(MetaDataType type, const char* str, int len) {
  if (len <= 0) return;
  switch (type) {
    case Title:  snprintf(current.title,  sizeof(current.title),  "%.*s", len, str); break;
    case Artist: snprintf(current.artist, sizeof(current.artist), "%.*s", len, str); break;
    case Album:  snprintf(current.album,  sizeof(current.album),  "%.*s", len, str); break;
    default: break;
  }
}


void loadTrack(const String& path) {
  if (audioFile) audioFile.close();
  memset(&current, 0, sizeof(current));
  snprintf(current.path, sizeof(current.path), "%s", path.c_str());

  audioFile = SD.open(path);
  if (!audioFile) {
    Serial.printf("loadTrack: failed to open %s\n", path.c_str());
    return;
  }

  // fallback title = filename until ID3 arrives
  snprintf(current.title, sizeof(current.title), "%s", path.c_str() + 1);

  outMeta.end();  outMeta.begin();     // reset ID3 parser
  out2dec.end();  out2dec.begin();     // reset MP3 decoder
  copier.begin(out, audioFile);

  tcur = acur = alcur = 0;
  ttime = atime = altime = millis();
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

String findNextMP3(const String& currentPath) {
  Serial.printf("findNextMP3 looking for '%s' (len %d)\n",
                currentPath.c_str(), currentPath.length());

  File root = SD.open("/");
  if (!root) return "";

  bool foundCurrent = false;
  String first = "", next = "";
  File entry;

  while ((entry = root.openNextFile())) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      String lower = name;
      lower.toLowerCase();
      if (lower.endsWith(".mp3")) {
        String path = "/";
        path += name;
        Serial.printf("  candidate '%s' match=%d\n",
                      path.c_str(), (path == currentPath) ? 1 : 0);
        if (first.isEmpty()) first = path;        // remember the top of the list
        if (foundCurrent && next.isEmpty()) next = path;
        if (path == currentPath) foundCurrent = true;
      }
    }
    entry.close();
  }
  root.close();

  if (!next.isEmpty()) return next;
  return first;   // wrapped around — or empty if none found
}


void dumpDir() {
  File root = SD.open("/");
  if (!root) { Serial.println("root open failed"); return; }
  File e;
  int i = 0;
  while ((e = root.openNextFile())) {
    Serial.printf("[%d] dir=%d name='%s' len=%d\n",
                  i++, e.isDirectory(), e.name(), (int)strlen(e.name()));
    e.close();
  }
  root.close();
  Serial.println("--- end dir ---");
}

void setup()
{
  Serial.begin(115200);

  out.add(outMeta);
  out.add(out2dec);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }
  display.clearDisplay();

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(100);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD initialization FAILED!");
    while (true) delay(1000);
  }

  startTime = millis();
  ttime = atime = altime = lastscrtime = startTime;

  String mp3Path = findFirstMP3();
  if (mp3Path.isEmpty()) {
    Serial.println("No MP3 files found in root directory!");
    while (true) delay(1000);
  }

  dumpDir();   // debug; remove when done

  // I2S first
  auto config = i2s.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.pin_bck = I2S_BCK;
  config.pin_ws = I2S_WS;
  config.pin_data = I2S_DATA;
  config.is_master = true;
  config.i2s_format = I2S_STD_FORMAT;
  config.buffer_count = 16;
  config.buffer_size = 1024;
  i2s.begin(config);

  // then the track
  outMeta.setCallback(printMetaData);
  loadTrack(mp3Path);
}


bool needAdvance = false;

void loop() 
{
  // 1. handle a pending track change while nothing is mid-copy
  if (needAdvance) {
    needAdvance = false;
    String next = findNextMP3(current.path);
    if (next.isEmpty()) next = findFirstMP3();   // wrap
    if (!next.isEmpty()) loadTrack(next);
  }

  uint32_t now = millis();
  if ((now - lastscrtime) >= 300) {
    lastscrtime = now;
    updateScreen();
  }

  size_t n = copier.copy();

  // 2. only *flag* the end, don't act on it here
  if (n == 0 && !audioFile.available()) {
    needAdvance = true;
  }
}

void updateScreen() {
  display.clearDisplay();
  

  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(2);
  display.setCursor(0, 0);

  display.println("WMTU");
  display.drawScrollingText(current.title,  &tcur,  16, T_WAIT, 1, &ttime);
  display.drawScrollingText(current.artist, &acur,  32, T_WAIT, 4, &atime);
  display.drawScrollingText(current.album,  &alcur, 48, T_WAIT, 2, &altime);
  display.display();
}

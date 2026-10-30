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

// SD card SPI pins
#define SD_CS     13
#define SD_SCK    18
#define SD_MISO   19
#define SD_MOSI   23

// Button pins (buttons connect pin to GND when pressed)
#define BTN_NEXT  33
#define BTN_PLAY  32
#define BTN_PREV  14

#define DEBOUNCE_MS  30

Headphones_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

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

void updateScreen();

struct TrackInfo {
  char title[128];
  char artist[128];
  char album[128];
  char path[128];
};

TrackInfo current;   // the only song you display

// ---------- Playback state (shared with button task) ----------
volatile bool needAdvance  = false;
volatile bool needPrevious = false;
volatile bool needScreen   = false;
volatile bool paused       = false;

uint8_t silence[512] = {0};   // written to I2S while paused

// ---------- Button handling ----------
struct Button {
  uint8_t  pin;
  bool     stableState;
  bool     lastReading;
  uint32_t lastChange;
};

Button btnNext = {BTN_NEXT, HIGH, HIGH, 0};
Button btnPlay = {BTN_PLAY, HIGH, HIGH, 0};
Button btnPrev = {BTN_PREV, HIGH, HIGH, 0};

// Returns true once per press (debounced HIGH -> LOW edge)
bool buttonPressed(Button& b) {
  bool reading = digitalRead(b.pin);
  uint32_t now = millis();

  if (reading != b.lastReading) {
    b.lastChange = now;
    b.lastReading = reading;
  }

  if ((now - b.lastChange) > DEBOUNCE_MS && reading != b.stableState) {
    b.stableState = reading;
    if (b.stableState == LOW) return true;
  }
  return false;
}

// Runs on its own, every 5 ms, independent of audio
void buttonTask(void* param) {
  for (;;) {
    if (buttonPressed(btnPlay)) {
      paused = !paused;
      needScreen = true;
      Serial.println(paused ? "BTN: pause" : "BTN: play");
    }

    if (buttonPressed(btnNext)) {
      Serial.println("BTN: next");
      needAdvance = true;
    }

    if (buttonPressed(btnPrev)) {
      Serial.println("BTN: previous");
      needPrevious = true;
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ---------- Metadata ----------
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
  Serial.printf("Now playing: %s\n", path.c_str());

  snprintf(current.title, sizeof(current.title), "%s", path.c_str() + 1);

  outMeta.end();  outMeta.begin();
  out2dec.end();  out2dec.begin();
  copier.begin(out, audioFile);

  tcur = acur = alcur = 0;
  ttime = atime = altime = millis();
}

// ---------- File finding ----------
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
        if (first.isEmpty()) first = path;
        if (foundCurrent && next.isEmpty()) next = path;
        if (path == currentPath) foundCurrent = true;
      }
    }
    entry.close();
  }
  root.close();

  if (!next.isEmpty()) return next;
  return first;
}

String findPrevMP3(const String& currentPath) {
  File root = SD.open("/");
  if (!root) return "";

  String prev = "", last = "", result = "";
  bool found = false;
  File entry;

  while ((entry = root.openNextFile())) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      String lower = name;
      lower.toLowerCase();
      if (lower.endsWith(".mp3")) {
        String path = "/";
        path += name;
        if (!found && path == currentPath) {
          found = true;
          result = prev;
        }
        prev = path;
        last = path;
      }
    }
    entry.close();
  }
  root.close();

  if (!result.isEmpty()) return result;
  return last;   // current was first -> wrap to last song
}

// ---------- Setup ----------
void setup()
{
  Serial.begin(115200);

  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_PLAY, INPUT_PULLUP);
  pinMode(BTN_PREV, INPUT_PULLUP);


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

  outMeta.setCallback(printMetaData);
  loadTrack(mp3Path);

  // Button reader on core 0 (loop() runs on core 1)
  xTaskCreatePinnedToCore(buttonTask, "buttons", 4096, NULL, 2, NULL, 0);
}

// ---------- Loop ----------
void loop()
{
  if (needAdvance) {
    needAdvance = false;
    String next = findNextMP3(String(current.path));
    if (next.isEmpty()) next = findFirstMP3();
    if (!next.isEmpty()) loadTrack(next);
    paused = false;
  }

  if (needPrevious) {
    needPrevious = false;
    String prev = findPrevMP3(String(current.path));
    if (!prev.isEmpty()) loadTrack(prev);
    paused = false;
  }

  uint32_t now = millis();
  if (needScreen || (now - lastscrtime) >= 300) {
    needScreen = false;
    lastscrtime = now;
    updateScreen();
  }

  if (paused) {
    i2s.write(silence, sizeof(silence));
    return;
  }

  size_t n = copier.copy();

  if (n == 0 && !audioFile.available()) {
    needAdvance = true;
  }
}

// ---------- Display ----------
void updateScreen() {
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(2);
  display.setCursor(0, 0);

  display.println(paused ? "PAUSE" : "WMTU");
  display.drawScrollingText(current.title,  &tcur,  16, T_WAIT, 1, &ttime);
  display.drawScrollingText(current.artist, &acur,  32, T_WAIT, 4, &atime);
  display.drawScrollingText(current.album,  &alcur, 48, T_WAIT, 2, &altime);
  display.display();
}

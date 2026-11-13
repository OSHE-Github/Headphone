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
#include "Shuffle.h"
#include "EQSettings.h"
#include <ButtonEvents.h>

// OLED screen I2C pins
#define I2C_SDA   21
#define I2C_SCL   22

// SD card SPI pins
#define SD_CS     13
#define SD_SCK    18
#define SD_MISO   19
#define SD_MOSI   23

// Rotary Encoder pins
#define DI_ENCODER_CLK  15
#define DI_ENCODER_DT   14
#define DI_ENCODER_SW   32
static ButtonEvents encoder_sw(DI_ENCODER_SW);

#define DEBOUNCE_MS     30
#define PLAYBACK_STACK  16384

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
Equalizer3Bands eq(i2s);
ConfigEqualizer3Bands cfg_eq;
EncodedAudioStream out2dec(&i2s, new MP3DecoderHelix());

MultiOutput out;
StreamCopy copier(out, audioFile);

ShufflePlaylist playlist;

void updateScreen();

// Rotary Encoder callbacks
//void knobCallback(long value);
//void buttonCallback(unsigned long duration);

struct TrackInfo {
  char title[128];
  char artist[128];
  char album[128];
  char path[128];
};

TrackInfo current;

// ---------- Playback state (shared with button task) ----------
volatile bool needAdvance  = false;
volatile bool needPrevious = false;
volatile bool needScreen   = false;
volatile bool needShuffle  = false;
volatile bool needRedraw   = false;
volatile bool paused       = false;

uint8_t silence[512] = {0};

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

void loadTrack(const char* path) {
  if (audioFile) audioFile.close();
  memset(&current, 0, sizeof(current));
  if (!path || !path[0]) return;
  snprintf(current.path, sizeof(current.path), "%s", path);

  audioFile = SD.open(path);
  if (!audioFile) {
    Serial.printf("loadTrack: failed to open %s\n", path);
    return;
  }
  Serial.printf("Now playing [%u/%u]: %s\n",
                (unsigned)playlist.position() + 1, (unsigned)playlist.size(), path);

  // fallback title = file name until ID3 arrives
  snprintf(current.title, sizeof(current.title), "%s", path[0] == '/' ? path + 1 : path);

  outMeta.end();  outMeta.begin();
  out2dec.end();  out2dec.begin();
  copier.begin(out, audioFile);

  tcur = acur = alcur = 0;
  ttime = atime = altime = millis();
  needRedraw = true;
}

// ---------- Playback task ----------
void playbackTask(void* param) {
  for (;;) {
    /*if (needShuffle) {
      needShuffle = false;
      playlist.toggleShuffleFlag();
      
      // HACKHACKHACK this should be another function, stopping all playback and going back to scanning
      // could take a lot of time if there are a lot of files.
      playlist.scan(SD, "/");
    }*/

    if (!paused) {
      uint32_t n = copier.copy();
      if (n == 0) {
        if (!audioFile || !audioFile.available()) needAdvance = true;
        vTaskDelay(1);
      }
    } else {
      i2s.write(silence, sizeof(silence));   // blocks until I2S has room
    }

    if (needAdvance) {
      needAdvance = false;
      loadTrack(playlist.next());
      paused = false;
    }

    if (needPrevious) {
      needPrevious = false;
      loadTrack(playlist.prev());
      paused = false;
    }

    uint32_t now = millis();
    if (needScreen || needRedraw || (now - lastscrtime) >= 360) {
      needScreen = false;
      needRedraw = false;
      lastscrtime = now;
      updateScreen();
    }

    

    

    //size_t n = copier.copy();
    /*if (n == 0) {
      if (!audioFile || !audioFile.available()) needAdvance = true;
      vTaskDelay(1);
    }*/
  }
}

static void onEncoderSW(ButtonEvents::Event event, const ButtonEvents::EventData& data,
                     ButtonEvents&) {
  switch (event) {
    case ButtonEvents::Event::PRESSED:
      break;
    case ButtonEvents::Event::HOLDING:
      break;
    case ButtonEvents::Event::RELEASED:
      
      break;
    case ButtonEvents::Event::SEQUENCE:
      if (data.count == 1) {
        paused = !paused;
      } else if (data.count == 2) {
        needAdvance = true;
        paused = false;
      } else {
        needPrevious = true;
        paused = false;
      }

      Serial.print(F("sequence of "));
      Serial.println(data.count);
      break;
  }
}

// ---------- Setup ----------
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
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("OSHE: Scanning");
  display.display();

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(100);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD initialization FAILED!");
    while (true) delay(1000);
  }

  uint32_t t0 = millis();
  size_t count = playlist.scan(SD, "/");
  Serial.printf("Scanned %u tracks in %lu ms\n", (unsigned)count, millis() - t0);
  if (count == 0) {
    Serial.println("No MP3 files found in root directory!");
    while (true) delay(1000);
  }
  playlist.printOrder(Serial);

  startTime = millis();
  ttime = atime = altime = lastscrtime = startTime;

  auto config = i2s.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.pin_bck = I2S_BCK;
  config.pin_ws = I2S_WS;
  config.pin_data = I2S_DATA;
  config.is_master = true;
  config.i2s_format = I2S_STD_FORMAT;
  config.buffer_count = I2S_BUFFER_COUNT;
  config.buffer_size = 1024;
  i2s.begin(config);

  initPresets();
  initEQcfg(&eq, &cfg_eq);

  outMeta.setCallback(printMetaData);
  loadTrack(playlist.current());

  pinMode(DI_ENCODER_CLK, INPUT);
  pinMode(DI_ENCODER_DT, INPUT);
  encoder_sw.onEvent(onEncoderSW);
  encoder_sw.begin();

  xTaskCreatePinnedToCore(playbackTask, "playback", PLAYBACK_STACK, NULL, 3, NULL, 1);
}

void loop() {
  encoder_sw.update();
  static uint8_t p_cstate = HIGH;

  //-1 for CCW, 0 for no turn, 1 for CW
  int8_t dir = 0;
  uint8_t c_cstate = digitalRead(DI_ENCODER_CLK);

  if (p_cstate != c_cstate && c_cstate == HIGH) {
    if (digitalRead(DI_ENCODER_DT) == LOW) {
      dir = -1;
    } else {
      dir = 1;
    }
  } else {
    dir = 0;
  }

  // Trigger turn event
  if (dir != 0) {
    Serial.printf("Direction: %d\n", dir, p_cstate, c_cstate);
  }

  if (dir == 1) {
    needAdvance = true;
  } else if (dir == -1) {
    needPrevious = true;
  }

  p_cstate = c_cstate;

  delay(10);
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
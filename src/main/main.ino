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



// SD card SPI pins
#define SD_CS     13
#define SD_SCK    18
#define SD_MISO   19
#define SD_MOSI   23

// Display configuration
#define I2C_SDA   21
#define I2C_SCL   22
#define I2C_DEV_ADDR 0x55
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

#define METADATA_BUFFER_SIZE 128

// NOTE: this should be increased in the future
#define FILE_NAME_LIST_SIZE 20
uint16_t fname_list_end = 0; // this points to the current end of the file name list
uint16_t fname_list_idx = 0; // this points to our current index (what song we're playing)
char* file_names[FILE_NAME_LIST_SIZE];

AudioInfo info(AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, BITS_PER_SAMPLE);

File audioFile;
MetaDataOutput outMeta;
I2SStream i2s;
EncodedAudioStream out2dec(&i2s, new MP3DecoderHelix());
MultiOutput out;
StreamCopy copier(out, audioFile);

char* mp3title;
char* mp3artist;
char* mp3album;

void updateScreen();

void addFname(char* name);

// Print the song's metadata to Serial and update the strings that the screen uses
void printMetaData(MetaDataType type, const char* str, int len);

String findMP3s();

String findFirstMP3();

void setup() 
{
  Serial.begin(115200);
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

  //delay(100);

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
  display.drawScrollingText(mp3title, &tcur, 16, T_WAIT, 2, &ttime);
  display.drawScrollingText(mp3artist, &acur, 32, T_WAIT, 2, &atime);
  display.drawScrollingText(mp3album, &alcur, 48, T_WAIT, 2, &altime);

  display.display();
}

void printMetaData(MetaDataType type, const char* str, int len) {
  Serial.println(str);

  switch (type) {
    case Title: {
      static char titleBuf[METADATA_BUFFER_SIZE];
      strncpy(titleBuf, str, sizeof(titleBuf) - 1);
      titleBuf[sizeof(titleBuf) - 1] = '\0';
      mp3title = titleBuf;
      break;
    }
    case Artist: {
      static char artistBuf[METADATA_BUFFER_SIZE];
      strncpy(artistBuf, str, sizeof(artistBuf) - 1);
      artistBuf[sizeof(artistBuf) - 1] = '\0';
      mp3artist = artistBuf;
      break;
    }
    case Album: {
      static char albumBuf[METADATA_BUFFER_SIZE];
      strncpy(albumBuf, str, METADATA_BUFFER_SIZE - 1);
      albumBuf[METADATA_BUFFER_SIZE - 1] = '\0';
      mp3album = albumBuf;
      break;
    }
    default:
      break;
  }
}

/*
 * Find all MP3s in the root directory and place them in the file name list.
 * TOOD: if an entry is a directory, we could open it to look for more MP3s,
 * that way the user can have folders of albums or playlists. This could be
 * further improved by passing a directory in to findMP3s, allowing the user
 * to open just one directory/playlist. The only problem with this is that
 * at the moment, it would have to be hard-coded or written through Serial.
*/
String findMP3s() {
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

          addFname(path);
        }
      }

      entry.close();
    }

    root.close();
    return "";
}

// Add the filename specified by name to the fname_list
void addFname(char* name) {
  // If the file name list is full, start over at the beginning
  // Additionally, if the list is not full then increment our end index
  if (fname_list_idx > FILE_NAME_LIST_SIZE-1) {
    fname_list_idx = 0;
  } else {
    fname_list_end++;
  }

  file_names[fname_list_idx] = name;
  fname_list_idx++;
}

// LEGACY. May be removed soon, see findMP3s() instead
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

          file_name[fname_idx] = path;

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
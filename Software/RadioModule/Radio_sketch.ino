
#include <Wire.h>
#include <RDA5807.h>

RDA5807 rx;

const int TUNE_DOWN = 8;
const int TUNE_UP = 9;
const int PRESET_91 = 10;

int currentFrequency = 9630;  // 96.3 MHz

void displayFrequency() {
  Serial.print("Current frequency: ");
  Serial.print(currentFrequency / 100.0, 2);
  Serial.println(" MHz");
}

void setup() {
  Serial.begin(9600);
  delay(1500);

  pinMode(TUNE_DOWN, INPUT_PULLUP);
  pinMode(TUNE_UP, INPUT_PULLUP);
  pinMode(PRESET_91, INPUT_PULLUP);

  Serial.println("Starting radio...");

  rx.setup();

  rx.setFrequency(currentFrequency);
  rx.setVolume(15);
  rx.setMute(false);
  rx.setMono(true);
  rx.setSoftmute(false);

  delay(500);

  Serial.println("Radio ready!");
  displayFrequency();

  Serial.println("D8: Tune Down");
  Serial.println("D9: Tune Up");
  Serial.println("D10: Preset 91.9 MHz");
}

void loop() {

  // Tune down by 0.1 MHz
  if (digitalRead(TUNE_DOWN) == LOW) {
    delay(30);

    if (digitalRead(TUNE_DOWN) == LOW) {
      currentFrequency -= 10;

      if (currentFrequency < 8750) {
        currentFrequency = 8750;
      }

      rx.setFrequency(currentFrequency);
      displayFrequency();

      while (digitalRead(TUNE_DOWN) == LOW) {
        delay(10);
      }
      delay(100);
    }
  }

  // Tune up by 0.1 MHz
  if (digitalRead(TUNE_UP) == LOW) {
    delay(30);

    if (digitalRead(TUNE_UP) == LOW) {
      currentFrequency += 10;

      if (currentFrequency > 10800) {
        currentFrequency = 10800;
      }

      rx.setFrequency(currentFrequency);
      displayFrequency();

      while (digitalRead(TUNE_UP) == LOW) {
        delay(10);
      }
      delay(100);
    }
  }

  // Return to preset 91.9 MHz
  if (digitalRead(PRESET_91) == LOW) {
    delay(30);

    if (digitalRead(PRESET_91) == LOW) {
      currentFrequency = 9190;

      rx.setFrequency(currentFrequency);
      displayFrequency();

      while (digitalRead(PRESET_91) == LOW) {
        delay(10);
      }
      delay(100);
    }
  }
}

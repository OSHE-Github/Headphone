#include <Wire.h>
#include <RDA5807.h>
#include <SSD1306AsciiWire.h>
#include <SSD1306Ascii.h>
#include <ButtonEvents.h>

// TODO: Update any pin assignments to what
// they actually are on the board!!!!!!
// Rotary Encoder Pins
#define DI_ENCODER_CLK  2 // These don't need internal pull-up
#define DI_ENCODER_DT   3
#define DI_ENCODER_SW   14
static ButtonEvents encoder_sw(DI_ENCODER_SW);

// I2C Pins (Display and RDA5807)
#define I2C_SDA     18 // These MUST be internal pull-up
#define I2C_SCL     19

// Display Configuration
#define SCR_W       128
#define SCR_H       64
#define OLED_RESET  -1
#define SCR_ADDR    0x3C
#define SCR_TIME    33
SSD1306AsciiWire display;

// RDA5807 Radio IC Configuration
#define FIX_BAND RADIO_BAND_FM
RDA5807 radio;

#define NUM_PRESETS 3
uint16_t presetIdx = 0;

uint16_t currentFrequency = 9630;  // 96.3 MHz
uint16_t presets[NUM_PRESETS] = {9190, 9630, 9770};
volatile bool needScreen = true;
volatile bool needIncrease = false;
volatile bool needDecrease = false;
volatile bool needPressed = false;
volatile bool presetMenuState = false;
volatile bool needScreenRefresh = false;
volatile uint32_t last_input_time = millis();
volatile bool lock_flag = false;

void updateScreen();
void handleRotaryEncoder();
static void onEncoderSW(ButtonEvents::Event event, const ButtonEvents::EventData& data,
                      ButtonEvents&);

void displayFrequency() {
  //Serial.print(F("Current frequency: "));
  //Serial.print(currentFrequency / 100.0, 2);
  //Serial.println(" MHz");
}

void setup() {
  Serial.begin(9600);

  // Start the I2C Wire
  Wire.begin();
  Wire.setClock(400000L);
  display.begin(&Adafruit128x64, 0x3C);
  // Try to start the screen
  //if (!display.begin(&Adafruit128x64, 0x3C)) {
  //  Serial.println(F("SSD1306 allocation failed"));
  //  while (1);
  //}

  display.setFont(Verdana12_bold);

  // Write "OSHE" to the top of the screen
  display.clear();
  //display.setTextColor(SSD1306_WHITE);
  //display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("OSHE");
  //display.display();

  // Setup and begin the Rotary Encoder
  pinMode(DI_ENCODER_CLK, INPUT);
  pinMode(DI_ENCODER_DT, INPUT);
  encoder_sw.onEvent(onEncoderSW);
  encoder_sw.begin();

  //Serial.println("Starting radio...");

  radio.setup();

  radio.setFrequency(currentFrequency);
  radio.setVolume(7);
  radio.setMute(false);
  radio.setMono(false);
  radio.setSoftmute(false);

  delay(500);

  //Serial.println("Radio ready!");
  displayFrequency();

  //Serial.println(F("D8: Tune Down"));
  //Serial.println(F("D9: Tune Up"));
  //Serial.println(F("D10: Preset 91.9 MHz"));
}

void loop() {
  uint32_t now = millis();
  static bool dim_flag = false;
  static uint16_t lastFrequency = currentFrequency;
  handleRotaryEncoder();

  if (now - last_input_time > 10000 && dim_flag == false) {
    display.setContrast(40);
    needScreenRefresh = true;
    dim_flag = true;
  } else if (now - last_input_time <= 10000 && dim_flag == true) {
    display.setContrast(127);
    needScreenRefresh = true;
    dim_flag = false;
  }

  if (needScreenRefresh) {
    needScreenRefresh = false;
    display.clear();
    needScreen = true;
  }

  if (needScreen) {
    needScreen = false;
    updateScreen();
  }

  if (presetMenuState) {
    if (needIncrease) {
      needIncrease = false;
      presets[presetIdx] += 10;

      needScreen = true;
    } 

    if (needDecrease) {
      needDecrease = false;
      presets[presetIdx] -= 10;

      needScreen = true;
    }

    if (needPressed && !lock_flag) {
      needPressed = false;
      if (presetIdx < NUM_PRESETS-1) {
        presetIdx++;
      } else {
        presetIdx = 0;
      }

      needScreen = true;
      needScreenRefresh = true;
    }
  } else {
    if (needPressed) {
      needPressed = false;
      lock_flag = !lock_flag;
      needScreenRefresh = true;
    }

    if (!lock_flag) {
      if (needIncrease) {
        needIncrease = false;
        currentFrequency += 10;
      
        needScreen = true;
      }

      if (needDecrease) {
        needDecrease = false;
        currentFrequency -= 10;

        needScreen = true;
      }
    }
  }

  if (currentFrequency != lastFrequency) {
    radio.setFrequency(currentFrequency);
  }
  
  lastFrequency = currentFrequency;
}


void updateScreen() {
  static bool wmtu_prev_flag = false;
  display.setCursor(0, 0);

  if (currentFrequency == 9190) {
    wmtu_prev_flag = true;
    display.print(F("WMTU"));
  } else {

    // HACKHACKHACK clear the buffer if we were just at WMTU because
    // in the font, WMTU and OSHE are not the same width
    if (wmtu_prev_flag == true) {
      display.clear();
      wmtu_prev_flag = false;
    }

    display.print(F("OSHE"));
  }

  if (presetMenuState) {
    display.println(F("      PRESETS"));
    display.println(F("P-1        P-2        P-3"));
    display.print(presets[0]/100);
    display.print(F("."));
    display.print(presets[0]%100);
    display.print(F("   "));
    display.print(presets[1]/100);
    display.print(F("."));
    display.print(presets[1]%100);
    display.print(F("   "));
    display.print(presets[2]/100);
    display.print(F("."));
    display.println(presets[2]%100);
    display.print(F("   "));
    display.setCursor(42*presetIdx + 15, 58);
    display.print(F("^"));
    //display.printf("%d.%d FM\n", presets[0]/100, presets[0]%100);
  } else {
    if (lock_flag) {
      display.print(" LOCK");
    }
    display.print("\n");
    display.print(currentFrequency/100);
    display.print(F("."));
    display.print(currentFrequency%100);
    display.println(F(" FM"));
    //display.printf("%d.%d FM\n", currentFrequency/100, currentFrequency%100);
  }

  //display.display();
}

void handleRotaryEncoder() {
  encoder_sw.update();
  static volatile uint8_t p_cstate = HIGH;
  
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
    last_input_time = millis();
  }

  if (dir == 1) {
    needIncrease = true;
  } else if (dir == -1) {
    needDecrease = true;
  }

  p_cstate = c_cstate;
}

static void onEncoderSW(ButtonEvents::Event event, const ButtonEvents::EventData& data,
                      ButtonEvents&) {
  switch (event) {
    case ButtonEvents::Event::PRESSED:
      break;
    case ButtonEvents::Event::HOLDING:
      break;
    case ButtonEvents::Event::RELEASED:
      if (!lock_flag && data.heldMs > 750) {
        if (presetMenuState == true) {
          currentFrequency = presets[presetIdx];
        }

        presetMenuState = !presetMenuState;
        needScreenRefresh = true;
      } else {
        needPressed = true;
      }

      last_input_time = millis();
      break;
    case ButtonEvents::Event::SEQUENCE:
      if (!lock_flag) {
        if (data.count == 2) {
          currentFrequency = 9190;
        }

        last_input_time = millis();
      }
      break;
  }
}
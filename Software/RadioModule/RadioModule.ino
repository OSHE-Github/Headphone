#include <Wire.h>
#include <RDA5807.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ButtonEvents.h>

// TODO: Update any pin assignments to what
// they actually are on the board!!!!!!
// Rotary Encoder Pins
#define DI_ENCODER_CLK  XX // These don't need internal pull-up
#define DI_ENCODER_DT   XX
#define DI_ENCODER_SW   XX
static ButtonEvents encoder_sw(DI_ENCODER_SW);

// I2C Pins (Display and RDA5807)
#define I2C_SDA     XX // These MUST be internal pull-up
#define I2C_SCL     XX

// Display Configuration
#define SCR_W       128
#define SCR_H       64
#define OLED_RESET  -1
#define SCR_ADDR    0x3C
#define SCR_TIME    33
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, OLED_RESET);

// RDA5807 Radio IC Configuration
RDA5807 radio;

#define NUM_PRESETS 3

uint16_t currentFrequency = 9630;  // 96.3 MHz
uint16_t presets[NUM_PRESETS] = {9190, 9630, 9770};
bool needScreen = false;
bool needIncrease = false;
bool needDecrease = false;
bool needPressed = false;
bool presetMenuState = false;

void updateScreen();
void handleRotaryEncoder();
static void onEncoderSW(ButtonEvents::Event event, const ButtonEvents::EventData& data,
                      ButtonEvents&);

void displayFrequency() {
  Serial.print("Current frequency: ");
  Serial.print(currentFrequency / 100.0, 2);
  Serial.println(" MHz");
}

void setup() {
  Serial.begin(9600);

  // Start the I2C Wire
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  // Try to start the screen
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

  // Write "OSHE" to the top of the screen
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("OSHE");
  display.display();

  // Setup and begin the Rotary Encoder
  pinMode(DI_ENCODER_CLK, INPUT);
  pinMode(DI_ENCODER_DT, INPUT);
  encoder_sw.onEvent(onEncoderSW);
  encoder_sw.begin();

  Serial.println("Starting radio...");

  radio.setup();

  radio.setFrequency(currentFrequency);
  radio.setVolume(15);
  radio.setMute(false);
  radio.setMono(true); // Why are we in mono??
  radio.setSoftmute(false);

  delay(500);

  Serial.println("Radio ready!");
  displayFrequency();
}

void loop() {
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

    if (needPressed) {
      if (presetIdx < NUM_PRESETS) {
        presetIdx++;
      } else {
        presetIdx = 0;
      }

      needScreen = true;
    }
  } else {
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

  handleRotaryEncoder();
}


void updateScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(2);
  display.setCursor(0, 0);

  display.println("OSHE");
  if (presetMenuState) {
    display.println("Preset 1");
    display.printf("%d.%d FM\n", presets[0]/100, presets[0]%100);
  } else {
    display.printf("%d.%d FM\n", currentFrequency/100, currentFrequency%100);
  }

  display.display();
}

void handleRotaryEncoder() {
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
    needIncrease = true;
  } else if (dir == -1) {
    needDecrease = true;
  }

  p_cstate = c_cstate;

  delay(10);
}

static void onEncoderSW(ButtonEvents::Event event, const ButtonEvents::EventData& data,
                      ButtonEvents&) {
  switch (event) {
    case ButtonEvents::Event::PRESSED:
      break;
    case ButtonEvents::Event::HOLDING:
      break;
    case ButtonEvents::Event::RELEASED:
      if (data.heldMs > 750) {
        presetMenuState = !presetMenuState;
      } else {
        needPressed = true;
      }
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

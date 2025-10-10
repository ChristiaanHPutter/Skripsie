// ESP32 Button and LED Control with SH1106 OLED Display
// Using U8g2 library for 1.3" SH1106 OLED
// Based on your working code structure

#include <U8g2lib.h>
#include <Wire.h>

// Create display object for SH1106 1.3" OLED with I2C
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Pin Definitions - Buttons
#define BTN_1 32
#define BTN_2 33
#define BTN_3 27
#define BTN_4 26
#define BTN_5 0

// Pin Definitions - LEDs
#define LED_1 18
#define LED_2 19
#define LED_3 23
#define LED_4 13
#define LED_5 5
#define LED_6 15

// Button debouncing variables
const int NUM_BUTTONS = 5;
const int buttonPins[NUM_BUTTONS] = {BTN_1, BTN_2, BTN_3, BTN_4, BTN_5};
bool lastButtonReading[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
bool buttonState[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
bool lastButtonState[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime[NUM_BUTTONS] = {0, 0, 0, 0, 0};
const unsigned long DEBOUNCE_DELAY = 50;

// LED array for easy control
const int ledPins[6] = {LED_1, LED_2, LED_3, LED_4, LED_5, LED_6};

// LED cycling variables
int currentLED = 0;
unsigned long lastLEDUpdate = 0;
const unsigned long LED_CYCLE_INTERVAL = 500;  // 500ms per LED
bool ledCyclingActive = false;

// Last pressed button
int lastPressedButton = 0;

void setup(void) {
  Serial.begin(115200);
  Serial.println("=== ESP32 Button + LED + OLED System ===");
  
  // Initialize I2C with custom pins (SDA=21, SCL=22)
  Wire.begin(21, 22);
  
  // Initialize the U8g2 display
  u8g2.begin();
  
  Serial.println("SH1106 OLED Display initialized!");
  
  // Display welcome message
  displayWelcomeScreen();
  
  // Initialize button pins with internal pull-ups
  for(int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    Serial.print("Button ");
    Serial.print(i + 1);
    Serial.print(" on GPIO ");
    Serial.println(buttonPins[i]);
  }
  
  // Initialize LED pins
  for(int i = 0; i < 6; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
    Serial.print("LED ");
    Serial.print(i + 1);
    Serial.print(" on GPIO ");
    Serial.println(ledPins[i]);
  }
  
  Serial.println("System ready!");
  
  // Show initial display
  updateDisplay(0, false);
}

void loop(void) {
  // Check all buttons
  for(int i = 0; i < NUM_BUTTONS; i++) {
    if(readButtonDebounced(i)) {
      int buttonNumber = i + 1;
      
      Serial.print("Button ");
      Serial.print(buttonNumber);
      Serial.println(" pressed!");
      
      if(buttonNumber == 5) {
        // Button 5: Toggle LED cycling
        ledCyclingActive = !ledCyclingActive;
        
        if(ledCyclingActive) {
          Serial.println("LED cycling STARTED");
          currentLED = 0;
          lastLEDUpdate = millis();
          turnOffAllLEDs();
          digitalWrite(ledPins[currentLED], HIGH);
        } else {
          Serial.println("LED cycling STOPPED");
          turnOffAllLEDs();
        }
      }
      
      lastPressedButton = buttonNumber;
      updateDisplay(buttonNumber, ledCyclingActive);
    }
  }
  
  // Handle LED cycling
  if(ledCyclingActive) {
    if(millis() - lastLEDUpdate >= LED_CYCLE_INTERVAL) {
      digitalWrite(ledPins[currentLED], LOW);
      
      currentLED++;
      if(currentLED >= 6) {
        currentLED = 0;  // Loop back to LED 1
      }
      
      digitalWrite(ledPins[currentLED], HIGH);
      
      Serial.print("LED ");
      Serial.print(currentLED + 1);
      Serial.println(" ON");
      
      lastLEDUpdate = millis();
      updateDisplay(lastPressedButton, ledCyclingActive);
    }
  }
  
  delay(1);
}

bool readButtonDebounced(int buttonIndex) {
  bool reading = digitalRead(buttonPins[buttonIndex]);
  bool buttonPressed = false;
  
  if(reading != lastButtonReading[buttonIndex]) {
    lastDebounceTime[buttonIndex] = millis();
  }
  
  unsigned long timeDiff = millis() - lastDebounceTime[buttonIndex];
  if(timeDiff >= DEBOUNCE_DELAY) {
    if(reading != buttonState[buttonIndex]) {
      buttonState[buttonIndex] = reading;
      
      if(buttonState[buttonIndex] == LOW && lastButtonState[buttonIndex] == HIGH) {
        buttonPressed = true;
      }
      lastButtonState[buttonIndex] = buttonState[buttonIndex];
    }
  }
  
  lastButtonReading[buttonIndex] = reading;
  return buttonPressed;
}

void updateDisplay(int buttonNum, bool cycling) {
  // Clear the buffer
  u8g2.clearBuffer();
  
  // Draw border frame
  u8g2.drawFrame(0, 0, 128, 64);
  
  // Title at top
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(25, 12, "Button + LED");
  
  // Horizontal line separator
  u8g2.drawHLine(2, 15, 124);
  
  // Display button number - LARGE
  u8g2.setFont(u8g2_font_ncenB18_tr);
  if(buttonNum > 0) {
    char btnStr[10];
    sprintf(btnStr, "BTN: %d", buttonNum);
    int btnWidth = u8g2.getStrWidth(btnStr);
    int btnX = (128 - btnWidth) / 2;
    u8g2.drawStr(btnX, 35, btnStr);
  } else {
    u8g2.drawStr(30, 35, "BTN: --");
  }
  
  // Display LED cycling status
  u8g2.setFont(u8g2_font_6x10_tr);
  if(cycling) {
    char ledStr[20];
    sprintf(ledStr, "Cycling: LED %d", currentLED + 1);
    int ledWidth = u8g2.getStrWidth(ledStr);
    int ledX = (128 - ledWidth) / 2;
    u8g2.drawStr(ledX, 50, ledStr);
  } else {
    u8g2.drawStr(28, 50, "Cycling: OFF");
  }
  
  // Status indicator at bottom
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(45, 60, "Ready");
  
  // Send buffer to display
  u8g2.sendBuffer();
}

void displayWelcomeScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(25, 20, "ESP32 OLED");
  u8g2.drawStr(15, 35, "Button & LED");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(28, 50, "Initializing...");
  u8g2.sendBuffer();
  
  Serial.println("Welcome screen displayed!");
  delay(2000);
}

void turnOffAllLEDs() {
  for(int i = 0; i < 6; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}
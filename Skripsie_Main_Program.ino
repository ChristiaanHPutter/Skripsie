// ESP32 SSR MOSFET Tester
// Tests the 3 MOSFET gates that control SSRs for heating elements
// Use with multimeter to verify proper operation

#include <U8g2lib.h>
#include <Wire.h>

// OLED Display
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Button pins
#define BTN_1 32  // Select SSR 1
#define BTN_2 33  // Select SSR 2
#define BTN_3 27  // Select SSR 3
#define BTN_4 26  // Toggle current SSR ON/OFF
#define BTN_5 0   // All OFF (Emergency stop)

// SSR MOSFET Control Pins
#define SSR_1 4   // Heating Element Zone 1
#define SSR_2 17  // Heating Element Zone 2
#define SSR_3 14  // Heating Element Zone 3

// Button debouncing
const int NUM_BUTTONS = 5;
const int buttonPins[NUM_BUTTONS] = {BTN_1, BTN_2, BTN_3, BTN_4, BTN_5};
bool lastButtonReading[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
bool buttonState[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
bool lastButtonState[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime[NUM_BUTTONS] = {0, 0, 0, 0, 0};
const unsigned long DEBOUNCE_DELAY = 50;

// SSR control
const int ssrPins[3] = {SSR_1, SSR_2, SSR_3};
bool ssrStates[3] = {false, false, false};
int currentSSR = 0;  // 0 = SSR1, 1 = SSR2, 2 = SSR3

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== SSR MOSFET Tester ===");
  
  // Initialize I2C
  Serial.println("Initializing I2C...");
  Wire.begin(21, 22);
  delay(100);
  
  // Initialize display
  Serial.println("Initializing display...");
  u8g2.begin();
  
  // Initialize buttons
  Serial.println("Initializing buttons...");
  for(int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  
  // Initialize SSR pins as outputs (all OFF)
  Serial.println("Initializing SSR MOSFET pins...");
  for(int i = 0; i < 3; i++) {
    pinMode(ssrPins[i], OUTPUT);
    digitalWrite(ssrPins[i], LOW);
    ssrStates[i] = false;
    Serial.print("SSR ");
    Serial.print(i + 1);
    Serial.print(" (GPIO");
    Serial.print(ssrPins[i]);
    Serial.println(") = OFF");
  }
  
  // Welcome screen
  displayWelcomeScreen();
  
  // Show main screen
  updateDisplay();
  
  Serial.println("\n=== System Ready ===");
  Serial.println("BTN1 = Select SSR 1");
  Serial.println("BTN2 = Select SSR 2");
  Serial.println("BTN3 = Select SSR 3");
  Serial.println("BTN4 = Toggle selected SSR ON/OFF");
  Serial.println("BTN5 = ALL OFF (Emergency Stop)");
  Serial.println("====================\n");
}

void loop() {
  // Check buttons
  for(int i = 0; i < NUM_BUTTONS; i++) {
    if(readButtonDebounced(i)) {
      handleButton(i + 1);
    }
  }
  
  delay(10);
}

void handleButton(int buttonNum) {
  Serial.print(">>> Button ");
  Serial.print(buttonNum);
  Serial.println(" pressed <<<");
  
  switch(buttonNum) {
    case 1:  // Select SSR 1
      currentSSR = 0;
      Serial.println("SSR 1 selected (GPIO4)");
      break;
      
    case 2:  // Select SSR 2
      currentSSR = 1;
      Serial.println("SSR 2 selected (GPIO17)");
      break;
      
    case 3:  // Select SSR 3
      currentSSR = 2;
      Serial.println("SSR 3 selected (GPIO14)");
      break;
      
    case 4:  // Toggle current SSR
      ssrStates[currentSSR] = !ssrStates[currentSSR];
      digitalWrite(ssrPins[currentSSR], ssrStates[currentSSR] ? HIGH : LOW);
      Serial.print("SSR ");
      Serial.print(currentSSR + 1);
      Serial.print(" (GPIO");
      Serial.print(ssrPins[currentSSR]);
      Serial.print(") = ");
      Serial.println(ssrStates[currentSSR] ? "ON" : "OFF");
      break;
      
    case 5:  // Emergency stop - ALL OFF
      Serial.println("!!! EMERGENCY STOP - ALL SSRs OFF !!!");
      for(int i = 0; i < 3; i++) {
        ssrStates[i] = false;
        digitalWrite(ssrPins[i], LOW);
        Serial.print("SSR ");
        Serial.print(i + 1);
        Serial.println(" = OFF");
      }
      break;
  }
  
  updateDisplay();
}

void updateDisplay() {
  u8g2.clearBuffer();
  
  // Border
  u8g2.drawFrame(0, 0, 128, 64);
  
  // Title
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(22, 10, "SSR MOSFET Test");
  u8g2.drawHLine(2, 12, 124);
  
  // Current SSR selection indicator
  u8g2.setFont(u8g2_font_6x10_tr);
  char selStr[20];
  sprintf(selStr, "Selected: SSR %d", currentSSR + 1);
  int selX = (128 - u8g2.getStrWidth(selStr)) / 2;
  u8g2.drawStr(selX, 22, selStr);
  
  // Show GPIO pin
  u8g2.setFont(u8g2_font_5x7_tr);
  char gpioStr[20];
  sprintf(gpioStr, "(GPIO %d)", ssrPins[currentSSR]);
  int gpioX = (128 - u8g2.getStrWidth(gpioStr)) / 2;
  u8g2.drawStr(gpioX, 30, gpioStr);
  
  // Show status of all SSRs
  u8g2.setFont(u8g2_font_6x10_tr);
  for(int i = 0; i < 3; i++) {
    char statusStr[15];
    sprintf(statusStr, "SSR%d: %s", i + 1, ssrStates[i] ? "ON" : "OFF");
    
    // Highlight selected SSR
    if(i == currentSSR) {
      u8g2.drawBox(10, 34 + (i * 8), 108, 9);
      u8g2.setColorIndex(0);  // Inverted color
      u8g2.drawStr(15, 41 + (i * 8), statusStr);
      u8g2.setColorIndex(1);  // Normal color
    } else {
      u8g2.drawStr(15, 41 + (i * 8), statusStr);
    }
  }
  
  // Button help
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(2, 60, "1-3:Sel 4:Toggle 5:AllOff");
  
  u8g2.sendBuffer();
}

void displayWelcomeScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(10, 20, "SSR MOSFET");
  u8g2.drawStr(35, 35, "Tester");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(10, 50, "All SSRs OFF");
  u8g2.sendBuffer();
  delay(2000);
}

bool readButtonDebounced(int buttonIndex) {
  bool reading = digitalRead(buttonPins[buttonIndex]);
  bool buttonPressed = false;
  
  if(reading != lastButtonReading[buttonIndex]) {
    lastDebounceTime[buttonIndex] = millis();
  }
  
  if((millis() - lastDebounceTime[buttonIndex]) >= DEBOUNCE_DELAY) {
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
// ESP32 Motor Relay MOSFET Tester
// Tests the MOSFET gate that controls the motor relay
// Use with multimeter to verify proper operation

#include <U8g2lib.h>
#include <Wire.h>

// OLED Display
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Button pins
#define BTN_1 32  // Toggle motor ON/OFF
#define BTN_2 33  // Not used
#define BTN_3 27  // Not used
#define BTN_4 26  // Not used
#define BTN_5 0   // Emergency stop (OFF)

// Motor Relay Control Pin
#define MOTOR_RELAY 16  // GPIO16 controls motor relay

// Button debouncing
const int NUM_BUTTONS = 5;
const int buttonPins[NUM_BUTTONS] = {BTN_1, BTN_2, BTN_3, BTN_4, BTN_5};
bool lastButtonReading[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
bool buttonState[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
bool lastButtonState[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime[NUM_BUTTONS] = {0, 0, 0, 0, 0};
const unsigned long DEBOUNCE_DELAY = 50;

// Motor control
bool motorState = false;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== Motor Relay MOSFET Tester ===");
  
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
  
  // Initialize Motor Relay pin
  Serial.println("Initializing Motor Relay MOSFET pin...");
  pinMode(MOTOR_RELAY, OUTPUT);
  digitalWrite(MOTOR_RELAY, LOW);  // Start with motor OFF
  
  Serial.print("Motor Relay (GPIO");
  Serial.print(MOTOR_RELAY);
  Serial.println(") = OFF");
  
  // Welcome screen
  displayWelcomeScreen();
  
  // Show main screen
  updateDisplay();
  
  Serial.println("\n=== System Ready ===");
  Serial.println("BTN1 = Toggle Motor ON/OFF");
  Serial.println("BTN5 = Emergency Stop (OFF)");
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
    case 1:  // Toggle motor ON/OFF
      motorState = !motorState;
      digitalWrite(MOTOR_RELAY, motorState ? HIGH : LOW);
      
      Serial.print("Motor = ");
      Serial.println(motorState ? "ON" : "OFF");
      Serial.print("Expected voltage at GPIO16: ");
      Serial.println(motorState ? "~3.3V" : "~0V");
      
      if(motorState) {
        Serial.println("Relay should CLICK and activate");
      } else {
        Serial.println("Relay should CLICK and deactivate");
      }
      break;
      
    case 5:  // Emergency stop
      Serial.println("!!! EMERGENCY STOP - Motor OFF !!!");
      motorState = false;
      digitalWrite(MOTOR_RELAY, LOW);
      Serial.println("Motor = OFF");
      Serial.println("Expected voltage at GPIO16: ~0V");
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
  u8g2.drawStr(15, 10, "Motor Relay Test");
  u8g2.drawHLine(2, 12, 124);
  
  // GPIO info
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(38, 24, "GPIO 16");
  
  // Motor state - LARGE
  u8g2.setFont(u8g2_font_ncenB24_tr);
  const char* stateText = motorState ? "ON" : "OFF";
  int stateWidth = u8g2.getStrWidth(stateText);
  int stateX = (128 - stateWidth) / 2;
  u8g2.drawStr(stateX, 46, stateText);
  
  // Status indicator
  u8g2.setFont(u8g2_font_6x10_tr);
  if(motorState) {
    u8g2.drawStr(30, 56, "Relay Active");
  } else {
    u8g2.drawStr(26, 56, "Relay Inactive");
  }
  
  // Button help
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(10, 62, "BTN1:Toggle BTN5:Stop");
  
  u8g2.sendBuffer();
}

void displayWelcomeScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(5, 20, "Motor Relay");
  u8g2.drawStr(18, 35, "MOSFET Test");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(25, 50, "Motor OFF");
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
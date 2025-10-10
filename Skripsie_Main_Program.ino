// ESP32 DS18B20 Temperature Sensor Identifier
// This tool helps you identify and label multiple DS18B20 sensors
// by their unique OneWire addresses

#include <U8g2lib.h>
#include <Wire.h>
#include <OneWire.h>

// Hardware setup
#define DS18B20_PIN 25  // OneWire data pin
OneWire ds(DS18B20_PIN);

// OLED Display
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Button pins
#define BTN_1 32  // Scan for sensors
#define BTN_2 33  // Previous sensor
#define BTN_3 27  // Next sensor
#define BTN_4 26  // Read temperature from current sensor
#define BTN_5 0   // Not used

// Button debouncing
const int NUM_BUTTONS = 5;
const int buttonPins[NUM_BUTTONS] = {BTN_1, BTN_2, BTN_3, BTN_4, BTN_5};
bool lastButtonReading[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
bool buttonState[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
bool lastButtonState[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime[NUM_BUTTONS] = {0, 0, 0, 0, 0};
const unsigned long DEBOUNCE_DELAY = 50;

// Sensor storage
const int MAX_SENSORS = 10;
byte sensorAddresses[MAX_SENSORS][8];
int sensorCount = 0;
int currentSensorIndex = 0;

// Temperature reading
float lastTemperature = -999.0;

void setup() {
  Serial.begin(115200);
  Serial.println("=== DS18B20 Sensor Identifier ===");
  
  // Initialize I2C
  Wire.begin(21, 22);
  
  // Initialize display
  u8g2.begin();
  
  // Initialize buttons
  for(int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  
  // Welcome screen
  displayWelcomeScreen();
  
  // Initial scan
  scanForSensors();
  
  displayMainScreen();
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
  Serial.print("Button ");
  Serial.print(buttonNum);
  Serial.println(" pressed");
  
  switch(buttonNum) {
    case 1:  // Scan for sensors
      scanForSensors();
      currentSensorIndex = 0;
      lastTemperature = -999.0;
      break;
      
    case 2:  // Previous sensor
      if(sensorCount > 0) {
        currentSensorIndex--;
        if(currentSensorIndex < 0) {
          currentSensorIndex = sensorCount - 1;
        }
        lastTemperature = -999.0;
      }
      break;
      
    case 3:  // Next sensor
      if(sensorCount > 0) {
        currentSensorIndex++;
        if(currentSensorIndex >= sensorCount) {
          currentSensorIndex = 0;
        }
        lastTemperature = -999.0;
      }
      break;
      
    case 4:  // Read temperature
      if(sensorCount > 0) {
        lastTemperature = readTemperatureFromSensor(currentSensorIndex);
      }
      break;
  }
  
  displayMainScreen();
}

void scanForSensors() {
  Serial.println("Scanning for DS18B20 sensors...");
  
  sensorCount = 0;
  ds.reset_search();
  
  displayScanningScreen();
  
  byte addr[8];
  while(ds.search(addr)) {
    // Verify CRC
    if(OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC invalid, skipping device");
      continue;
    }
    
    // Check if it's a DS18B20
    if(addr[0] != 0x10 && addr[0] != 0x28) {
      Serial.println("Not a DS18B20, skipping");
      continue;
    }
    
    // Store the address
    if(sensorCount < MAX_SENSORS) {
      for(int i = 0; i < 8; i++) {
        sensorAddresses[sensorCount][i] = addr[i];
      }
      
      Serial.print("Sensor ");
      Serial.print(sensorCount + 1);
      Serial.print(" found: ");
      printAddress(addr);
      Serial.println();
      
      sensorCount++;
    }
  }
  
  ds.reset_search();
  
  Serial.print("Total sensors found: ");
  Serial.println(sensorCount);
  
  delay(500);  // Brief pause to show scanning screen
}

float readTemperatureFromSensor(int index) {
  if(index < 0 || index >= sensorCount) {
    return -999.0;
  }
  
  byte* addr = sensorAddresses[index];
  byte data[12];
  
  // Start conversion
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);
  
  delay(1000);  // Wait for conversion
  
  // Read scratchpad
  ds.reset();
  ds.select(addr);
  ds.write(0xBE);
  
  for(int i = 0; i < 9; i++) {
    data[i] = ds.read();
  }
  
  // Calculate temperature
  int16_t raw = (data[1] << 8) | data[0];
  float celsius = (float)raw / 16.0;
  
  Serial.print("Temperature from sensor ");
  Serial.print(index + 1);
  Serial.print(": ");
  Serial.print(celsius);
  Serial.println(" °C");
  
  return celsius;
}

void displayMainScreen() {
  u8g2.clearBuffer();
  
  // Border
  u8g2.drawFrame(0, 0, 128, 64);
  
  // Title
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(20, 10, "Sensor ID Tool");
  u8g2.drawHLine(2, 12, 124);
  
  if(sensorCount == 0) {
    // No sensors found
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(15, 28, "No sensors found");
    u8g2.drawStr(10, 40, "Press BTN1 to scan");
  } else {
    // Show current sensor info
    u8g2.setFont(u8g2_font_6x10_tr);
    
    // Sensor count and index
    char countStr[30];
    sprintf(countStr, "Sensor %d of %d", currentSensorIndex + 1, sensorCount);
    int x = (128 - u8g2.getStrWidth(countStr)) / 2;
    u8g2.drawStr(x, 22, countStr);
    
    // Address (split into two lines for readability)
    u8g2.setFont(u8g2_font_5x7_tr);
    char addrStr1[20];
    char addrStr2[20];
    byte* addr = sensorAddresses[currentSensorIndex];
    
    sprintf(addrStr1, "%02X%02X%02X%02X", addr[0], addr[1], addr[2], addr[3]);
    sprintf(addrStr2, "%02X%02X%02X%02X", addr[4], addr[5], addr[6], addr[7]);
    
    u8g2.drawStr(20, 32, addrStr1);
    u8g2.drawStr(20, 40, addrStr2);
    
    // Temperature if read
    if(lastTemperature > -999.0) {
      char tempStr[15];
      dtostrf(lastTemperature, 5, 1, tempStr);
      strcat(tempStr, " C");
      u8g2.setFont(u8g2_font_6x10_tr);
      int tx = (128 - u8g2.getStrWidth(tempStr)) / 2;
      u8g2.drawStr(tx, 50, tempStr);
    }
  }
  
  // Button help
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(2, 60, "1:Scan 2:< 3:> 4:Temp");
  
  u8g2.sendBuffer();
}

void displayWelcomeScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(8, 20, "DS18B20 Sensor");
  u8g2.drawStr(25, 35, "Identifier");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(28, 50, "Starting...");
  u8g2.sendBuffer();
  delay(2000);
}

void displayScanningScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(25, 30, "Scanning...");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(15, 45, "Looking for sensors");
  u8g2.sendBuffer();
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

void printAddress(byte* addr) {
  for(int i = 0; i < 8; i++) {
    if(addr[i] < 16) Serial.print("0");
    Serial.print(addr[i], HEX);
    if(i < 7) Serial.print(":");
  }
}
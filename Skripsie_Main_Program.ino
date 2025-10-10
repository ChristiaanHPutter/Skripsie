#include <U8g2lib.h>
#include <Wire.h>
#include <OneWire.h>

// Temperature sensor setup
#define DS18B20_PIN 25  // Digital pin 17 for DS18B20 data line
OneWire ds(DS18B20_PIN);

// Create display object for SH1106 1.3" OLED with I2C
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Variables for temperature reading
float currentTemperature = -999.0;  // Initialize with error value
unsigned long lastTempRead = 0;
const unsigned long TEMP_READ_INTERVAL = 1000;  // Read temperature every 2 seconds

void setup(void) {
  // Initialize Serial Monitor for debugging
  Serial.begin(115200);
  Serial.println("Starting OLED + Temperature Sensor...");
  
  // Initialize I2C with custom pins (SDA=21, SCL=22)
  Wire.begin(21, 22);
  
  // Initialize the display
  u8g2.begin();
  
  // Test if display is working
  displayWelcomeMessage();
  
  // Initial temperature reading
  currentTemperature = readTemperature();
}

void loop(void) {
  // Read temperature at intervals
  if (millis() - lastTempRead >= TEMP_READ_INTERVAL) {
    currentTemperature = readTemperature();
    lastTempRead = millis();
  }
  
  // Clear the screen and display temperature
  displayTemperature();
  
  delay(100);  // Small delay for stability
}

float readTemperature() {
  byte data[12];
  byte addr[8];
  
  // Search for DS18B20 on the bus
  if (!ds.search(addr)) {
    Serial.println("No temperature sensor found!");
    ds.reset_search();
    return -999.0;  // Error value
  }
  
  // Verify CRC of the address
  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return -999.0;
  }
  
  // Check if device is recognized (should be 0x10 or 0x28 for DS18B20)
  if (addr[0] != 0x10 && addr[0] != 0x28) {
    Serial.println("Device is not a DS18B20 family device.");
    return -999.0;
  }
  
  // Start temperature conversion
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);  // Start conversion with parasite power
  
  delay(1000);  // Wait for conversion (could be up to 750ms)
  
  // Check if conversion is complete
  byte present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);  // Read Scratchpad
  
  // Read 9 bytes of data
  for (int i = 0; i < 9; i++) {
    data[i] = ds.read();
  }
  
  ds.reset_search();
  
  // Extract temperature from data
  byte MSB = data[1];
  byte LSB = data[0];
  
  // Convert to temperature using two's complement
  float tempRead = ((MSB << 8) | LSB);
  float temperatureSum = tempRead / 16.0;  // DS18B20 resolution
  
  Serial.print("Temperature: ");
  Serial.print(temperatureSum);
  Serial.println(" °C");
  
  return temperatureSum;
}

void displayTemperature() {
  u8g2.clearBuffer();
  
  // Draw border
  u8g2.drawFrame(0, 0, 128, 64);
  
  // Title
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(25, 15, "Temperature");
  
  // Display temperature
  if (currentTemperature > -999.0) {
    // Temperature value
    u8g2.setFont(u8g2_font_ncenB18_tr);
    char tempStr[10];
    dtostrf(currentTemperature, 5, 1, tempStr);
    
    // Center the temperature reading
    int tempWidth = u8g2.getStrWidth(tempStr);
    int tempX = (128 - tempWidth - 15) / 2;  // Account for °C text
    u8g2.drawStr(tempX, 40, tempStr);
    
    // Degree symbol and C
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(tempX + tempWidth + 2, 40, "°C");
    
    // Status
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(35, 55, "Sensor Active");
  } else {
    // Error display
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(20, 35, "Sensor Error!");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(15, 50, "Check Connection");
  }
  
  u8g2.sendBuffer();
}

void displayWelcomeMessage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(15, 20, "OLED + DS18B20");
  u8g2.drawStr(30, 35, "Starting...");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(20, 50, "Temperature Sensor");
  u8g2.sendBuffer();
  
  Serial.println("Welcome message displayed!");
  delay(3000);
}
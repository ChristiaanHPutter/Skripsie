// ROBUST Button + DS18B20 System with Non-blocking OneWire and Proper Debouncing
#include <OneWire.h>

#define MOSFET_PIN 13      
#define BUTTON_PIN 4       
#define DS18B20_PIN 25     

// Temperature sensor setup
OneWire ds(DS18B20_PIN);

// MOSFET control variables
bool mosfetState = false;
bool lastButtonReading = HIGH;
bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;  // 50ms is more responsive than 200ms

// NON-BLOCKING Temperature sensor state machine
enum TempState {
  TEMP_IDLE,
  TEMP_START_CONVERSION,
  TEMP_WAIT_CONVERSION,
  TEMP_READ_DATA
};

TempState tempState = TEMP_IDLE;
float temperature = -999.0;
byte sensorAddr[8];
bool sensorFound = false;
unsigned long tempTimer = 0;
unsigned long lastTempRequest = 0;
const unsigned long TEMP_INTERVAL = 1000;  // Request temp every 1 second
const unsigned long CONVERSION_TIME = 750; // Max conversion time for 12-bit

// Status display timing
unsigned long lastStatusTime = 0;
const unsigned long STATUS_INTERVAL = 1000;

// Function to find and cache sensor address (run once)
bool findSensor() {
  ds.reset_search();
  if (ds.search(sensorAddr)) {
    if (OneWire::crc8(sensorAddr, 7) == sensorAddr[7]) {
      if (sensorAddr[0] == 0x10 || sensorAddr[0] == 0x28) {
        Serial.println("DS18B20 sensor found and cached!");
        return true;
      }
    }
  }
  Serial.println("DS18B20 sensor not found!");
  return false;
}

// NON-BLOCKING temperature reading state machine
void updateTemperature() {
  switch (tempState) {
    case TEMP_IDLE:
      // Check if it's time to start a new reading
      if (millis() - lastTempRequest >= TEMP_INTERVAL) {
        if (sensorFound) {
          tempState = TEMP_START_CONVERSION;
        }
        lastTempRequest = millis();
      }
      break;

    case TEMP_START_CONVERSION:
      // Start conversion (non-blocking)
      ds.reset();
      ds.select(sensorAddr);
      ds.write(0x44, 1);  // Start conversion
      tempTimer = millis();
      tempState = TEMP_WAIT_CONVERSION;
      break;

    case TEMP_WAIT_CONVERSION:
      // Wait for conversion to complete (non-blocking check)
      if (millis() - tempTimer >= CONVERSION_TIME) {
        tempState = TEMP_READ_DATA;
      }
      break;

    case TEMP_READ_DATA:
      // Read the temperature data
      byte data[9];
      ds.reset();
      ds.select(sensorAddr);
      ds.write(0xBE);

      // Read 9 bytes
      for (int i = 0; i < 9; i++) {
        data[i] = ds.read();
      }

      // Verify CRC
      if (OneWire::crc8(data, 8) == data[8]) {
        // Extract temperature
        int16_t raw = (data[1] << 8) | data[0];
        temperature = (float)raw / 16.0;
      } else {
        Serial.println("CRC Error in temperature reading");
      }

      tempState = TEMP_IDLE;
      break;
  }
}

// ROBUST button debouncing with state machine
bool readButtonDebounced() {
  bool reading = digitalRead(BUTTON_PIN);
  bool buttonPressed = false;

  // Check if button state changed
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  // Handle millis() rollover safely
  unsigned long timeDiff = millis() - lastDebounceTime;
  if (timeDiff >= DEBOUNCE_DELAY) {
    // Button state is stable
    if (reading != buttonState) {
      buttonState = reading;
      
      // Detect button press (HIGH to LOW transition)
      if (buttonState == LOW && lastButtonState == HIGH) {
        buttonPressed = true;
      }
      lastButtonState = buttonState;
    }
  }

  lastButtonReading = reading;
  return buttonPressed;
}

void setup(void) {
  Serial.begin(115200);
  Serial.println("=== ROBUST DS18B20 + MOSFET CONTROL ===");
  Serial.println("Non-blocking temperature reading with proper debouncing");
  
  // Initialize pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, LOW);
  
  // Find and cache sensor address
  Serial.println("Searching for DS18B20 sensor...");
  sensorFound = findSensor();
  
  if (sensorFound) {
    Serial.print("Sensor address: ");
    for (int i = 0; i < 8; i++) {
      Serial.printf("%02X", sensorAddr[i]);
      if (i < 7) Serial.print(":");
    }
    Serial.println();
  } else {
    Serial.println("ERROR: No DS18B20 sensor found!");
    Serial.println("Check wiring and 4.7k pull-up resistor");
  }
  
  Serial.println();
  Serial.println("=== IMPROVEMENTS ===");
  Serial.println("✓ Non-blocking temperature reading");
  Serial.println("✓ Robust button debouncing");
  Serial.println("✓ Sensor address caching");
  Serial.println("✓ No system freezing");
  Serial.println("✓ Excellent button responsiveness");
  Serial.println();
  
  // Initial display
  Serial.printf("Mosfet: %s                          Temperature: %.0f\n",
    mosfetState ? "ON " : "OFF", temperature);
}

void loop(void) {
  // NON-BLOCKING temperature reading - never blocks the system
  updateTemperature();
  
  // ROBUST button handling - always responsive
  if (readButtonDebounced()) {
    // Toggle MOSFET
    mosfetState = !mosfetState;
    digitalWrite(MOSFET_PIN, mosfetState ? HIGH : LOW);
    
    // Immediate status display
    Serial.printf("Mosfet: %s                          Temperature: %.0f\n",
      mosfetState ? "ON " : "OFF", temperature);
  }
  
  // Periodic status display
  if (millis() - lastStatusTime >= STATUS_INTERVAL) {
    Serial.printf("Mosfet: %s                          Temperature: %.0f\n",
      mosfetState ? "ON " : "OFF", temperature);
    lastStatusTime = millis();
  }
  
  // Small delay for system stability (non-blocking)
  delay(1);  // Much smaller delay - system stays responsive
}
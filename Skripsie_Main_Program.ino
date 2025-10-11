// ESP32 Sous Vide Controller - BLE Communication
// Hardware: ESP32 DevKit V1 with SH1106 OLED, 5 buttons, 6 LEDs, 3 DS18B20 sensors

#include <U8g2lib.h>
#include <Wire.h>
#include <OneWire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ==================== PIN DEFINITIONS ====================
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define BTN_1 32
#define BTN_2 33
#define BTN_3 27
#define BTN_4 26
#define BTN_5 0
#define LED_1 18
#define LED_2 19
#define LED_3 23
#define LED_4 13
#define LED_5 5
#define LED_6 15
#define ONEWIRE_BUS 25

// ==================== BLE UUIDs ====================
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a9"

// ==================== HARDWARE OBJECTS ====================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
OneWire ds(ONEWIRE_BUS);

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

byte sensor1[8] = {0x28, 0x41, 0x2A, 0xF9, 0x0F, 0x00, 0x00, 0xA4};
byte sensor2[8] = {0x28, 0xE0, 0x35, 0xF8, 0x0F, 0x00, 0x00, 0x53};
byte sensor3[8] = {0x28, 0x68, 0x60, 0xF8, 0x0F, 0x00, 0x00, 0x5F};

// ==================== CONSTANTS ====================
#define NUM_BUTTONS 5
#define NUM_CHAMBERS 3
#define DEBOUNCE_DELAY 50
#define TEMP_READ_INTERVAL 10000
#define TIMER_UPDATE_INTERVAL 60000
#define STATUS_SEND_INTERVAL 2000
#define TEMP_TOLERANCE 0.5
#define TEMP_MIN 40
#define TEMP_MAX 90
#define TIME_MIN 0
#define TIME_MAX 120

// ==================== STATE MACHINE ====================
enum SystemState {
  STATE_IDLE,
  STATE_SETTING,
  STATE_RUNNING,
  STATE_PAUSED
};

enum SettingMode {
  MODE_TEMPERATURE,
  MODE_TIME
};

// ==================== DATA STRUCTURES ====================
struct ChamberData {
  float targetTemp;
  float currentTemp;
  int targetTime;
  int remainingTime;
  bool isActive;
  bool atTemperature;
  bool timerStarted;
  unsigned long timerLastUpdate;
};

struct ButtonState {
  int pin;
  bool lastReading;
  bool state;
  bool lastState;
  unsigned long lastDebounceTime;
};

// ==================== GLOBAL VARIABLES ====================
SystemState currentState = STATE_IDLE;
SettingMode settingMode = MODE_TEMPERATURE;
int selectedChamber = 0;
ChamberData chambers[NUM_CHAMBERS];
const int buttonPins[NUM_BUTTONS] = {BTN_1, BTN_2, BTN_3, BTN_4, BTN_5};
ButtonState buttons[NUM_BUTTONS];
const int redLEDs[NUM_CHAMBERS] = {LED_1, LED_2, LED_3};
const int greenLEDs[NUM_CHAMBERS] = {LED_4, LED_5, LED_6};
unsigned long lastTempRead = 0;
unsigned long lastButtonPress = 0;
unsigned long lastStatusSend = 0;
bool displayNeedsUpdate = true;
String receivedData = "";

// ==================== FUNCTION DECLARATIONS ====================
void parseAppCommand(String command);
void sendBLEData(String data);
void initializeHardware();
void initializeChambers();
void initializeBLE();
void updateButtons();
bool readButtonDebounced(int index);
void handleButtonPress(int buttonIndex);
void updateTemperatures();
void updateTimers();
void updateLEDs();
void updateDisplay();
void handleStateIdle();
void handleStateSetting();
void handleStateRunning();
void adjustTemperature(int delta);
void adjustTime(int delta);
float readTemperatureFromSensor(byte addr[8]);
void sendStatusToApp();

// ==================== BLE CALLBACKS ====================
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        receivedData = rxValue;
        Serial.print("Received: ");
        Serial.println(receivedData);
        parseAppCommand(receivedData);
      }
    }
};

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== ESP32 Sous Vide Controller ===");
  
  initializeHardware();
  initializeChambers();
  initializeBLE();
  
  Serial.println("System ready!");
  displayNeedsUpdate = true;
}

// ==================== MAIN LOOP ====================
void loop() {
  // Handle BLE disconnection
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }
  
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  
  updateButtons();
  
  if (displayNeedsUpdate) {
    updateDisplay();
    displayNeedsUpdate = false;
  }
  
  unsigned long timeSinceButton = millis() - lastButtonPress;
  if (millis() - lastTempRead >= TEMP_READ_INTERVAL && timeSinceButton > 5000) {
    updateTemperatures();
    lastTempRead = millis();
    displayNeedsUpdate = true;
  }
  
  if (currentState == STATE_RUNNING) {
    updateTimers();
  }
  
  if (deviceConnected && millis() - lastStatusSend >= STATUS_SEND_INTERVAL) {
    sendStatusToApp();
    lastStatusSend = millis();
  }
  
  switch (currentState) {
    case STATE_IDLE: handleStateIdle(); break;
    case STATE_SETTING: handleStateSetting(); break;
    case STATE_RUNNING: handleStateRunning(); break;
    case STATE_PAUSED: break;
  }
  
  updateLEDs();
  delay(1);
}

// ==================== BLE FUNCTIONS ====================
void initializeBLE() {
  Serial.println("Initializing BLE...");
  
  BLEDevice::init("Multi-Chambered Sous Vide Cooker");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();
  pServer->getAdvertising()->start();
  
  Serial.println("BLE initialized - Multi-Chambered Sous Vide Cooker");
  Serial.println("Waiting for connection...");
}

void sendBLEData(String data) {
  if (deviceConnected) {
    pTxCharacteristic->setValue(data.c_str());
    pTxCharacteristic->notify();
  }
}

void parseAppCommand(String command) {
  command.trim();
  
  if (command.startsWith("SET:")) {
    command.remove(0, 4);
    
    int chamberIndex = 0;
    while (command.length() > 0 && chamberIndex < NUM_CHAMBERS) {
      int commaPos = command.indexOf(',');
      String chamberData = (commaPos == -1) ? command : command.substring(0, commaPos);
      
      int colon1 = chamberData.indexOf(':');
      int colon2 = chamberData.indexOf(':', colon1 + 1);
      
      if (colon1 > 0 && colon2 > 0) {
        int chamber = chamberData.substring(1, colon1).toInt() - 1;
        int temp = chamberData.substring(colon1 + 1, colon2).toInt();
        int time = chamberData.substring(colon2 + 1).toInt();
        
        if (chamber >= 0 && chamber < NUM_CHAMBERS) {
          chambers[chamber].targetTemp = temp;
          chambers[chamber].targetTime = time;
          
          Serial.print("Chamber ");
          Serial.print(chamber + 1);
          Serial.print(" set to ");
          Serial.print(temp);
          Serial.print("°C for ");
          Serial.print(time);
          Serial.println(" minutes");
        }
      }
      
      if (commaPos == -1) break;
      command.remove(0, commaPos + 1);
      chamberIndex++;
    }
    
    sendBLEData("ACK:Settings received\n");
    displayNeedsUpdate = true;
  }
}

void sendStatusToApp() {
  String status = "STATUS:";
  
  for (int i = 0; i < NUM_CHAMBERS; i++) {
    if (i > 0) status += ",";
    status += "C" + String(i + 1) + ":";
    status += String(chambers[i].currentTemp, 1) + ":";
    status += String((int)chambers[i].targetTemp) + ":";
    status += String(chambers[i].remainingTime) + ":";
    status += String(chambers[i].atTemperature ? 1 : 0) + ":";
    status += String(chambers[i].timerStarted ? 1 : 0);
  }
  
  status += "|STATE:";
  switch (currentState) {
    case STATE_IDLE: status += "IDLE"; break;
    case STATE_SETTING: status += "SETTING"; break;
    case STATE_RUNNING: status += "RUNNING"; break;
    case STATE_PAUSED: status += "PAUSED"; break;
  }
  
  status += "\n";
  sendBLEData(status);
}

// ==================== INITIALIZATION ====================
void initializeHardware() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  u8g2.begin();
  
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    buttons[i].pin = buttonPins[i];
    buttons[i].lastReading = HIGH;
    buttons[i].state = HIGH;
    buttons[i].lastState = HIGH;
    buttons[i].lastDebounceTime = 0;
  }
  
  for (int i = 0; i < NUM_CHAMBERS; i++) {
    pinMode(redLEDs[i], OUTPUT);
    pinMode(greenLEDs[i], OUTPUT);
    digitalWrite(redLEDs[i], LOW);
    digitalWrite(greenLEDs[i], LOW);
  }
  
  displayWelcomeScreen();
}

void displayWelcomeScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(10, 20, "Sous Vide");
  u8g2.drawStr(10, 40, "Controller");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(15, 55, "BLE Ready");
  u8g2.sendBuffer();
  delay(2000);
}

void initializeChambers() {
  for (int i = 0; i < NUM_CHAMBERS; i++) {
    chambers[i].targetTemp = 0;
    chambers[i].currentTemp = 0;
    chambers[i].targetTime = 0;
    chambers[i].remainingTime = 0;
    chambers[i].isActive = false;
    chambers[i].atTemperature = false;
    chambers[i].timerStarted = false;
    chambers[i].timerLastUpdate = 0;
  }
  
  lastTempRead = millis();
  lastButtonPress = millis();
}

// ==================== BUTTON HANDLING ====================
void updateButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (readButtonDebounced(i)) {
      handleButtonPress(i);
    }
  }
}

bool readButtonDebounced(int index) {
  bool reading = digitalRead(buttons[index].pin);
  bool buttonPressed = false;
  
  if (reading != buttons[index].lastReading) {
    buttons[index].lastDebounceTime = millis();
  }
  
  if ((millis() - buttons[index].lastDebounceTime) >= DEBOUNCE_DELAY) {
    if (reading != buttons[index].state) {
      buttons[index].state = reading;
      if (buttons[index].state == LOW && buttons[index].lastState == HIGH) {
        buttonPressed = true;
      }
      buttons[index].lastState = buttons[index].state;
    }
  }
  
  buttons[index].lastReading = reading;
  return buttonPressed;
}

void handleButtonPress(int buttonIndex) {
  lastButtonPress = millis();
  
  switch (buttonIndex) {
    case 0:
      if (currentState != STATE_RUNNING) {
        selectedChamber = (selectedChamber + 1) % NUM_CHAMBERS;
        currentState = STATE_SETTING;
      }
      break;
    case 1:
      if (currentState == STATE_SETTING) {
        settingMode = (settingMode == MODE_TEMPERATURE) ? MODE_TIME : MODE_TEMPERATURE;
      }
      break;
    case 2:
      if (currentState == STATE_SETTING) {
        if (settingMode == MODE_TEMPERATURE) adjustTemperature(-1);
        else adjustTime(-1);
      }
      break;
    case 3:
      if (currentState == STATE_SETTING) {
        if (settingMode == MODE_TEMPERATURE) adjustTemperature(1);
        else adjustTime(1);
      }
      break;
    case 4:
      if (currentState == STATE_RUNNING) {
        currentState = STATE_IDLE;
        for (int i = 0; i < NUM_CHAMBERS; i++) {
          chambers[i].isActive = false;
          chambers[i].atTemperature = false;
          chambers[i].timerStarted = false;
        }
        sendBLEData("EVENT:STOPPED\n");
      } else {
        currentState = STATE_RUNNING;
        for (int i = 0; i < NUM_CHAMBERS; i++) {
          if (chambers[i].targetTemp >= TEMP_MIN) {
            chambers[i].isActive = true;
            chambers[i].remainingTime = chambers[i].targetTime;
            chambers[i].timerStarted = false;
            chambers[i].timerLastUpdate = millis();
          }
        }
        sendBLEData("EVENT:STARTED\n");
      }
      break;
  }
  
  displayNeedsUpdate = true;
}

void adjustTemperature(int delta) {
  float newTemp = chambers[selectedChamber].targetTemp + delta;
  if (chambers[selectedChamber].targetTemp == 0 && delta > 0) newTemp = TEMP_MIN;
  else if (newTemp < TEMP_MIN && delta < 0) newTemp = 0;
  if (newTemp > TEMP_MAX) newTemp = TEMP_MAX;
  if (newTemp < 0) newTemp = 0;
  chambers[selectedChamber].targetTemp = newTemp;
}

void adjustTime(int delta) {
  int newTime = chambers[selectedChamber].targetTime + delta;
  if (newTime > TIME_MAX) newTime = TIME_MAX;
  if (newTime < TIME_MIN) newTime = TIME_MIN;
  chambers[selectedChamber].targetTime = newTime;
}

// ==================== SENSOR UPDATES ====================
float readTemperatureFromSensor(byte addr[8]) {
  byte data[12];
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);
  delay(800);
  ds.reset();
  ds.select(addr);
  ds.write(0xBE);
  for (int i = 0; i < 9; i++) data[i] = ds.read();
  byte MSB = data[1];
  byte LSB = data[0];
  float tempRead = ((MSB << 8) | LSB);
  return tempRead / 16.0;
}

void updateTemperatures() {
  chambers[0].currentTemp = readTemperatureFromSensor(sensor1);
  chambers[1].currentTemp = readTemperatureFromSensor(sensor2);
  chambers[2].currentTemp = readTemperatureFromSensor(sensor3);
  
  for (int i = 0; i < NUM_CHAMBERS; i++) {
    if (chambers[i].isActive && chambers[i].targetTemp >= TEMP_MIN) {
      if (abs(chambers[i].currentTemp - chambers[i].targetTemp) <= TEMP_TOLERANCE) {
        chambers[i].atTemperature = true;
        if (!chambers[i].timerStarted) {
          chambers[i].timerStarted = true;
          chambers[i].timerLastUpdate = millis();
          String event = "EVENT:TEMP_REACHED:C" + String(i + 1) + "\n";
          sendBLEData(event);
        }
      } else {
        chambers[i].atTemperature = false;
      }
    }
  }
}

void updateTimers() {
  for (int i = 0; i < NUM_CHAMBERS; i++) {
    if (chambers[i].isActive && chambers[i].timerStarted && chambers[i].targetTime > 0) {
      unsigned long currentTime = millis();
      if (currentTime - chambers[i].timerLastUpdate >= TIMER_UPDATE_INTERVAL) {
        if (chambers[i].remainingTime > 0) {
          chambers[i].remainingTime--;
          chambers[i].timerLastUpdate = currentTime;
          displayNeedsUpdate = true;
          
          if (chambers[i].remainingTime == 0) {
            String event = "EVENT:COMPLETE:C" + String(i + 1) + "\n";
            sendBLEData(event);
          }
        }
      }
    }
  }
}

void updateLEDs() {
  for (int i = 0; i < NUM_CHAMBERS; i++) {
    digitalWrite(redLEDs[i], chambers[i].isActive ? HIGH : LOW);
    digitalWrite(greenLEDs[i], chambers[i].atTemperature ? HIGH : LOW);
  }
}

void handleStateIdle() {}
void handleStateSetting() {}
void handleStateRunning() {}

// ==================== DISPLAY ====================
void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.drawFrame(0, 0, 128, 64);
  
  if (currentState == STATE_RUNNING) {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(35, 10, "RUNNING");
    u8g2.drawHLine(2, 12, 124);
    u8g2.setFont(u8g2_font_6x10_tr);
    
    for (int i = 0; i < NUM_CHAMBERS; i++) {
      int y = 24 + (i * 13);
      char line[25];
      
      if (chambers[i].isActive) {
        char timeStr[10];
        if (!chambers[i].timerStarted) sprintf(timeStr, "Heat");
        else if (chambers[i].remainingTime > 0) sprintf(timeStr, "%dm", chambers[i].remainingTime);
        else sprintf(timeStr, "Done");
        
        sprintf(line, "C%d:%d->%dC %s", 
                i + 1, (int)chambers[i].currentTemp,
                (int)chambers[i].targetTemp, timeStr);
      } else {
        sprintf(line, "C%d: OFF", i + 1);
      }
      u8g2.drawStr(8, y, line);
    }
  } else {
    u8g2.setFont(u8g2_font_ncenB14_tr);
    char chamberStr[15];
    sprintf(chamberStr, "CHAMBER %d", selectedChamber + 1);
    int chamberX = (128 - u8g2.getStrWidth(chamberStr)) / 2;
    u8g2.drawStr(chamberX, 16, chamberStr);
    u8g2.drawHLine(2, 18, 124);
    
    u8g2.setFont(u8g2_font_ncenB10_tr);
    char tempStr[20];
    if (chambers[selectedChamber].targetTemp == 0) sprintf(tempStr, "Temp: OFF");
    else sprintf(tempStr, "Temp: %dC", (int)chambers[selectedChamber].targetTemp);
    
    if (settingMode == MODE_TEMPERATURE) {
      int tempWidth = u8g2.getStrWidth(tempStr);
      u8g2.drawBox(6, 22, tempWidth + 4, 14);
      u8g2.setColorIndex(0);
      u8g2.drawStr(8, 34, tempStr);
      u8g2.setColorIndex(1);
    } else {
      u8g2.drawStr(8, 34, tempStr);
    }
    
    char timeStr[20];
    if (chambers[selectedChamber].targetTime == 0) sprintf(timeStr, "Time: OFF");
    else sprintf(timeStr, "Time: %dm", chambers[selectedChamber].targetTime);
    
    if (settingMode == MODE_TIME) {
      int timeWidth = u8g2.getStrWidth(timeStr);
      u8g2.drawBox(6, 38, timeWidth + 4, 14);
      u8g2.setColorIndex(0);
      u8g2.drawStr(8, 50, timeStr);
      u8g2.setColorIndex(1);
    } else {
      u8g2.drawStr(8, 50, timeStr);
    }
  }
  
  u8g2.sendBuffer();
}
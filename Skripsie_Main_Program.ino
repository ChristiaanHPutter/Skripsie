// Simple MOSFET toggle test
#define MOSFET_PIN 13   // MOSFET gate on GPIO13

// Variables for MOSFET control
bool mosfetState = false;           // Current MOSFET state (off by default)
unsigned long lastToggleTime = 0;
const unsigned long TOGGLE_INTERVAL = 3000;  // Toggle every 3 seconds

void setup(void) {
  // Initialize Serial Monitor for debugging
  Serial.begin(115200);
  Serial.println("MOSFET Test Starting...");
  
  // Initialize MOSFET control pin
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, LOW);  // Start with MOSFET off
  
  Serial.println("Setup complete. GPIO13 will toggle every 3 seconds.");
  Serial.println("Measure voltage between GPIO13 and GND.");
}

void loop(void) {
  // Check if it's time to toggle
  if (millis() - lastToggleTime >= TOGGLE_INTERVAL) {
    // Toggle MOSFET state
    mosfetState = !mosfetState;
    digitalWrite(MOSFET_PIN, mosfetState ? HIGH : LOW);
    
    Serial.print("GPIO13 is now: ");
    Serial.print(mosfetState ? "HIGH (3.3V)" : "LOW (0V)");
    Serial.print(" - MOSFET should be: ");
    Serial.println(mosfetState ? "ON" : "OFF");
    
    lastToggleTime = millis();
  }
  
  delay(100);  // Small delay for stability
}
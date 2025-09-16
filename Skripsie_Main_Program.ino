// Button controlled MOSFET - DEBUG VERSION with detailed messages
#define MOSFET_PIN 13   // MOSFET gate on GPIO13
#define BUTTON_PIN 4    // Button on GPIO4

// Variables for MOSFET control
bool mosfetState = false;           // Current MOSFET state (off by default)
bool lastButtonState = HIGH;       // Previous button state (HIGH due to pull-up)
bool buttonPressed = false;        // Flag to track if button was just pressed
unsigned long lastPressTime = 0;   // Time of last button press
const unsigned long DEBOUNCE_DELAY = 200;  // 200ms debounce delay

// Debug timing
unsigned long lastDebugTime = 0;
const unsigned long DEBUG_INTERVAL = 2000;  // Print debug info every 2 seconds

void setup(void) {
  // Initialize Serial Monitor for debugging
  Serial.begin(115200);
  Serial.println("=== MOSFET DEBUG VERSION ===");
  Serial.println("Detailed messages to help debug MOSFET switching");
  Serial.println();
  
  // Initialize button with internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.print("✓ GPIO4 (button) initialized as INPUT_PULLUP. Current state: ");
  Serial.println(digitalRead(BUTTON_PIN) ? "HIGH" : "LOW");
  
  // Initialize MOSFET control pin
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, LOW);  // Start with MOSFET off
  Serial.print("✓ GPIO13 (MOSFET) initialized as OUTPUT. Current state: ");
  Serial.println(digitalRead(MOSFET_PIN) ? "HIGH (3.3V)" : "LOW (0V)");
  
  Serial.println();
  Serial.println("=== WIRING CHECK ===");
  Serial.println("Button: One side → GPIO4, other side → GND");
  Serial.println("MOSFET gate → GPIO13");
  Serial.println("Test with multimeter: Measure voltage between GPIO13 and GND");
  Serial.println();
  
  Serial.println("=== MOSFET TEST ===");
  Serial.println("1. First verify GPIO13 voltage changes when button pressed");
  Serial.println("2. Then test MOSFET source-drain continuity");
  Serial.println("3. Make sure MOSFET is logic-level (works with 3.3V)");
  Serial.println();
  
  Serial.println("Press button to toggle MOSFET...");
  Serial.println("Debug info will print every 2 seconds");
  Serial.println();
}

void loop(void) {
  // Read current button state
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Simple button press detection with debounce
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    // Button was just pressed (HIGH to LOW transition)
    if (millis() - lastPressTime > DEBOUNCE_DELAY) {
      Serial.println();
      Serial.println("BUTTON PRESS DETECTED!");
      
      // Toggle MOSFET state
      mosfetState = !mosfetState;
      
      Serial.print("Setting GPIO13 to: ");
      Serial.println(mosfetState ? "HIGH" : "LOW");
      
      digitalWrite(MOSFET_PIN, mosfetState ? HIGH : LOW);
      
      // Verify the pin was actually set
      bool actualPinState = digitalRead(MOSFET_PIN);
      Serial.print("GPIO13 actual state after digitalWrite: ");
      Serial.println(actualPinState ? "HIGH (3.3V)" : "LOW (0V)");
      
      if (actualPinState != mosfetState) {
        Serial.println("WARNING: Pin state doesn't match what we tried to set!");
      }
      
      Serial.print("MOSFET should now be: ");
      Serial.println(mosfetState ? "ON (conducting)" : "OFF (not conducting)");
      
      Serial.println("TEST: Measure voltage between GPIO13 and GND with multimeter");
      Serial.println("TEST: Check source-drain continuity on MOSFET");
      Serial.println();
      
      lastPressTime = millis();
    }
  }
  
  // Track button state changes for debugging
  if (currentButtonState != lastButtonState) {
    Serial.print("Button state change: ");
    Serial.print(lastButtonState ? "HIGH" : "LOW");
    Serial.print(" -> ");
    Serial.println(currentButtonState ? "HIGH" : "LOW");
  }
  
  // Save current state for next iteration
  lastButtonState = currentButtonState;
  
  // Periodic debug info
  if (millis() - lastDebugTime >= DEBUG_INTERVAL) {
    Serial.println("--- PERIODIC DEBUG INFO ---");
    Serial.print("Button (GPIO4): ");
    Serial.print(digitalRead(BUTTON_PIN) ? "HIGH" : "LOW");
    Serial.print(" | MOSFET (GPIO13): ");
    Serial.print(digitalRead(MOSFET_PIN) ? "HIGH (3.3V)" : "LOW (0V)");
    Serial.print(" | Expected MOSFET state: ");
    Serial.println(mosfetState ? "ON" : "OFF");
    
    lastDebugTime = millis();
  }
  
  delay(10);  // Small delay for stability
}
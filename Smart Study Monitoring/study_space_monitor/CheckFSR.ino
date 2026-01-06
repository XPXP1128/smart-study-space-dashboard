// Simple FSR (Force Sensitive Resistor) Test
// Press the sensor to see readings!

#define FSR_PIN 34  // Analog pin for FSR

void setup() {
  Serial.begin(9600);
  pinMode(FSR_PIN, INPUT);
  
  Serial.println("===========================");
  Serial.println("FSR Sensor Test");
  Serial.println("===========================");
  Serial.println("Press the FSR sensor!");
  Serial.println();
  
  delay(2000);
}

void loop() {
  // Read FSR value (0-4095 on ESP32)
  int fsrReading = analogRead(FSR_PIN);
  
  // Print reading
  Serial.print("FSR Value: ");
  Serial.print(fsrReading);
  Serial.print("  |  ");
  
  // Visual indicator
  if (fsrReading < 50) {
    Serial.println("⚪ NO PRESSURE");
  } else {
    Serial.println("🔴🔴 VERY HARD PRESS!");
  }
  
  // Progress bar
  Serial.print("[");
  int bars = fsrReading / 100;
  for (int i = 0; i < bars && i < 40; i++) {
    Serial.print("=");
  }
  Serial.println("]");
  Serial.println();
  
  delay(500); // Update twice per second
}
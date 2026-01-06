// Simple Ultrasonic Sensor Test
// No other sensors needed!

#define TRIG_PIN 18
#define ECHO_PIN 16

void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  Serial.println("===========================");
  Serial.println("Ultrasonic Sensor Test");
  Serial.println("===========================");
  Serial.println("Wave your hand in front!");
  Serial.println();
}

void loop() {
  // Send trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read echo pulse
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  // Calculate distance
  if (duration == 0) {
    Serial.println("❌ No object detected (out of range)");
  } else {
    float distance = duration * 0.034 / 2.0;
    
    // Print with visual indicator
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.print(" cm");
    
    // Visual feedback
    if (distance < 10) {
      Serial.println(" 🔴 VERY CLOSE!");
    } else if (distance < 30) {
      Serial.println(" 🟡 CLOSE");
    } else if (distance < 100) {
      Serial.println(" 🟢 MEDIUM");
    } else {
      Serial.println(" ⚪ FAR");
    }
  }
  
  delay(500); // Update twice per second
}
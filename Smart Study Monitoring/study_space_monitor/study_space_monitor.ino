#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ===================================================
// SECTION 1: CONFIGURATION - CHANGE THESE VALUES!
// ===================================================

// WiFi credentials - PUT YOUR WIFI HERE!
#define WIFI_SSID "cslab"
#define WIFI_PASSWORD "aksesg31"

// Firebase credentials - PUT YOUR FIREBASE INFO HERE!
#define API_KEY "AIzaSyCRZ7WkCXFl2QGLWDlJAHgWVkiARecHQ04"
#define DATABASE_URL "https://smart-study-space-b59cb-default-rtdb.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL "esp32@studyspace.com"
#define USER_PASSWORD "Study2024!ESP32"

// ===================================================
// SECTION 2: PIN DEFINITIONS (Don't change unless wiring different)
// ===================================================
#define LIGHT_SENSOR_PIN 34    // LDR analog pin
#define FSR_PIN 34             // FSR analog pin
#define TRIG_PIN 18            // Ultrasonic trigger
#define ECHO_PIN 16            // Ultrasonic echo
#define RED_PIN 26             // RGB LED Red
#define GREEN_PIN 25           // RGB LED Green
#define YELLOW_PIN 27            // RGB LED Yellow

// ===================================================
// SECTION 3: OLED DISPLAY SETTINGS
// ===================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===================================================
// SECTION 4: FIREBASE OBJECTS
// ===================================================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ===================================================
// SECTION 5: GLOBAL VARIABLES
// ===================================================
unsigned long sendDataPrevMillis = 0;
const unsigned long sendDataInterval = 10000; // Send every 10 seconds
bool signupOK = false;
String spaceId = "space_001"; // Change if multiple units

// Sensor thresholds - adjust based on your environment
const int LIGHT_THRESHOLD_LOW = 500;
const int LIGHT_THRESHOLD_HIGH = 3000;
const int FSR_THRESHOLD = 200;
const int DISTANCE_THRESHOLD = 50; // cm

// ===================================================
// SECTION 6: SETUP FUNCTION (Runs once at startup)
// ===================================================
void setup() {
  // Start serial communication
  Serial.begin(115200);
  Serial.println();
  Serial.println("=================================");
  Serial.println("Smart Study Space Monitor");
  Serial.println("=================================");
  
  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  
  // Turn off RGB LED initially
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);
  
  // Initialize OLED display
  Serial.println("Initializing OLED display...");
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("ERROR: SSD1306 allocation failed!");
    Serial.println("Check OLED connections:");
    Serial.println("- VCC to 3.3V");
    Serial.println("- GND to GND");
    Serial.println("- SDA to GPIO 21");
    Serial.println("- SCL to GPIO 22");
    while(1); // Stop here
  }
  
  Serial.println("OLED initialized successfully!");
  
  // Display startup message
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Study Space");
  display.println("Monitor v1.0");
  display.println("---------------");
  display.println("Starting...");
  display.display();
  delay(2000);
  
  // Connect to WiFi
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Connecting WiFi");
  display.println(WIFI_SSID);
  display.display();
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("ERROR: WiFi connection failed!");
    Serial.println("Please check:");
    Serial.println("1. WiFi SSID is correct");
    Serial.println("2. WiFi password is correct");
    Serial.println("3. WiFi is 2.4GHz (not 5GHz)");
    Serial.println("4. Router is working");
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("WiFi FAILED!");
    display.println("Check:");
    display.println("- SSID");
    display.println("- Password");
    display.println("- 2.4GHz");
    display.display();
    
    while(1); // Stop here
  }
  
  Serial.println();
  Serial.println("WiFi connected successfully!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("WiFi Connected!");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
  delay(2000);
  
  // Configure Firebase
  Serial.println("Configuring Firebase...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  
  // Initialize Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  Serial.println("Firebase initialized!");
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Firebase Ready!");
  display.display();
  delay(2000);
  
  Serial.println("=================================");
  Serial.println("System ready! Starting monitoring...");
  Serial.println("=================================");
}

// ===================================================
// SECTION 7: MAIN LOOP (Runs repeatedly)
// ===================================================
void loop() {
  // Read all sensors
  int lightValue = analogRead(LIGHT_SENSOR_PIN);
  int fsrValue = analogRead(FSR_PIN);
  float distance = readUltrasonicDistance();
  
  // Determine occupancy (either FSR pressure OR close distance)
  bool occupied = (fsrValue > FSR_THRESHOLD) || (distance < DISTANCE_THRESHOLD);
  
  // Determine light quality
  String lightStatus;
  if (lightValue < LIGHT_THRESHOLD_LOW) {
    lightStatus = "Too Dark";
  } else if (lightValue > LIGHT_THRESHOLD_HIGH) {
    lightStatus = "Too Bright";
  } else {
    lightStatus = "Optimal";
  }
  
  // Calculate quality score (0-100)
  int qualityScore = calculateQualityScore(lightValue, occupied);
  
  // Update local actuators
  updateRGBLED(occupied, lightStatus);
  updateOLEDDisplay(occupied, lightValue, distance, lightStatus, qualityScore);
  
  // Print to Serial Monitor for debugging
  Serial.println("--- Sensor Readings ---");
  Serial.print("Light: "); Serial.print(lightValue); 
  Serial.print(" ("); Serial.print(lightStatus); Serial.println(")");
  Serial.print("FSR: "); Serial.println(fsrValue);
  Serial.print("Distance: "); Serial.print(distance); Serial.println(" cm");
  Serial.print("Occupied: "); Serial.println(occupied ? "YES" : "NO");
  Serial.print("Quality Score: "); Serial.println(qualityScore);
  Serial.println();

// ===================================================
// SECTION 8: HELPER FUNCTIONS
// ===================================================

// Read distance from ultrasonic sensor
float readUltrasonicDistance() {
  // Send trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read echo pulse
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  
  // If timeout, return max distance
  if (duration == 0) {
    return 999.0;
  }
  
  // Calculate distance in cm
  float distance = duration * 0.034 / 2.0;
  return distance;
}

// Calculate overall quality score (0-100)
int calculateQualityScore(int lightValue, bool occupied) {
  int score = 100;
  
  // Deduct points for poor lighting
  if (lightValue < LIGHT_THRESHOLD_LOW) {
    score -= 30; // Too dark
  } else if (lightValue > LIGHT_THRESHOLD_HIGH) {
    score -= 20; // Too bright
  }
  
  // Deduct points if occupied
  if (occupied) {
    score -= 50; // Space not available
  }
  
  return max(0, score); // Keep score between 0-100
}

// Update RGB LED based on status
void updateRGBLED(bool occupied, String lightStatus) {
  if (occupied) {
    // RED - Space occupied
    analogWrite(RED_PIN, 255);
    analogWrite(GREEN_PIN, 0);
    analogWrite(YELLOW_PIN, 0);
  } else if (lightStatus == "Too Dark" || lightStatus == "Too Bright") {
    // YELLOW - Available but poor lighting
    analogWrite(RED_PIN, 255);
    analogWrite(GREEN_PIN, 255);
    analogWrite(YELLOW_PIN, 0);
  } else {
    // GREEN - Available with good lighting
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 255);
    analogWrite(YELLOW_PIN, 0);
  }
}

// Update OLED display with current information
void updateOLEDDisplay(bool occupied, int light, float distance, String lightStatus, int quality) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  
  // Title
  display.println("Study Space");
  display.println("------------------");
  
  // Status
  display.print("Status: ");
  if (occupied) {
    display.println("OCCUPIED");
  } else {
    display.println("AVAILABLE");
  }
  
  // Light info
  display.print("Light: ");
  display.println(lightStatus);
  display.print("Value: ");
  display.println(light);
  
  // Distance
  display.print("Dist: ");
  display.print(distance, 1);
  display.println(" cm");
  
  // Quality score
  display.print("Quality: ");
  display.print(quality);
  display.println("%");
  
  display.display();
}

// Send data to Firebase Realtime Database
void sendDataToFirebase(bool occupied, int light, float distance, String lightStatus, int quality) {
  Serial.println("Sending data to Firebase...");
  
  // Create JSON object
  FirebaseJson json;
  json.set("occupied", occupied);
  json.set("lightLevel", light);
  json.set("distance", distance);
  json.set("lightStatus", lightStatus);
  json.set("qualityScore", quality);
  json.set("lastUpdated", (int)millis());
  json.set("timestamp/.sv", "timestamp"); // Server timestamp
  
  // Send to current data path
  String currentPath = "studySpaces/" + spaceId + "/current";
  if (Firebase.RTDB.setJSON(&fbdo, currentPath.c_str(), &json)) {
    Serial.println("✓ Data sent successfully to: " + currentPath);
    signupOK = true;
  } else {
    Serial.println("✗ Failed to send data!");
    Serial.println("Reason: " + fbdo.errorReason());
  }
  
  // Also save to history for analytics
  String historyPath = "studySpaces/" + spaceId + "/history/" + String(millis());
  Firebase.RTDB.setJSON(&fbdo, historyPath.c_str(), &json);
  
  Serial.println("Next update in 10 seconds...");
  Serial.println();
}

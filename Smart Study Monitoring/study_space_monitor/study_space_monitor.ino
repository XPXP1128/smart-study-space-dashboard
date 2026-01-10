#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>   


// Firebase
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"


// ---------------- OLED ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


// ---------------- WiFi credentials ----------------
#define WIFI_SSID "cslab"
#define WIFI_PASSWORD "aksesg31"

// ---------------- Firebase credentials ----------------
#define API_KEY "AIzaSyCRZ7WkCXFl2QGLWDlJAHgWVkiARecHQ04"
#define DATABASE_URL "https://smart-study-space-b59cb-default-rtdb.asia-southeast1.firebasedatabase.app/"


#define USER_EMAIL "esp32@studyspace.com"
#define USER_PASSWORD "Study2024!ESP32"


// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// ---------------- PINS ----------------
#define TRIG_PIN 18
#define ECHO_PIN 16


#define FSR_PIN 34
#define LDR_AO_PIN 35
#define LDR_DO_PIN 32   // 0=Bright, 1=Low light


#define LED_RED 25
#define LED_GREEN 26
#define LED_YELLOW 27


// ---------------- THRESHOLDS ----------------
int FSR_THRESHOLD = 1000;
float RESERVED_DIST_THRESHOLD = 30.0;
int RESERVED_CONFIRM_COUNT = 3;


// ---------------- STATE ----------------
int reservedCounter = 0;


// Firebase send interval
unsigned long lastFirebaseSend = 0;
const unsigned long firebaseInterval = 2000; // send every 2 seconds


// History logging interval
unsigned long lastLogPush = 0;
const unsigned long logInterval = 30000; // push history every 30 seconds


// ---------------- ULTRASONIC FUNCTION ----------------
float getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);


  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  float distance = duration * 0.034 / 2;


  if (duration == 0) return 999;
  return distance;
}


// ---------------- WiFi Connect ----------------
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");


  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    Serial.print(".");
    delay(500);
    tries++;
  }


  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi Failed. Check SSID/PASS.");
  }
}


// ---------------- NTP TIME SYNC ----------------
void syncTime() {
  // Malaysia UTC+8
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("✅ NTP syncing...");


  time_t nowTime = time(nullptr);
  int retries = 0;


  while (nowTime < 100000 && retries < 20) { // prevents 1970 invalid time
    delay(500);
    Serial.print(".");
    nowTime = time(nullptr);
    retries++;
  }


  if (nowTime >= 100000) {
    Serial.println("\n✅ Time synced successfully!");
  } else {
    Serial.println("\n⚠️ Time sync failed (will still work but logs may fail).");
  }
}


// ---------------- Setup ----------------
void setup() {
  Serial.begin(9600);


  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);


  pinMode(LDR_DO_PIN, INPUT);


  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);


  analogSetAttenuation(ADC_11db);


  // OLED init
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found. Try 0x3D");
    while (1);
  }


  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Starting...");
  display.display();


  // WiFi connect
  connectWiFi();


  // NTP time sync (needed for dashboard analytics)
  if (WiFi.status() == WL_CONNECTED) {
    syncTime();
  }


  // Firebase config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;


  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;


  config.token_status_callback = tokenStatusCallback;


  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);


  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi + Firebase");
  display.println("Connecting...");
  display.display();
  delay(1000);
}


// ---------------- Main Loop ----------------
void loop() {


  // READ sensors
  float distance = getDistanceCM();
  int fsrValue = analogRead(FSR_PIN);
  int ldrAO = analogRead(LDR_AO_PIN);
  int ldrDO = digitalRead(LDR_DO_PIN);  // 0=Bright, 1=Low light


  // LIGHT STATUS
  bool isLowLight = (ldrDO == 1);
  String lightStatus = isLowLight ? "LOW LIGHT" : "BRIGHT";


  // Yellow LED static ON for low light
  digitalWrite(LED_YELLOW, isLowLight ? HIGH : LOW);


  // SEAT STATUS (3 states)
  bool fsrOccupied = (fsrValue > FSR_THRESHOLD);
  bool deskHasItem = (distance < RESERVED_DIST_THRESHOLD);


  if (deskHasItem && !fsrOccupied) reservedCounter++;
  else reservedCounter = 0;


  bool reservedFinal = (reservedCounter >= RESERVED_CONFIRM_COUNT);


  String seatStatus;
  if (fsrOccupied) seatStatus = "OCCUPIED";
  else if (reservedFinal) seatStatus = "RESERVED";
  else seatStatus = "AVAILABLE";


  // Red/Green LEDs
  if (seatStatus == "OCCUPIED") {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
  } else if (seatStatus == "AVAILABLE") {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
  } else { // RESERVED
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
  }


  // OLED display
  display.clearDisplay();
  display.setCursor(0, 0);


  display.print("Dist: "); display.print(distance); display.println("cm");
  display.print("FSR : "); display.println(fsrValue);
  display.print("Light: "); display.println(lightStatus);


  display.println("----------------");
  display.print("Seat: ");
  display.println(seatStatus);


  display.display();


  // Real epoch time in ms (for dashboard filtering)
  //long epochMs = (long)time(nullptr) * 1000;
  uint64_t epochMs = (uint64_t)time(nullptr) * 1000ULL;
  double ts = (double)epochMs; // RTDB stores numbers as double safely for this range


  // ---------------- Firebase Upload ----------------
  unsigned long now = millis();
  if (Firebase.ready() && (now - lastFirebaseSend >= firebaseInterval)) {
    lastFirebaseSend = now;


    String path = "/USMLibrary/Desk01/";


    bool ok = true;
    ok &= Firebase.RTDB.setString(&fbdo, path + "seatStatus", seatStatus);
    ok &= Firebase.RTDB.setFloat(&fbdo, path + "distance_cm", distance);
    ok &= Firebase.RTDB.setInt(&fbdo, path + "fsrValue", fsrValue);


    ok &= Firebase.RTDB.setString(&fbdo, path + "lightStatus", lightStatus);
    ok &= Firebase.RTDB.setInt(&fbdo, path + "ldrAO", ldrAO);
    ok &= Firebase.RTDB.setInt(&fbdo, path + "ldrDO", ldrDO);


    // UpdatedAt now uses real epoch time
    ok &= Firebase.RTDB.setDouble(&fbdo, path + "updatedAt", ts);


    if (ok) {
      Serial.println("✅ Firebase Updated!");


      // HISTORY LOGGING (push every 30 sec)
      if (now - lastLogPush >= logInterval) {
        lastLogPush = now;


        FirebaseJson logJson;
        logJson.set("seatStatus", seatStatus);
        logJson.set("distance_cm", distance);
        logJson.set("fsrValue", fsrValue);
        logJson.set("lightStatus", lightStatus);
        logJson.set("ldrAO", ldrAO);
        logJson.set("ldrDO", ldrDO);


        // Store real epoch time for dashboard analytics
        logJson.set("updatedAt", ts);
        logJson.set("timestamp", ts);


        Serial.print("epochMs = ");
        Serial.println((unsigned long long)epochMs);

        if (Firebase.RTDB.pushJSON(&fbdo, "/USMLibrary/Desk01Logs", &logJson)) {
          Serial.println("📌 ✅ Log pushed to Desk01Logs!");
        } else {
          Serial.print("📌 ❌ Log push error: ");
          Serial.println(fbdo.errorReason());
        }
      }

    } else {
      Serial.print("❌ Firebase Error: ");
      Serial.println(fbdo.errorReason());
    }
  }

  delay(500);
}


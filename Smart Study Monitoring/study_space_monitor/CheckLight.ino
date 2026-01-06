#define FSR_PIN 34
#define LDR_AO 35
#define LDR_DO 32

void setup() {
  Serial.begin(9600);
  pinMode(LDR_DO, INPUT);
}

void loop() {
  int fsr = analogRead(FSR_PIN);
  int lightAnalog = analogRead(LDR_AO);
  int lightDigital = digitalRead(LDR_DO);

  Serial.print("FSR: ");
  Serial.print(fsr);

  Serial.print(" | LDR AO: ");
  Serial.print(lightAnalog);

  Serial.print(" | LDR DO: ");
  Serial.println(lightDigital);

  delay(500);
}
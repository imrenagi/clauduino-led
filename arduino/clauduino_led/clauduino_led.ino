#include "arduino_secrets.h"

void setup() {
  Serial.begin(115200);
  // Give the native USB Serial a moment on Uno R4 WiFi.
  delay(1500);
  Serial.println(F("clauduino-led: boot"));
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    Serial.println(F("clauduino-led: alive"));
  }
}

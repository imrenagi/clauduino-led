#include <WiFiS3.h>
#include "arduino_secrets.h"

static void connectWiFi() {
  Serial.print(F("WiFi: connecting to "));
  Serial.println(SECRET_WIFI_SSID);

  while (WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD) != WL_CONNECTED) {
    Serial.println(F("WiFi: retry in 3s"));
    delay(3000);
  }

  Serial.print(F("WiFi: connected, IP="));
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("clauduino-led: boot"));
  connectWiFi();
}

void loop() {
  // If we drop WiFi, reconnect.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi: lost, reconnecting"));
    connectWiFi();
  }
  delay(1000);
}

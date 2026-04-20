#include <WiFiS3.h>
#include "arduino_secrets.h"

static void connectWiFi() {
  Serial.print(F("WiFi: connecting to "));
  Serial.println(SECRET_WIFI_SSID);

  while (WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD) != WL_CONNECTED) {
    Serial.println(F("WiFi: retry in 3s"));
    delay(3000);
  }

  // WiFi.begin() returns WL_CONNECTED as soon as association succeeds,
  // but DHCP may still be in progress. Wait (up to ~10s) for a valid IP.
  IPAddress ip = WiFi.localIP();
  unsigned long waited = 0;
  while (ip == IPAddress(0, 0, 0, 0) && waited < 10000) {
    delay(500);
    waited += 500;
    ip = WiFi.localIP();
  }

  Serial.print(F("WiFi: connected, IP="));
  Serial.println(ip);
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

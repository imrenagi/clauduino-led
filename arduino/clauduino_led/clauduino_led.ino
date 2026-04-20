#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include "arduino_secrets.h"

static WiFiClient wifiClient;
static MqttClient mqttClient(wifiClient);

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

static void connectMQTT() {
  Serial.print(F("MQTT: connecting to "));
  Serial.print(SECRET_MQTT_HOST);
  Serial.print(':');
  Serial.println(SECRET_MQTT_PORT);

  while (!mqttClient.connect(SECRET_MQTT_HOST, SECRET_MQTT_PORT)) {
    Serial.print(F("MQTT: failed, error="));
    Serial.println(mqttClient.connectError());
    delay(3000);
  }
  Serial.println(F("MQTT: connected"));

  mqttClient.subscribe("clauduino/led/#");
  Serial.println(F("MQTT: subscribed to clauduino/led/#"));
}

static void onMqttMessage(int messageSize) {
  String topic = mqttClient.messageTopic();
  Serial.print(F("MQTT rx: topic="));
  Serial.print(topic);
  Serial.print(F(" size="));
  Serial.println(messageSize);
  // Drain payload (we don't use it yet).
  while (mqttClient.available()) {
    mqttClient.read();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("clauduino-led: boot"));
  connectWiFi();

  mqttClient.onMessage(onMqttMessage);
  connectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); }
  if (!mqttClient.connected())       { connectMQTT(); }
  mqttClient.poll();
}

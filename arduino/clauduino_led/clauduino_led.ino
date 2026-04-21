#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <FastLED.h>
#include "arduino_secrets.h"
#include "animations.h"
#include "buzzer.h"
#include "songs.h"

static CRGB leds[NUM_LEDS];
static AnimationEngine engine;
static constexpr uint8_t BUZZER_PIN = 8;        // D8
static SongPlayer player;
static constexpr const Song& SONG_FOR_STOP = HAPPY_BIRTHDAY;  // change me to swap songs

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
  String topicStr = mqttClient.messageTopic();
  while (mqttClient.available()) { mqttClient.read(); }  // drain payload

  Event e = eventFromTopic(topicStr.c_str());
  Serial.print(F("MQTT rx: topic="));
  Serial.print(topicStr);
  Serial.print(F(" event="));
  Serial.println(static_cast<uint8_t>(e));

  if (e != Event::None) {
    engine.start(e);
    if (e == Event::Stop) player.play(SONG_FOR_STOP);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("clauduino-led: boot"));

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(96);          // conservative default; tune later
  engine.begin(leds);
  player.begin(BUZZER_PIN);

  connectWiFi();
  mqttClient.onMessage(onMqttMessage);
  connectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); }
  if (!mqttClient.connected())       { connectMQTT(); }
  mqttClient.poll();
  engine.tick();
  player.tick();
}

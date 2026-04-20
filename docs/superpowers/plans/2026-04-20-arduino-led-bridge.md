# Arduino LED Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire up three Claude Code hooks to MQTT, subscribe from an Arduino Uno R4 WiFi, and play a distinct WS2812 animation per event (Stop / SubagentStop / Notification).

**Architecture:** Hook shells out to `mosquitto_pub` against the local broker (already provisioned). Arduino connects to the broker over WiFi, subscribes to `clauduino/led/#`, and dispatches to a non-blocking animation engine driving 46 WS2812 LEDs on pin D6.

**Tech Stack:** Mosquitto (Docker), Claude Code hooks (settings.json), Arduino IDE, `WiFiS3`, `ArduinoMqttClient`, `FastLED`.

**Spec:** `docs/superpowers/specs/2026-04-20-arduino-led-bridge-design.md`

---

## File structure (locked in before tasks)

| Path | Purpose |
|---|---|
| `Makefile` | Add `trigger-stop` / `trigger-subagent` / `trigger-notify` for hardware testing without Claude |
| `.gitignore` | Add `arduino/clauduino_led/arduino_secrets.h` |
| `arduino/clauduino_led/clauduino_led.ino` | Main sketch: WiFi + MQTT connect, message dispatch, engine pump |
| `arduino/clauduino_led/animations.h` | `Event` enum, `AnimationEngine` class interface, `NUM_LEDS` / `DATA_PIN` constants |
| `arduino/clauduino_led/animations.cpp` | Three animation state machines + `AnimationEngine` dispatcher |
| `arduino/clauduino_led/arduino_secrets.h.example` | Tracked template for WiFi + broker config |
| `arduino/clauduino_led/arduino_secrets.h` | Real creds, gitignored |
| `.claude/settings.json` | Three hook entries (Stop / SubagentStop / Notification) |
| `CLAUDE.md` | Setup, hardware, and usage updates |

One responsibility per file: the `.ino` knows about transport, `animations.*` knows about LEDs, the hook config knows about Claude events, the spec/plan know about intent.

---

## Testing strategy

Two kinds of tests here:

1. **Mac-side, automatable:** the Makefile triggers, the hook publish command, and the `.claude/settings.json` wiring can all be verified by round-tripping through the broker (subscribe, fire the trigger, confirm the topic+payload lands).
2. **Arduino-side, manual:** firmware is verified visually on the real strip. Each Arduino task has a "Manual verification" step with exact observable criteria. We don't invent a software emulator — it's wasted effort for a three-animation project and the real strip is the oracle anyway.

The Makefile targets from Task 1 are the test harness for every Arduino task: you trigger an event with `make trigger-stop` and check what the strip does.

---

## Task 1: Add Makefile trigger targets

**Files:**
- Modify: `Makefile`

Why first: these targets are the verification harness for every subsequent Arduino task. No Arduino code can be verified without them.

- [ ] **Step 1.1: Write the verifying sub/pub check**

In a terminal, make sure the broker is up:
```bash
make up
make ps
```
Expected: container status `Up ... (healthy)`.

- [ ] **Step 1.2: Add trigger targets to the Makefile**

Append these three targets above the `clean` target in `Makefile`:

```makefile
trigger-stop: ## Simulate a Claude Stop event
	docker exec $(CONTAINER) mosquitto_pub -h localhost -t 'clauduino/led/stop' -m ''

trigger-subagent: ## Simulate a Claude SubagentStop event
	docker exec $(CONTAINER) mosquitto_pub -h localhost -t 'clauduino/led/subagent_stop' -m ''

trigger-notify: ## Simulate a Claude Notification event
	docker exec $(CONTAINER) mosquitto_pub -h localhost -t 'clauduino/led/notification' -m ''
```

Also add these three targets to the `.PHONY` line:
```makefile
.PHONY: help up down restart ps logs sub pub smoke clean trigger-stop trigger-subagent trigger-notify
```

- [ ] **Step 1.3: Verify help lists the new targets**

Run: `make help`
Expected: output includes lines for `trigger-stop`, `trigger-subagent`, `trigger-notify` with their descriptions.

- [ ] **Step 1.4: Verify each trigger publishes the right topic+payload**

Terminal A:
```bash
docker exec clauduino-mqtt mosquitto_sub -h localhost -t 'clauduino/led/#' -v
```

Terminal B (run in sequence):
```bash
make trigger-stop
make trigger-subagent
make trigger-notify
```

Expected in Terminal A:
```
clauduino/led/stop
clauduino/led/subagent_stop
clauduino/led/notification
```

Each line has an empty payload after the topic. (Ctrl-C to stop the subscriber.)

- [ ] **Step 1.5: Commit**

```bash
git add Makefile
git commit -m "Add Makefile trigger targets for hardware testing"
```

---

## Task 2: Gitignore arduino_secrets.h before creating it

**Files:**
- Modify: `.gitignore`

Why before Task 3: if we create the secrets file first and forget to gitignore, a reflex `git add .` can leak WiFi credentials. Gitignore first, create second.

- [ ] **Step 2.1: Append the secrets rule**

Add to `.gitignore`, in the tooling block near the bottom:
```
arduino/clauduino_led/arduino_secrets.h
```

- [ ] **Step 2.2: Verify git treats the path as ignored once created**

```bash
mkdir -p arduino/clauduino_led
touch arduino/clauduino_led/arduino_secrets.h
git check-ignore -v arduino/clauduino_led/arduino_secrets.h
rm arduino/clauduino_led/arduino_secrets.h   # we'll really create it in Task 3
```

Expected output from `check-ignore`: a line referencing the `.gitignore` rule you added. Non-empty output means the pattern matches.

- [ ] **Step 2.3: Commit**

```bash
git add .gitignore
git commit -m "Gitignore arduino_secrets.h to prevent credential leaks"
```

---

## Task 3: Arduino project skeleton + secrets template

**Files:**
- Create: `arduino/clauduino_led/clauduino_led.ino`
- Create: `arduino/clauduino_led/arduino_secrets.h.example`
- Create: `arduino/clauduino_led/arduino_secrets.h` (local, gitignored)

This task gets the sketch building on the board with a Serial heartbeat. No WiFi, no MQTT, no LEDs yet — just prove the toolchain works.

- [ ] **Step 3.1: Install board support and libraries**

In Arduino IDE:
1. Boards Manager → install **"Arduino UNO R4 Boards"** by Arduino.
2. Library Manager → install **"ArduinoMqttClient"** by Arduino and **"FastLED"** by Daniel Garcia.

(These are runtime dependencies we'll `#include` in later tasks. Installing now avoids mid-task interruptions.)

- [ ] **Step 3.2: Create `arduino_secrets.h.example` (tracked template)**

File: `arduino/clauduino_led/arduino_secrets.h.example`
```cpp
#pragma once

// Copy this file to arduino_secrets.h and fill in real values.
// arduino_secrets.h is gitignored.

#define SECRET_WIFI_SSID     "YourWiFiName"
#define SECRET_WIFI_PASSWORD "YourWiFiPassword"

// The Mac running Docker/Mosquitto, reachable on the LAN.
// Use the Mac's LAN IP, NOT 127.0.0.1 (the Arduino is a separate device).
#define SECRET_MQTT_HOST     "192.168.1.100"
#define SECRET_MQTT_PORT     1883
```

- [ ] **Step 3.3: Create local `arduino_secrets.h` with real values**

```bash
cp arduino/clauduino_led/arduino_secrets.h.example arduino/clauduino_led/arduino_secrets.h
```

Open `arduino/clauduino_led/arduino_secrets.h` in Arduino IDE (or a text editor) and fill in your real WiFi SSID/password and your Mac's LAN IP (find it with `ipconfig getifaddr en0` on macOS).

- [ ] **Step 3.4: Create the minimal sketch**

File: `arduino/clauduino_led/clauduino_led.ino`
```cpp
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
```

- [ ] **Step 3.5: Upload and verify**

In Arduino IDE:
1. Open `arduino/clauduino_led/clauduino_led.ino`.
2. Tools → Board → Arduino UNO R4 WiFi.
3. Tools → Port → select the connected board.
4. Click Upload.
5. Open Serial Monitor at 115200 baud.

Expected: `clauduino-led: boot` then `clauduino-led: alive` every 2 seconds.

- [ ] **Step 3.6: Confirm gitignore holds**

```bash
git status
```
Expected: `arduino/clauduino_led/arduino_secrets.h` must NOT appear. `arduino_secrets.h.example` and `clauduino_led.ino` should appear as untracked.

- [ ] **Step 3.7: Commit**

```bash
git add arduino/clauduino_led/clauduino_led.ino arduino/clauduino_led/arduino_secrets.h.example
git commit -m "Scaffold Arduino sketch with Serial heartbeat"
```

---

## Task 4: WiFi connect in setup()

**Files:**
- Modify: `arduino/clauduino_led/clauduino_led.ino`

- [ ] **Step 4.1: Rewrite `clauduino_led.ino` to connect to WiFi**

Replace the file contents with:
```cpp
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
```

- [ ] **Step 4.2: Upload and verify**

Upload from Arduino IDE. In Serial Monitor:

Expected:
```
clauduino-led: boot
WiFi: connecting to <your SSID>
WiFi: connected, IP=<assigned IP>
```

If it loops on "retry in 3s", check SSID/password in `arduino_secrets.h`.

- [ ] **Step 4.3: Commit**

```bash
git add arduino/clauduino_led/clauduino_led.ino
git commit -m "Connect Arduino to WiFi on boot"
```

---

## Task 5: MQTT connect + subscribe + log received topics

**Files:**
- Modify: `arduino/clauduino_led/clauduino_led.ino`

- [ ] **Step 5.1: Extend the sketch to connect and subscribe**

Replace `clauduino_led.ino` with:
```cpp
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
  Serial.print(F("WiFi: connected, IP="));
  Serial.println(WiFi.localIP());
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
```

- [ ] **Step 5.2: Upload and verify the connect path**

Upload. Serial Monitor expected:
```
clauduino-led: boot
WiFi: ...
WiFi: connected, IP=...
MQTT: connecting to <host>:1883
MQTT: connected
MQTT: subscribed to clauduino/led/#
```

- [ ] **Step 5.3: Verify messages arrive**

Without closing Serial Monitor, in a Mac terminal:
```bash
make trigger-stop
make trigger-subagent
make trigger-notify
```

Expected in Serial Monitor:
```
MQTT rx: topic=clauduino/led/stop size=0
MQTT rx: topic=clauduino/led/subagent_stop size=0
MQTT rx: topic=clauduino/led/notification size=0
```

- [ ] **Step 5.4: Commit**

```bash
git add arduino/clauduino_led/clauduino_led.ino
git commit -m "Connect Arduino to MQTT broker and log incoming topics"
```

---

## Task 6: AnimationEngine skeleton + placeholder animations per event

**Files:**
- Create: `arduino/clauduino_led/animations.h`
- Create: `arduino/clauduino_led/animations.cpp`
- Modify: `arduino/clauduino_led/clauduino_led.ino`

This task introduces the animation engine with a visible but intentionally simple placeholder for each event (distinct colors, full strip, 1 second). Real animations replace these in Tasks 7–9.

- [ ] **Step 6.1: Create `animations.h`**

File: `arduino/clauduino_led/animations.h`
```cpp
#pragma once

#include <FastLED.h>

constexpr int NUM_LEDS = 46;
constexpr int DATA_PIN = 6;          // D6

enum class Event : uint8_t {
  None,
  Stop,
  SubagentStop,
  Notification
};

// Maps a topic like "clauduino/led/stop" to an Event.
// Returns Event::None if the topic isn't recognised.
Event eventFromTopic(const char* topic);

class AnimationEngine {
public:
  void begin(CRGB* buffer);          // store pointer, clear strip
  void start(Event e);               // pre-empts any current animation
  void tick();                       // call every loop iteration
  bool isActive() const { return current_ != Event::None; }

private:
  CRGB* leds_ = nullptr;
  Event current_ = Event::None;
  unsigned long startedAt_ = 0;      // millis() when current started
  unsigned long lastStep_ = 0;       // millis() of last frame
  uint16_t step_ = 0;                // generic step counter
  uint8_t pass_ = 0;                 // used by chase animations

  void renderPlaceholder(CRGB color, unsigned long durationMs);
  void clear_();
};
```

- [ ] **Step 6.2: Create `animations.cpp` with placeholder implementations**

File: `arduino/clauduino_led/animations.cpp`
```cpp
#include "animations.h"
#include <string.h>

Event eventFromTopic(const char* topic) {
  // topic is expected to start with "clauduino/led/"
  const char* prefix = "clauduino/led/";
  const size_t plen = strlen(prefix);
  if (strncmp(topic, prefix, plen) != 0) return Event::None;

  const char* suffix = topic + plen;
  if (strcmp(suffix, "stop")           == 0) return Event::Stop;
  if (strcmp(suffix, "subagent_stop")  == 0) return Event::SubagentStop;
  if (strcmp(suffix, "notification")   == 0) return Event::Notification;
  return Event::None;
}

void AnimationEngine::begin(CRGB* buffer) {
  leds_ = buffer;
  clear_();
  FastLED.show();
}

void AnimationEngine::start(Event e) {
  current_   = e;
  startedAt_ = millis();
  lastStep_  = startedAt_;
  step_      = 0;
  pass_      = 0;
}

void AnimationEngine::clear_() {
  if (!leds_) return;
  fill_solid(leds_, NUM_LEDS, CRGB::Black);
}

// Placeholder: fill the whole strip with `color` for `durationMs`, then off.
void AnimationEngine::renderPlaceholder(CRGB color, unsigned long durationMs) {
  unsigned long elapsed = millis() - startedAt_;
  if (elapsed >= durationMs) {
    clear_();
    FastLED.show();
    current_ = Event::None;
    return;
  }
  fill_solid(leds_, NUM_LEDS, color);
  FastLED.show();
}

void AnimationEngine::tick() {
  if (current_ == Event::None) return;

  switch (current_) {
    case Event::Stop:          renderPlaceholder(CRGB::White,  1000); break;
    case Event::SubagentStop:  renderPlaceholder(CRGB::Cyan,   1000); break;
    case Event::Notification:  renderPlaceholder(CRGB::Orange, 1000); break;
    default: break;
  }
}
```

- [ ] **Step 6.3: Wire the engine into `clauduino_led.ino`**

Replace `clauduino_led.ino` with:
```cpp
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <FastLED.h>
#include "arduino_secrets.h"
#include "animations.h"

static CRGB leds[NUM_LEDS];
static AnimationEngine engine;

static WiFiClient wifiClient;
static MqttClient mqttClient(wifiClient);

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

  if (e != Event::None) engine.start(e);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("clauduino-led: boot"));

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(96);          // conservative default; tune later
  engine.begin(leds);

  connectWiFi();
  mqttClient.onMessage(onMqttMessage);
  connectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); }
  if (!mqttClient.connected())       { connectMQTT(); }
  mqttClient.poll();
  engine.tick();
}
```

- [ ] **Step 6.4: Wire up hardware**

Before uploading:
1. Connect the strip's data-in to Arduino D6 via a 330 Ω resistor. If you have a 74AHCT125 level shifter, use it between D6 and the strip (recommended for reliability).
2. Connect the strip's +5V and GND to a dedicated 5V PSU (≥3A). Share GND with the Arduino.
3. Add a 1000 µF capacitor across the strip's 5V and GND close to the first LED.

Upload the sketch.

- [ ] **Step 6.5: Manual verification**

Open Serial Monitor. Run from a Mac terminal:
```bash
make trigger-stop
```
Expected on strip: all 46 LEDs turn white for ~1 second, then off.

```bash
make trigger-subagent
```
Expected: all LEDs cyan for ~1 second.

```bash
make trigger-notify
```
Expected: all LEDs orange for ~1 second.

Serial also prints `event=1` / `event=2` / `event=3` respectively.

- [ ] **Step 6.6: Commit**

```bash
git add arduino/clauduino_led/animations.h \
        arduino/clauduino_led/animations.cpp \
        arduino/clauduino_led/clauduino_led.ino
git commit -m "Wire up AnimationEngine with placeholder animations"
```

---

## Task 7: Implement `Stop` animation (rainbow chase ×3, hue-shifted)

**Files:**
- Modify: `arduino/clauduino_led/animations.cpp`

Spec recap:
- A "comet" of 8 LEDs travels 0 → 45.
- Within the comet, hues span ~30° so it reads as a rainbow gradient tail.
- Base hue advances 120° per pass across 3 passes.
- ~20 ms per step → ~0.9 s per pass → ~2.8 s total, then fade out.

FastLED uses a 0–255 hue scale. 120° ≈ 85 units, 30° ≈ 21 units.

- [ ] **Step 7.1: Replace `renderStop` case with the real animation**

In `animations.cpp`, **remove** the `case Event::Stop: renderPlaceholder(...)` line and replace it with a call to a new `renderStop()` method. Add the method.

Final `animations.cpp` should read:
```cpp
#include "animations.h"
#include <string.h>

Event eventFromTopic(const char* topic) {
  const char* prefix = "clauduino/led/";
  const size_t plen = strlen(prefix);
  if (strncmp(topic, prefix, plen) != 0) return Event::None;
  const char* suffix = topic + plen;
  if (strcmp(suffix, "stop")           == 0) return Event::Stop;
  if (strcmp(suffix, "subagent_stop")  == 0) return Event::SubagentStop;
  if (strcmp(suffix, "notification")   == 0) return Event::Notification;
  return Event::None;
}

void AnimationEngine::begin(CRGB* buffer) {
  leds_ = buffer;
  clear_();
  FastLED.show();
}

void AnimationEngine::start(Event e) {
  current_   = e;
  startedAt_ = millis();
  lastStep_  = startedAt_;
  step_      = 0;
  pass_      = 0;
}

void AnimationEngine::clear_() {
  if (!leds_) return;
  fill_solid(leds_, NUM_LEDS, CRGB::Black);
}

void AnimationEngine::renderPlaceholder(CRGB color, unsigned long durationMs) {
  unsigned long elapsed = millis() - startedAt_;
  if (elapsed >= durationMs) {
    clear_();
    FastLED.show();
    current_ = Event::None;
    return;
  }
  fill_solid(leds_, NUM_LEDS, color);
  FastLED.show();
}

// Stop: rainbow comet (length 8) chases head-to-tail, hue offset +120° per pass, 3 passes.
void AnimationEngine::renderStop() {
  constexpr uint16_t STEP_MS     = 20;
  constexpr uint8_t  COMET_LEN   = 8;
  constexpr uint8_t  HUE_PER_LED = 21;   // ~30° in the 0-255 FastLED scale
  constexpr uint8_t  HUE_PER_PASS = 85;  // ~120°
  constexpr uint8_t  TOTAL_PASSES = 3;

  unsigned long now = millis();
  if (now - lastStep_ < STEP_MS) return;
  lastStep_ = now;

  // step_ is the comet head index, 0..NUM_LEDS-1 (inclusive of a trailing fade at the end).
  fill_solid(leds_, NUM_LEDS, CRGB::Black);

  uint8_t baseHue = pass_ * HUE_PER_PASS;
  for (uint8_t i = 0; i < COMET_LEN; ++i) {
    int16_t idx = (int16_t)step_ - (int16_t)i;
    if (idx < 0 || idx >= NUM_LEDS) continue;
    uint8_t hue        = baseHue + i * HUE_PER_LED;
    uint8_t brightness = 255 - (i * (255 / COMET_LEN));  // fade toward tail
    leds_[idx] = CHSV(hue, 255, brightness);
  }
  FastLED.show();

  step_++;
  if (step_ >= NUM_LEDS + COMET_LEN) {
    step_ = 0;
    pass_++;
    if (pass_ >= TOTAL_PASSES) {
      clear_();
      FastLED.show();
      current_ = Event::None;
    }
  }
}

void AnimationEngine::tick() {
  if (current_ == Event::None) return;
  switch (current_) {
    case Event::Stop:          renderStop(); break;
    case Event::SubagentStop:  renderPlaceholder(CRGB::Cyan,   1000); break;
    case Event::Notification:  renderPlaceholder(CRGB::Orange, 1000); break;
    default: break;
  }
}
```

- [ ] **Step 7.2: Declare `renderStop` in `animations.h`**

Add inside the `AnimationEngine` class `private:` section (below `renderPlaceholder`):
```cpp
  void renderStop();
```

- [ ] **Step 7.3: Upload and verify**

Upload. In a Mac terminal:
```bash
make trigger-stop
```

Expected: a bright 8-LED comet with rainbow-gradient tail sweeps 0→45 three times. Each sweep's base color is noticeably different (roughly red-ish, green-ish, blue-ish). Strip goes dark between passes for a brief moment and after the third pass.

If the comet jumps, is jittery, or colors look wrong: re-check DATA_PIN is D6 and NUM_LEDS is 46.

- [ ] **Step 7.4: Commit**

```bash
git add arduino/clauduino_led/animations.h arduino/clauduino_led/animations.cpp
git commit -m "Implement Stop animation: rainbow comet, 3 passes, hue-shifted"
```

---

## Task 8: Implement `SubagentStop` animation (single cyan sweep)

**Files:**
- Modify: `arduino/clauduino_led/animations.cpp`
- Modify: `arduino/clauduino_led/animations.h`

- [ ] **Step 8.1: Add `renderSubagentStop` declaration**

In `animations.h` `AnimationEngine` `private:` section, below `renderStop()`:
```cpp
  void renderSubagentStop();
```

- [ ] **Step 8.2: Implement it in `animations.cpp`**

Add this method in `animations.cpp` above `tick()`:
```cpp
// SubagentStop: single cyan comet, one pass, dimmer than Stop.
void AnimationEngine::renderSubagentStop() {
  constexpr uint16_t STEP_MS     = 20;
  constexpr uint8_t  COMET_LEN   = 6;
  constexpr uint8_t  HEAD_BRIGHT = 180;  // softer than Stop's 255

  unsigned long now = millis();
  if (now - lastStep_ < STEP_MS) return;
  lastStep_ = now;

  fill_solid(leds_, NUM_LEDS, CRGB::Black);

  for (uint8_t i = 0; i < COMET_LEN; ++i) {
    int16_t idx = (int16_t)step_ - (int16_t)i;
    if (idx < 0 || idx >= NUM_LEDS) continue;
    uint8_t brightness = HEAD_BRIGHT - (i * (HEAD_BRIGHT / COMET_LEN));
    leds_[idx] = CHSV(128, 255, brightness);  // hue 128 ≈ cyan
  }
  FastLED.show();

  step_++;
  if (step_ >= NUM_LEDS + COMET_LEN) {
    clear_();
    FastLED.show();
    current_ = Event::None;
  }
}
```

- [ ] **Step 8.3: Dispatch SubagentStop to the real animation**

In `animations.cpp` inside `tick()`, change:
```cpp
case Event::SubagentStop:  renderPlaceholder(CRGB::Cyan, 1000); break;
```
to:
```cpp
case Event::SubagentStop:  renderSubagentStop(); break;
```

- [ ] **Step 8.4: Upload and verify**

Upload. In a Mac terminal:
```bash
make trigger-subagent
```

Expected: a single cyan comet (shorter and dimmer than `Stop`'s rainbow) sweeps once from 0 to 45, then the strip goes dark. About 1 second total.

- [ ] **Step 8.5: Commit**

```bash
git add arduino/clauduino_led/animations.h arduino/clauduino_led/animations.cpp
git commit -m "Implement SubagentStop animation: single cyan sweep"
```

---

## Task 9: Implement `Notification` animation (amber breathe)

**Files:**
- Modify: `arduino/clauduino_led/animations.cpp`
- Modify: `arduino/clauduino_led/animations.h`

Spec: whole strip amber, brightness follows a sine wave (~1.5 s per cycle), 3 cycles (~5 s). Amber hue ≈ 32 on FastLED's 0–255 scale (≈45° on the color wheel).

- [ ] **Step 9.1: Add `renderNotification` declaration**

In `animations.h` `AnimationEngine` `private:` section, below `renderSubagentStop()`:
```cpp
  void renderNotification();
```

- [ ] **Step 9.2: Implement it in `animations.cpp`**

Add above `tick()`:
```cpp
// Notification: amber breathe, 3 cycles ~1.5s each.
void AnimationEngine::renderNotification() {
  constexpr uint16_t STEP_MS       = 16;   // ~60 fps feel
  constexpr uint16_t CYCLE_MS      = 1500;
  constexpr uint8_t  CYCLES        = 3;
  constexpr uint8_t  AMBER_HUE     = 32;
  constexpr uint8_t  PEAK_BRIGHT   = 200;

  unsigned long now = millis();
  if (now - lastStep_ < STEP_MS) return;
  lastStep_ = now;

  unsigned long elapsed = now - startedAt_;
  if (elapsed >= (unsigned long)CYCLE_MS * CYCLES) {
    clear_();
    FastLED.show();
    current_ = Event::None;
    return;
  }

  // Map elapsed into [0..255] per cycle, use sine8 for breathing curve.
  uint8_t phase = (uint8_t)((elapsed % CYCLE_MS) * 255UL / CYCLE_MS);
  // sine8(0)=128, sine8(64)=255, sine8(128)=128, sine8(192)=1 — map so breath starts/ends dim.
  uint8_t brightness = ((uint16_t)sin8(phase) * PEAK_BRIGHT) / 255;

  fill_solid(leds_, NUM_LEDS, CHSV(AMBER_HUE, 255, brightness));
  FastLED.show();
}
```

- [ ] **Step 9.3: Dispatch Notification to the real animation**

In `animations.cpp` `tick()`, change:
```cpp
case Event::Notification:  renderPlaceholder(CRGB::Orange, 1000); break;
```
to:
```cpp
case Event::Notification:  renderNotification(); break;
```

- [ ] **Step 9.4: Upload and verify single event**

Upload. In a Mac terminal:
```bash
make trigger-notify
```
Expected: whole strip glows amber and "breathes" in brightness three times over ~5 seconds, then goes dark.

- [ ] **Step 9.5: Verify pre-emption**

While the notification breathe is still running, quickly run:
```bash
make trigger-stop
```
Expected: the amber breathe cuts off immediately and the rainbow comet starts. (This exercises the "new event pre-empts current" rule from the spec.)

- [ ] **Step 9.6: Commit**

```bash
git add arduino/clauduino_led/animations.h arduino/clauduino_led/animations.cpp
git commit -m "Implement Notification animation: amber breathe, 3 cycles"
```

---

## Task 10: Claude Code hook wiring in `.claude/settings.json`

**Files:**
- Create: `.claude/settings.json`

This is the last piece — connect real Claude events to the MQTT publish.

- [ ] **Step 10.1: Verify `mosquitto` CLI is installed on the Mac**

```bash
which mosquitto_pub
```
Expected: a path under `/opt/homebrew/bin` or `/usr/local/bin`.

If missing:
```bash
brew install mosquitto
```
Re-check: `mosquitto_pub --help` should print usage.

- [ ] **Step 10.2: Create `.claude/settings.json`**

File: `.claude/settings.json`
```json
{
  "hooks": {
    "Stop": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "mosquitto_pub -h 127.0.0.1 -p 1883 -W 1 -q 0 -t clauduino/led/stop -m '' || true"
          }
        ]
      }
    ],
    "SubagentStop": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "mosquitto_pub -h 127.0.0.1 -p 1883 -W 1 -q 0 -t clauduino/led/subagent_stop -m '' || true"
          }
        ]
      }
    ],
    "Notification": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "mosquitto_pub -h 127.0.0.1 -p 1883 -W 1 -q 0 -t clauduino/led/notification -m '' || true"
          }
        ]
      }
    ]
  }
}
```

- [ ] **Step 10.3: Verify hook config is picked up**

1. Restart / reload Claude Code in this repo so it picks up the new `.claude/settings.json`.
2. In a Mac terminal: `make sub`
3. Trigger a Claude turn: ask Claude Code anything trivial and wait for the response to finish.

Expected in `make sub` output within a second of Claude finishing:
```
clauduino/led/stop
```

- [ ] **Step 10.4: Verify Arduino animates**

With the Arduino powered and connected, trigger another Claude turn. Expected: rainbow comet animation plays on the strip after Claude's reply ends.

- [ ] **Step 10.5: Verify broker-down does not break Claude**

```bash
make down
```
Trigger a Claude turn. Expected: Claude still responds; hook fails silently (1-second timeout, `|| true`). No visible UI lag.

Restart:
```bash
make up
```

- [ ] **Step 10.6: Commit**

```bash
git add .claude/settings.json
git commit -m "Wire Claude Code Stop/SubagentStop/Notification hooks to MQTT"
```

---

## Task 11: Update CLAUDE.md with setup, hardware, and usage

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 11.1: Update the Status checklist**

In `CLAUDE.md`, find the Status section at the bottom and replace it with:

```markdown
## Status

- [x] Mosquitto broker in docker-compose, verified pub/sub round-trip
- [x] Claude Code hooks (Stop / SubagentStop / Notification) publishing to MQTT
- [x] Arduino sketch subscribing to `clauduino/led/#` and driving the strip
```

- [ ] **Step 11.2: Add a "First-time setup" section**

Insert this section above the existing "Running the broker" section:

```markdown
## First-time setup

**Mac side:**
```bash
brew install mosquitto        # provides mosquitto_pub for the hook
make up                       # start the broker
```

The hook wiring is already in `.claude/settings.json` and is picked up by Claude Code automatically.

**Arduino side:**
1. Install the **Arduino UNO R4 Boards** package and the **ArduinoMqttClient** and **FastLED** libraries from the Arduino IDE.
2. Copy `arduino/clauduino_led/arduino_secrets.h.example` to `arduino/clauduino_led/arduino_secrets.h` and fill in your WiFi SSID/password and the Mac's LAN IP (`ipconfig getifaddr en0`). The file is gitignored.
3. Open `arduino/clauduino_led/clauduino_led.ino`, select board **Arduino UNO R4 WiFi**, and upload.
```

- [ ] **Step 11.3: Add a "Hardware" section**

Insert below "First-time setup":

```markdown
## Hardware

- Arduino Uno R4 WiFi
- WS2812 LED strip, 46 LEDs
- 5V PSU rated ≥3A for the strip, with its GND tied to the Arduino GND
- 330 Ω resistor in series on the data line (Arduino D6 → strip DI)
- 1000 µF capacitor across strip 5V/GND, near the first LED
- Recommended: a 74AHCT125 (or similar) 3.3V→5V buffer between D6 and the strip for reliable data. Uno R4 drives 3.3V logic; WS2812 is happier seeing ~5V.

Data pin is `D6` (`DATA_PIN` in `arduino/clauduino_led/animations.h`). Change it there if you need a different pin.
```

- [ ] **Step 11.4: Add a "Triggering animations manually" section**

Insert below "Smoke-testing pub/sub":

```markdown
## Triggering animations manually

Useful for iterating on the Arduino firmware without waiting for real Claude events:

```bash
make trigger-stop        # rainbow chase ×3
make trigger-subagent    # single cyan sweep
make trigger-notify      # amber breathe ~5s
```

Each publishes to `clauduino/led/<event>` with an empty payload — the same message Claude's hook sends.
```

- [ ] **Step 11.5: Commit**

```bash
git add CLAUDE.md
git commit -m "Document Arduino setup, hardware, and manual trigger usage"
```

---

## Self-review

**Spec coverage:**

| Spec requirement | Task(s) |
|---|---|
| Three hook events publish to distinct topics | 10 |
| `mosquitto_pub` command fails silently if broker down (`\|\| true`, `-W 1`) | 10.2, 10.5 |
| `.claude/settings.json` tracked in repo | 10 |
| `arduino_secrets.h` gitignored, `.h.example` tracked | 2, 3 |
| WiFiS3 + ArduinoMqttClient + FastLED stack | 4, 5, 6 |
| Subscribe to `clauduino/led/#`, dispatch on topic suffix | 5, 6 |
| Non-blocking animation engine (millis-paced, no `delay()` inside tick) | 6, 7, 8, 9 |
| Event pre-empts current animation | 6.2 (`start()` resets state), verified in 9.5 |
| Stop animation: rainbow chase ×3, hue-shifted | 7 |
| SubagentStop animation: single cyan sweep | 8 |
| Notification animation: amber breathe ~5s | 9 |
| Data pin D6 | 6 (`DATA_PIN`) |
| Reconnect logic for WiFi and MQTT | 4, 5, 6 (loop reconnect checks) |
| Manual trigger targets for testing without Claude | 1 |
| Hardware wiring documented | 11 |
| Acceptance: Stop fires within 500ms of Claude finishing | 10.3 (observable; sub-1s in practice with local broker) |

No gaps.

**Placeholder scan:** no TBD/TODO/"implement later"/"handle edge cases" in any task. All code blocks are complete; all file paths are exact; all commands have expected output.

**Type / name consistency:**
- `Event` enum values (`None`, `Stop`, `SubagentStop`, `Notification`) — used consistently in Tasks 6–9.
- `eventFromTopic(const char*)` — defined in 6.1, used in 6.3.
- `AnimationEngine` methods: `begin(CRGB*)`, `start(Event)`, `tick()`, `isActive()` const, `clear_()`, `renderPlaceholder(CRGB, unsigned long)`, `renderStop()`, `renderSubagentStop()`, `renderNotification()` — declared in 6.1 and 7.2/8.1/9.1 additions; used consistently in `tick()`.
- Constants `NUM_LEDS = 46`, `DATA_PIN = 6` — defined in 6.1, used in 6.3.
- Topic strings match between Task 1 (Makefile), Task 6 (`eventFromTopic`), and Task 10 (hook commands).

No inconsistencies found.

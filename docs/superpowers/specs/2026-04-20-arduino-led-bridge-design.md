# Claude-Hook-to-Arduino LED Bridge — Design

**Date:** 2026-04-20
**Status:** Approved
**Scope:** Claude Code hook → MQTT → Arduino Uno R4 WiFi → 46-LED WS2812 strip

## Goal

Physical feedback on the desk: when Claude Code finishes a turn, needs attention, or a subagent completes, a WS2812 LED strip plays an animation specific to that event. Broker is already provisioned locally (previous commit).

## Architecture

```
Claude Code (Stop / SubagentStop / Notification hook)
    │ shells out:  mosquitto_pub -h 127.0.0.1 -p 1883 -t clauduino/led/<event> -m ""
    ▼
Mosquitto broker (Docker, this repo)  port 1883
    │ subscribed by
    ▼
Arduino Uno R4 WiFi  (subscribes to clauduino/led/#)
    │ dispatches on topic suffix
    ▼
WS2812 strip, 46 LEDs, data on D6
```

Three independent components, each with one job:

| Component | Responsibility | Lives in |
|---|---|---|
| Hook | Translate a Claude Code lifecycle event into an MQTT publish | `.claude/settings.json` |
| Broker | Route messages from publisher to subscriber | `docker-compose.yml` (already built) |
| Firmware | Receive topic, play the matching animation, never block MQTT | `arduino/clauduino_led/` |

## Topic contract

One topic per event. Payload empty — the topic IS the signal.

| Event | Topic | Payload |
|---|---|---|
| Claude finished a response | `clauduino/led/stop` | empty |
| A subagent finished | `clauduino/led/subagent_stop` | empty |
| Claude needs user attention | `clauduino/led/notification` | empty |

Firmware subscribes to `clauduino/led/#` and dispatches on the topic suffix. Adding a new event later = add a topic + dispatch case; no schema migration.

**Rejected alternative:** single topic `clauduino/led/event` with event name in the payload. Requires payload parsing on an 8-bit-feeling MCU, harder to filter/test, less idiomatic MQTT.

## Animations

All three run as non-blocking state machines on the Arduino. A new event always cancels the current animation and starts the new one (so `notification` interrupts immediately).

### `stop` — rainbow chase ×3, hue-shifted per pass
- A short moving "comet" of ~8 LEDs travels from index 0 → 45.
- Within the comet, each LED has a different hue (a slice of the rainbow), creating a rainbow-gradient tail.
- Between passes, the comet's starting hue advances by 120° — so pass 1 starts red-ish, pass 2 green-ish, pass 3 blue-ish.
- ~20 ms per step × 46 steps × 3 passes ≈ 3 s total, then fade off.

### `subagent_stop` — single cyan sweep
- Same comet structure, one pass, solid cyan (no hue variation within the comet).
- Softer/dimmer than `stop` — it's a subtle tick, not a finale.
- ~1 s total.

### `notification` — amber breathe ~5s
- All LEDs set to amber (~45° on the 0–360° color wheel).
- Brightness follows a sine wave (breathe) over ~1.5 s per cycle.
- Runs for ~5 s (3 cycles) or until another event arrives.

## Hook side

### Config location
`.claude/settings.json` (tracked in repo). Works out of the box after cloning + `brew install mosquitto`. User-global settings are deliberately NOT used — this is a per-project integration.

### Hook entries
Three entries, one per event. Each runs:

```
mosquitto_pub -h 127.0.0.1 -p 1883 -W 1 -q 0 -t clauduino/led/<event> -m "" || true
```

- `-W 1` — 1-second connect timeout, so a stopped broker doesn't hang the hook.
- `-q 0` — QoS 0; fire-and-forget. We're not archiving these events.
- `|| true` — never fail the hook; a broken light must not break Claude.

### What the settings.json block looks like
```json
{
  "hooks": {
    "Stop": [
      { "hooks": [ { "type": "command",
                     "command": "mosquitto_pub -h 127.0.0.1 -p 1883 -W 1 -q 0 -t clauduino/led/stop -m '' || true" } ] }
    ],
    "SubagentStop": [
      { "hooks": [ { "type": "command",
                     "command": "mosquitto_pub -h 127.0.0.1 -p 1883 -W 1 -q 0 -t clauduino/led/subagent_stop -m '' || true" } ] }
    ],
    "Notification": [
      { "hooks": [ { "type": "command",
                     "command": "mosquitto_pub -h 127.0.0.1 -p 1883 -W 1 -q 0 -t clauduino/led/notification -m '' || true" } ] }
    ]
  }
}
```

All three events support optional matchers (e.g. `SubagentStop` can match on `agent_type`, `Notification` on `notification_type`). We deliberately omit `matcher` so every variant fires the light — that matches the "any event → light up" intent.

Each hook receives JSON on stdin (`session_id`, `transcript_path`, `cwd`, and event-specific fields). We don't read it — the command is a straight `mosquitto_pub`. Future enhancement: enrich the MQTT payload with `session_id` so multiple users in the same broker could have per-session strips.

## Arduino side

### Libraries
- `WiFiS3` — stock Uno R4 WiFi library, talks to the onboard ESP32-S3 co-processor.
- `ArduinoMqttClient` — official Arduino MQTT client.
- `FastLED` — WS2812 driver with built-in HSV helpers (needed for the rainbow chase).

### Configuration
Credentials and broker address live in `arduino_secrets.h` (gitignored). A tracked `arduino_secrets.h.example` documents the expected fields:

```cpp
#define SECRET_WIFI_SSID     "..."
#define SECRET_WIFI_PASSWORD "..."
#define SECRET_MQTT_HOST     "192.168.x.y"  // Mac's LAN IP
#define SECRET_MQTT_PORT     1883
```

### Startup flow
1. Connect to WiFi (blocking, with retry).
2. Connect to MQTT broker (blocking, with retry).
3. Subscribe to `clauduino/led/#`.
4. Enter main loop.

### Main loop
Every iteration:
1. `mqttClient.poll()` — service incoming messages / keepalives. Non-blocking.
2. `animation.tick()` — advance one frame if enough time has passed (`millis()`-paced).
3. Check WiFi / MQTT connection; reconnect with simple backoff if dropped.

Animations are step-counter state machines — no `delay()` calls — so loop latency stays low (~1–5 ms) and MQTT messages are picked up mid-animation.

### Message dispatch
On incoming message: parse topic suffix, look up animation by name, call `animation.start(<name>)`. Starting a new animation resets the state machine — the current animation is pre-empted.

### Data pin
`D6` by default. Pick another digital pin if `D6` conflicts with something.

## Hardware notes

These go in CLAUDE.md / a hardware README for the user to act on — not encoded in firmware.

- **Power:** 46 × WS2812 ≈ 2.8 A at worst case (all white, full brightness). USB cannot supply this. Use a dedicated 5V ≥3A PSU powering the strip directly, with its **ground tied to the Arduino GND**.
- **Logic level:** Uno R4 I/O is 3.3V. WS2812 data line typically wants ~0.7·VCC (3.5V). Short data run often works, but the reliable fix is a 74AHCT125 (or equivalent) 3.3→5V buffer between Arduino D6 and the strip's DI.
- **Protection:** 1000 µF electrolytic across 5V/GND at the strip, 330 Ω in series on the data line. Standard WS2812 defensive wiring.
- **First-LED burn-out:** if you ever see LED 0 dying repeatedly, it's almost always insufficient data-line buffering — add the level shifter.

## Repo layout changes

```
arduino/clauduino_led/
    clauduino_led.ino           # main sketch
    animations.h                # AnimationEngine interface + event enum
    animations.cpp              # three animation state machines
    arduino_secrets.h           # gitignored
    arduino_secrets.h.example   # tracked template
.claude/settings.json           # hook config (tracked)
docs/superpowers/specs/...      # this spec
Makefile                        # + trigger-stop / trigger-subagent / trigger-notify
.gitignore                      # + arduino_secrets.h
CLAUDE.md                       # + setup / hardware / usage updates
```

## Testing strategy

Test each layer independently, then end-to-end.

| Layer | How |
|---|---|
| Broker | `make smoke` (already works) |
| Hook only (no Arduino) | `make sub` tail, trigger a real Claude turn; watch messages appear |
| Arduino only (no hook) | `make trigger-stop` / `trigger-subagent` / `trigger-notify` — publishes the exact topic the hook publishes, Arduino plays the animation |
| End-to-end | Broker up + Arduino powered + Claude Code running in this repo → animation fires on each event |

New Makefile targets just wrap `mosquitto_pub` against the three topics with empty payloads.

## Out of scope (explicitly)

- Success/error differentiation in the `Stop` animation. Claude Code's `Stop` hook doesn't receive a success/failure signal; we'd need transcript parsing, which isn't worth the complexity here.
- Authenticated / TLS MQTT. LAN-only dev setup; anonymous is fine.
- OTA firmware updates for the Arduino.
- Web dashboard or mobile control.
- Retained messages / event replay. Events are ephemeral.

## Open decisions deferred to implementation

None blocking. Minor tuning that can happen during implementation:

- Exact comet length (default 8 LEDs).
- Exact per-step delay (default 20 ms).
- Amber hue value for `notification` (~45° on the color wheel; FastLED uses a 0–255 scale, so ≈32 there).

## Acceptance criteria

- [ ] `brew install mosquitto` + clone + flash Arduino + power on → Claude Code Stop fires the rainbow chase within 500 ms of Claude finishing a turn.
- [ ] `SubagentStop` produces a distinct, shorter cyan sweep.
- [ ] `Notification` interrupts any running animation and plays amber breathe.
- [ ] Broker down → hook still returns cleanly in ≤1 s; Claude is not blocked.
- [ ] Arduino WiFi drop → firmware reconnects without reflash.
- [ ] All three `make trigger-*` targets play the right animation without Claude involvement.

# clauduino-led

Arduino-driven LED strip that lights up when Claude Code finishes a task. Claude Code hooks publish over MQTT; an Arduino subscribes and drives the strip.

## Architecture

```
Claude Code (Stop/SubagentStop hook)
        │ publishes
        ▼
  MQTT broker (Mosquitto, this repo)
        │ subscribed by
        ▼
  Arduino + LED strip + piezo buzzer
```

- **Broker:** Eclipse Mosquitto 2.0 in Docker (this repo).
- **Transport:** MQTT over TCP on `1883`, WebSockets on `9001` (for browser debug clients).
- **Auth:** anonymous (LAN-only test setup).
- **Topic convention (proposed):** `clauduino/led/<event>` — e.g. `clauduino/led/status` for task-finished events.

## Layout

- `docker-compose.yml` — starts the Mosquitto broker
- `mosquitto/config/mosquitto.conf` — broker config
- `mosquitto/data/`, `mosquitto/log/` — runtime (gitignored)

Arduino sketch and Claude Code hook script will live alongside these as the project grows.

## First-time setup

**Mac side:**
```bash
brew install mosquitto        # provides mosquitto_pub for the hook
make up                       # start the broker
```

Install the Claude Code hooks. They live in user scope so they fire from any project, not just this repo. Merge the contents of `.claude/settings.json.example` into `~/.claude/settings.json`, or copy the file outright if you don't have one yet:

```bash
# no user settings.json yet
cp .claude/settings.json.example ~/.claude/settings.json

# otherwise, merge the "hooks" block from the example into your existing file
```

**Arduino side:**
1. In the Arduino IDE install the **Arduino UNO R4 Boards** package and the **ArduinoMqttClient** and **FastLED** libraries.
2. Copy `arduino/clauduino_led/arduino_secrets.h.example` to `arduino/clauduino_led/arduino_secrets.h` and fill in your WiFi SSID/password and the Mac's LAN IP (`ipconfig getifaddr en0`). The file is gitignored.
3. Open `arduino/clauduino_led/clauduino_led.ino`, select board **Arduino UNO R4 WiFi**, and upload.

## Hardware

- Arduino Uno R4 WiFi
- WS2812 / WS2812B LED strip, 46 LEDs, data on `D6`
- 5 V PSU (≥ 3 A) for the strip, common ground with the Arduino
- 220–470 Ω resistor in series on the data line, near the strip
- 1000 µF electrolytic across the strip's 5V/GND, near LED #1
- Passive piezo buzzer on `D8` (one lead to D8, other to GND) — plays the currently-bound song on Stop events

Full wiring diagram, rationale for each passive, and a troubleshooting table are in [`docs/hardware.md`](docs/hardware.md).

## Running the broker

`make help` lists all targets. Common ones:

```bash
make up          # start broker
make ps          # check health
make logs        # tail logs
make sub         # interactive subscribe to clauduino/#
make smoke       # end-to-end pub/sub round-trip test
make down        # stop broker
make clean       # stop + wipe runtime data/log
```

Raw docker-compose works too (`docker-compose up -d`, etc.) if you prefer.

## Smoke-testing pub/sub

Subscribe (inside the broker container):
```bash
docker exec -it clauduino-mqtt mosquitto_sub -h localhost -t 'clauduino/#' -v
```

Publish from another terminal (the real hook sends the same shape — empty payload on one of the three `clauduino/led/<event>` topics):
```bash
docker exec clauduino-mqtt mosquitto_pub -h localhost -t 'clauduino/led/stop' -m ''
```

From a separate host/container (confirms the published host port works):
```bash
docker run --rm eclipse-mosquitto:2.0 \
  mosquitto_pub -h host.docker.internal -p 1883 \
  -t 'clauduino/led/stop' -m ''
```

## Hook behavior when the broker is down

Each hook command ends with `|| true` so a failed `mosquitto_pub` can never fail a Claude turn. Practical cases:

- **Broker stopped (`make down`).** `mosquitto_pub` gets ECONNREFUSED from the kernel in milliseconds on localhost; the hook returns almost instantly and Claude's UI is unaffected.
- **Mac networking wedged / Docker daemon hung.** `mosquitto_pub` has no explicit connect timeout here (the `-W` flag, despite appearing in the original spec, is `mosquitto_sub`-only), so it can block for its default TCP connect timeout. If this ever bites you, wrap the command in `timeout 2 ...` (install via `brew install coreutils` if needed).

## Triggering animations manually

Useful for iterating on the Arduino firmware without waiting for real Claude events:

```bash
make trigger-stop        # rainbow chase ×6
make trigger-subagent    # single cyan sweep
make trigger-notify      # amber breathe ~5s
```

Each publishes to `clauduino/led/<event>` with an empty payload — the same message Claude's hook sends.

## Swapping the Stop song

Songs live in `arduino/clauduino_led/songs.h` / `songs.cpp`. To change
what plays on Stop:

1. (If needed) add a new song in `songs.cpp` — a `const Note[]` array
   plus a `Song` struct. Declare its `extern const Song NAME;` in
   `songs.h`.
2. In `arduino/clauduino_led/clauduino_led.ino`, change the one line:
   ```cpp
   static constexpr const Song& SONG_FOR_STOP = HAPPY_BIRTHDAY;
   ```
   to point at the new song.
3. Re-upload the sketch from the Arduino IDE.

To silence the buzzer entirely without removing hardware, comment out
the `player.play(SONG_FOR_STOP)` call in `onMqttMessage()` and reupload.

**Retrigger behavior differs between LED and buzzer.** If Claude finishes twice in quick succession, the LED animation pre-empts (new rainbow chase from LED 0) while the buzzer ignores the retrigger and lets the current song finish. Over rapid turns you may see two or three rainbow chases but only one rendition of the song. This is intentional — a 12-second song that constantly restarts would be tiresome.

## Conventions

- **Commits:** always create commits via the `/commit` skill rather than crafting commit messages by hand. It keeps message style consistent across the project.

## Status

- [x] Mosquitto broker in docker-compose, verified pub/sub round-trip
- [x] Claude Code hooks (Stop / SubagentStop / Notification) publishing to MQTT
- [x] Arduino sketch subscribing to `clauduino/led/#` and driving the strip

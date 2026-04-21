# Buzzer Playback System — Design

**Date:** 2026-04-21
**Status:** Approved
**Scope:** Add a passive piezo buzzer to the Arduino that plays a compile-time-selectable song when a Claude `Stop` event fires, in parallel with the existing LED rainbow-chase animation. Engine designed so new songs (and future event/song bindings) are a library addition, not an engine change.

## Goal

When Claude finishes a turn today, the LED strip runs a rainbow chase. With this change, the buzzer will simultaneously play "Happy Birthday To You" on pin `D8`. Swapping songs later means "edit one line + reflash" — no new files to write for the swap, no MQTT config.

## Architecture

Two independent, non-blocking state machines in the Arduino main loop:

```
MQTT message → eventFromTopic → Event e
    ├── engine.start(e)                 (existing — LED animation)
    └── if (e == Stop) player.play(SONG_FOR_STOP)   (NEW — buzzer)

loop():
    engine.tick()   (existing)
    player.tick()   (NEW)
```

No coupling between engines. They share nothing but the `Event` enum that fires them.

```
┌──────────────────────────┐       ┌────────────────────────┐
│ AnimationEngine (LED)    │       │ SongPlayer (buzzer)    │
│ - CRGB leds_[46]         │       │ - pin D8               │
│ - renderStop/Subagent... │       │ - Note-array playback  │
│ - millis-paced tick()    │       │ - millis-paced tick()  │
└──────────────────────────┘       └────────────────────────┘
            ▲                                 ▲
            └────── onMqttMessage ────────────┘
                    (clauduino/led/#)
```

## Topic contract — unchanged

No new MQTT topics. The existing `clauduino/led/stop` fires both engines; `clauduino/led/subagent_stop` and `clauduino/led/notification` only drive the LED (no songs bound). The hook config in `.claude/settings.json` is untouched.

## SongPlayer — API

```cpp
class SongPlayer {
 public:
  void begin(uint8_t pin);
  void play(const Song& song);    // no-op if already playing
  void tick();                    // call every loop iteration
  bool isPlaying() const;
 private:
  uint8_t        pin_;
  const Song*    current_        = nullptr;
  size_t         noteIdx_        = 0;
  unsigned long  noteStartedAt_  = 0;
  static constexpr uint16_t GAP_MS = 30;   // articulation between notes
};
```

`tick()` is the only time-sensitive part. Logic:

1. If `current_ == nullptr`, return (idle).
2. If `noteIdx_ >= current_->count`, call `noTone(pin_)`, set `current_ = nullptr`, return.
3. If `millis() - noteStartedAt_ >= note.duration_ms + GAP_MS`, advance — `noteIdx_++`, reset timer, kick off the next note (`tone(pin_, freq, duration)` for non-rest, `noTone(pin_)` for rest).

Arduino's `tone(pin, freq, duration)` is itself non-blocking — it programs a hardware timer and returns. We only have to sequence the notes.

## Types

```cpp
struct Note {
  uint16_t freq_hz;      // 0 = rest (silence)
  uint16_t duration_ms;
};

struct Song {
  const Note* notes;
  size_t      count;
  const char* name;      // for Serial debug only
};
```

Songs live in `songs.h` (declarations) / `songs.cpp` (definitions), as `const` file-scope data — goes in flash automatically on the Uno R4 WiFi's ARM core (no `PROGMEM` / `pgm_read_*` needed, unlike AVR Arduinos).

## Song library convention

`songs.h`:
```cpp
extern const Song HAPPY_BIRTHDAY;
// future: extern const Song IMPERIAL_MARCH;
```

`songs.cpp`:
```cpp
static const Note HAPPY_BIRTHDAY_NOTES[] = {
  // { freq_hz, duration_ms }
  ...
};
const Song HAPPY_BIRTHDAY = {
  HAPPY_BIRTHDAY_NOTES,
  sizeof(HAPPY_BIRTHDAY_NOTES) / sizeof(Note),
  "Happy Birthday"
};
```

**Adding a song:** append an array + a `Song` struct in `songs.cpp`, declare the extern in `songs.h`. Zero changes to `SongPlayer` or the `.ino`.

**Swapping the song bound to Stop:** change one line in `clauduino_led.ino`:
```cpp
#include "songs.h"
constexpr const Song& SONG_FOR_STOP = HAPPY_BIRTHDAY;   // change me
```
Then reflash. No other edit.

## Happy Birthday transcription

Standard verse, C major, quarter note = 400 ms (≈150 BPM). ~15 s total. Note sequence (G4=392 Hz, A4=440, B4=494, C5=523, D5=587, E5=659, F5=698, G5=784):

```
Hap-py birth-day to  you     | G4 G4 A4 G4 C5 B4
Hap-py birth-day to  you     | G4 G4 A4 G4 D5 C5
Hap-py birth-day dear <name> | G4 G4 G5 E5 C5 B4 A4
Hap-py birth-day to  you     | F5 F5 E5 C5 D5 C5
```

Durations approximate the familiar rhythm: dotted-eighth + sixteenth + quarter + quarter + quarter + half across each line. Rests (`freq_hz = 0`) separate the four phrases (~150 ms each).

Exact encoding is implementation detail, finalized in the plan.

## Retrigger behavior

When `SongPlayer::play()` is called while `isPlaying()` is true: **ignore the new request**, let the current song finish naturally.

Rationale: the LED engine pre-empts because animations are short (≤ 3 s) and crisp restarts look intentional. A 15-second song that constantly restarts on rapid Claude turns would be annoying. The buzzer and LED will sometimes visibly desync (LED does 2 rainbow chases while the buzzer still plays the first song's tail) — acceptable, and the divergence should be documented in CLAUDE.md.

## Mute

No runtime mute mechanism. To silence: comment out the `player.play(SONG_FOR_STOP)` line in the `.ino` and reflash — a two-character edit. MQTT-driven mute and compile-time flags were considered and rejected (YAGNI; the current swap workflow is already "edit one line + reflash").

## Hardware wiring

```
Arduino D8 ─── piezo (+)
Arduino GND ── piezo (–)
```

- Passive piezo buzzer (two leads, no on-board oscillator).
- Optional 100–220 Ω series resistor on the D8 line as a volume softener. Not needed to protect the pin — piezos draw <10 mA.
- No common-ground setup (buzzer is powered from the Arduino's 3.3 V I/O directly).
- `docs/hardware.md` gets a short **Buzzer** section with this diagram and a note about `tone()`'s single-voice limitation.

## Repo layout (additions)

```
arduino/clauduino_led/
  buzzer.h            # SongPlayer class + Note + Song types
  buzzer.cpp          # SongPlayer implementation
  songs.h             # extern const Song HAPPY_BIRTHDAY;
  songs.cpp           # Note arrays + Song struct(s)
  clauduino_led.ino   # + SongPlayer instance, player.begin, player.play, player.tick
docs/hardware.md      # + Buzzer wiring section
CLAUDE.md             # + architecture line + swap-song instruction
```

## Testing

Re-uses the existing trigger harness:

- **`make trigger-stop`** — already publishes to `clauduino/led/stop`. After this change, LED animates AND buzzer sings.
- **End-to-end via real Claude event** — continue to work as before. No new Makefile target needed.
- Without the buzzer physically wired, `tone()` on D8 is a no-op audibly. Firmware logic can be iterated without having to plug in the buzzer every time.

## Out of scope

- Polyphony / chords. `tone()` is single-voice; accepted.
- Runtime song selection via MQTT payload. Rejected per brainstorming.
- Mute mechanism. Rejected per brainstorming (comment the `play()` call to silence).
- Tempo/volume/transpose runtime controls. Could come later; not needed to ship.
- Songs for SubagentStop / Notification. Engine is ready; binding is a one-line addition if we want it.

## Acceptance criteria

- [ ] Stop event fires both the rainbow chase and Happy Birthday simultaneously, starting within ~ms of each other.
- [ ] Arduino remains responsive during playback — new MQTT messages are still picked up (LED pre-empts, song keeps playing its current note sequence).
- [ ] Calling `player.play()` while a song is playing is a no-op (verified via quick consecutive Stop events: one song, not a stutter).
- [ ] Adding a new song is demonstrably a library-only change (append to `songs.h`/`.cpp`; no touch to `buzzer.*`).
- [ ] Swapping Stop's song is a one-line edit in the `.ino`.
- [ ] Buzzer unplugged → firmware still runs, no crashes, no Serial errors.

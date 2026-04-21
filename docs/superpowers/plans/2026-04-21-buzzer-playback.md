# Buzzer Playback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a passive piezo buzzer on Arduino pin `D8` that plays "Happy Birthday To You" simultaneously with the rainbow-chase LED animation whenever a Claude Stop event fires. Songs are compile-time-selectable via a one-line edit.

**Architecture:** New `SongPlayer` class mirrors the existing `AnimationEngine` pattern — non-blocking, `millis()`-paced, pre-empts-not behavior (ignores re-triggers). Songs are `const Note[]` arrays in `songs.h/.cpp`; Stop's song is bound by a single `constexpr const Song& SONG_FOR_STOP = HAPPY_BIRTHDAY;` in the sketch. AnimationEngine remains untouched.

**Tech Stack:** Arduino Uno R4 WiFi core, `tone()` / `noTone()` API (standard). No new libraries.

**Spec:** `docs/superpowers/specs/2026-04-21-buzzer-playback-design.md`

---

## File structure (locked in before tasks)

| Path | Purpose |
|---|---|
| `arduino/clauduino_led/buzzer.h` | `Note` / `Song` types + `SongPlayer` class declaration |
| `arduino/clauduino_led/buzzer.cpp` | `SongPlayer` implementation — non-blocking note sequencer |
| `arduino/clauduino_led/songs.h` | `extern const Song HAPPY_BIRTHDAY;` (+ future songs) |
| `arduino/clauduino_led/songs.cpp` | Note array + `Song` struct definition for Happy Birthday |
| `arduino/clauduino_led/clauduino_led.ino` | Instantiate `SongPlayer`, call `begin/play/tick`, bind `SONG_FOR_STOP` |
| `docs/hardware.md` | New "Buzzer" section with wiring + notes about `tone()` |
| `CLAUDE.md` | Architecture line + swap-song instruction |

One clear responsibility per file: **buzzer.*** owns playback state, **songs.*** owns music data, **.ino** owns wiring. Same separation already proven on the LED side.

---

## Testing strategy

- **Compile-only tasks (1–3):** a sketch upload after each task is sufficient verification — Arduino IDE compiles every `.cpp` and `.h` in the sketch folder, so syntax/type errors surface on Upload even while the code is unused.
- **Integration task (4):** first audible test. Upload, run `make trigger-stop`, expect both the rainbow chase AND Happy Birthday at once.
- **Doc tasks (5–6):** visual review only.

The existing `make trigger-stop` is the harness — no new Makefile targets needed.

---

## Task 1: Add `buzzer.h` — types + SongPlayer class skeleton

**Files:**
- Create: `arduino/clauduino_led/buzzer.h`

- [ ] **Step 1.1: Create `buzzer.h`**

File: `arduino/clauduino_led/buzzer.h`
```cpp
#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// A single note in a song. freq_hz == 0 means "rest" (silence for duration_ms).
struct Note {
  uint16_t freq_hz;
  uint16_t duration_ms;
};

// A song is just an array of notes plus its length and a debug name.
struct Song {
  const Note* notes;
  size_t      count;
  const char* name;
};

// Non-blocking monophonic song player driven by Arduino's tone() API.
// Mirrors AnimationEngine's pattern: begin() once in setup(), play() to
// kick off a song, tick() every loop iteration to advance notes.
class SongPlayer {
 public:
  void begin(uint8_t pin);
  void play(const Song& song);   // no-op if a song is already playing
  void tick();                    // call every loop iteration
  bool isPlaying() const { return current_ != nullptr; }

 private:
  uint8_t        pin_             = 0;
  const Song*    current_         = nullptr;
  size_t         noteIdx_         = 0;
  unsigned long  noteStartedAt_   = 0;

  static constexpr uint16_t GAP_MS = 30;  // short silence between notes
};
```

- [ ] **Step 1.2: Verify it compiles**

In Arduino IDE, with the existing sketch open, click Upload. Expected: compiles cleanly and uploads successfully. The new header is not yet included anywhere, so behavior is unchanged (LED still animates on events, no sound).

If compile fails with errors *about `buzzer.h`*, fix them; errors elsewhere are pre-existing and not this task's concern.

- [ ] **Step 1.3: Commit**

```bash
git add arduino/clauduino_led/buzzer.h
git commit -m "$(cat <<'EOF'
feat(buzzer): add Note/Song types and SongPlayer class header

Declares the tiny data model for the upcoming compile-time song
library — Note is {freq_hz, duration_ms} (freq_hz=0 means rest),
Song is a pointer-and-length around a Note array plus a debug name.
SongPlayer mirrors AnimationEngine: begin() once, play() to kick
off a song, tick() every loop. Implementation comes in the next
task.

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Implement `SongPlayer` in `buzzer.cpp`

**Files:**
- Create: `arduino/clauduino_led/buzzer.cpp`

Non-blocking note sequencer. The state machine:

- `play()` stores the song pointer, resets `noteIdx_=0`, stamps `noteStartedAt_=millis()`, and kicks off the first note (`tone(pin, freq, duration)` or `noTone(pin)` for a rest).
- `tick()` checks if `millis() - noteStartedAt_ >= duration + GAP_MS`. When true, advance to the next note; when the index passes the end, stop playback.

- [ ] **Step 2.1: Create `buzzer.cpp`**

File: `arduino/clauduino_led/buzzer.cpp`
```cpp
#include "buzzer.h"

void SongPlayer::begin(uint8_t pin) {
  pin_ = pin;
  noTone(pin_);
}

static void startNote(uint8_t pin, const Note& n) {
  if (n.freq_hz == 0) {
    noTone(pin);
  } else {
    tone(pin, n.freq_hz, n.duration_ms);
  }
}

void SongPlayer::play(const Song& song) {
  // Retrigger rule: ignore new requests while a song is playing.
  if (current_ != nullptr) return;
  if (song.count == 0 || song.notes == nullptr) return;

  current_       = &song;
  noteIdx_       = 0;
  noteStartedAt_ = millis();
  startNote(pin_, song.notes[0]);
}

void SongPlayer::tick() {
  if (current_ == nullptr) return;

  const Note& cur = current_->notes[noteIdx_];
  unsigned long elapsed = millis() - noteStartedAt_;
  if (elapsed < (unsigned long)cur.duration_ms + GAP_MS) return;

  noteIdx_++;
  if (noteIdx_ >= current_->count) {
    noTone(pin_);
    current_ = nullptr;
    return;
  }

  noteStartedAt_ = millis();
  startNote(pin_, current_->notes[noteIdx_]);
}
```

- [ ] **Step 2.2: Verify it compiles**

Click Upload in the Arduino IDE. Expected: compiles cleanly. Behavior still unchanged (the class isn't instantiated yet).

If compile fails with errors about `buzzer.cpp`, fix them and upload again.

- [ ] **Step 2.3: Commit**

```bash
git add arduino/clauduino_led/buzzer.cpp
git commit -m "$(cat <<'EOF'
feat(buzzer): implement non-blocking SongPlayer state machine

play() kicks off the first note with tone() (or noTone() for a
rest) and stamps noteStartedAt_. tick() advances to the next note
once the current note's duration plus GAP_MS has elapsed. When
noteIdx_ runs off the end of the song, noTone() and clear current_.
A second play() while current_ is non-null is a no-op — matches
the "ignore retrigger during playback" rule from the spec.

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Song library — `songs.h` + `songs.cpp` with Happy Birthday

**Files:**
- Create: `arduino/clauduino_led/songs.h`
- Create: `arduino/clauduino_led/songs.cpp`

The Happy Birthday transcription uses standard rhythm at quarter=400 ms (~150 BPM) — roughly 12 s total playback.

- [ ] **Step 3.1: Create `songs.h`**

File: `arduino/clauduino_led/songs.h`
```cpp
#pragma once

#include "buzzer.h"

extern const Song HAPPY_BIRTHDAY;
```

- [ ] **Step 3.2: Create `songs.cpp`**

File: `arduino/clauduino_led/songs.cpp`
```cpp
#include "songs.h"

namespace {

// Frequencies (Hz) — C major, 4th and 5th octaves.
constexpr uint16_t G4 = 392;
constexpr uint16_t A4 = 440;
constexpr uint16_t B4 = 494;
constexpr uint16_t C5 = 523;
constexpr uint16_t D5 = 587;
constexpr uint16_t E5 = 659;
constexpr uint16_t F5 = 698;
constexpr uint16_t G5 = 784;

// Durations (ms) at ~150 BPM, quarter = 400 ms.
constexpr uint16_t E  = 200;    // eighth
constexpr uint16_t Q  = 400;    // quarter
constexpr uint16_t H  = 800;    // half
constexpr uint16_t DH = 1200;   // dotted half
constexpr uint16_t R  = 250;    // inter-phrase rest

const Note HAPPY_BIRTHDAY_NOTES[] = {
  // "Hap-py birth-day to  you"
  {G4, E}, {G4, E}, {A4, Q}, {G4, Q}, {C5, Q}, {B4, DH},
  {0,  R},
  // "Hap-py birth-day to  you"
  {G4, E}, {G4, E}, {A4, Q}, {G4, Q}, {D5, Q}, {C5, DH},
  {0,  R},
  // "Hap-py birth-day dear <name>"
  {G4, E}, {G4, E}, {G5, Q}, {E5, Q}, {C5, Q}, {B4, Q}, {A4, H},
  {0,  R},
  // "Hap-py birth-day to  you"
  {F5, E}, {F5, E}, {E5, Q}, {C5, Q}, {D5, Q}, {C5, DH},
};

}  // namespace

const Song HAPPY_BIRTHDAY = {
  HAPPY_BIRTHDAY_NOTES,
  sizeof(HAPPY_BIRTHDAY_NOTES) / sizeof(Note),
  "Happy Birthday"
};
```

- [ ] **Step 3.3: Verify it compiles**

Upload from Arduino IDE. Expected: compiles cleanly. Still no behavior change (the song isn't played from anywhere yet).

- [ ] **Step 3.4: Commit**

```bash
git add arduino/clauduino_led/songs.h arduino/clauduino_led/songs.cpp
git commit -m "$(cat <<'EOF'
feat(buzzer): add Happy Birthday song data

songs.h declares extern const Song HAPPY_BIRTHDAY. songs.cpp defines
the note array (C major, ~150 BPM, ~12s total) and the Song struct.
Named frequency and duration constants are kept file-local in an
anonymous namespace so future songs can reuse the same style without
exporting them. Adding a new song later is a songs.h/cpp-only
change — no touch to buzzer.* or the .ino.

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Wire `SongPlayer` into `clauduino_led.ino`

**Files:**
- Modify: `arduino/clauduino_led/clauduino_led.ino`

This is the first task with audible output. Instantiate `SongPlayer`, call `begin()` in `setup()`, call `play(SONG_FOR_STOP)` from the Stop branch of `onMqttMessage`, and pump `tick()` in `loop()`.

- [ ] **Step 4.1: Edit the includes**

Add these two lines to the top of `arduino/clauduino_led/clauduino_led.ino`, right after `#include "animations.h"`:

```cpp
#include "buzzer.h"
#include "songs.h"
```

- [ ] **Step 4.2: Add the pin constant + player instance + song binding**

Right after the `static AnimationEngine engine;` line, add:

```cpp
static constexpr uint8_t BUZZER_PIN = 8;        // D8
static SongPlayer player;
static constexpr const Song& SONG_FOR_STOP = HAPPY_BIRTHDAY;  // change me to swap songs
```

- [ ] **Step 4.3: Call `player.begin()` in `setup()`**

In `setup()`, right after `engine.begin(leds);`, add:

```cpp
  player.begin(BUZZER_PIN);
```

- [ ] **Step 4.4: Kick off the song on Stop in `onMqttMessage`**

In `onMqttMessage()`, inside the block that runs when `e != Event::None`, replace:

```cpp
  if (e != Event::None) engine.start(e);
```

with:

```cpp
  if (e != Event::None) {
    engine.start(e);
    if (e == Event::Stop) player.play(SONG_FOR_STOP);
  }
```

- [ ] **Step 4.5: Pump the player in `loop()`**

In `loop()`, after `engine.tick();`, add:

```cpp
  player.tick();
```

- [ ] **Step 4.6: Wire the buzzer hardware**

Before uploading:
1. Arduino `D8` → one lead of the passive piezo buzzer.
2. Other lead of the buzzer → Arduino `GND`.
3. (Optional) a 100–220 Ω resistor in series on `D8` to soften volume.

- [ ] **Step 4.7: Upload and verify**

Upload. In a Mac terminal:
```bash
make trigger-stop
```
Expected: the LED rainbow chase starts, AND the buzzer plays Happy Birthday (~12 s).

Also verify:
- **Serial monitor** still prints `MQTT rx: topic=clauduino/led/stop event=1` as before.
- **Retrigger:** while Happy Birthday is still playing, run `make trigger-stop` a second time. Expected: a second rainbow chase plays (LED pre-empts) but the song does NOT restart — it keeps playing the one that was already in progress.
- **Other events don't sing:** run `make trigger-subagent` and `make trigger-notify`. Expected: LED animates, buzzer stays silent.

- [ ] **Step 4.8: Commit**

```bash
git add arduino/clauduino_led/clauduino_led.ino
git commit -m "$(cat <<'EOF'
feat(buzzer): play Happy Birthday on Stop via SongPlayer on D8

Instantiates SongPlayer, binds SONG_FOR_STOP to HAPPY_BIRTHDAY, and
wires the engine into setup/loop/onMqttMessage. On Stop events the
sketch now fires the LED animation AND the buzzer song in parallel.
Other events (SubagentStop, Notification) still only drive the LED.
Swapping the song later is a one-line edit to SONG_FOR_STOP.

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Document the buzzer wiring in `docs/hardware.md`

**Files:**
- Modify: `docs/hardware.md`

- [ ] **Step 5.1: Add a "Buzzer" section**

Insert this section into `docs/hardware.md`, after the "Logic level (3.3 V → 5 V)" section and before "Troubleshooting quick reference":

```markdown
## Buzzer (piezo)

A passive piezo buzzer plays compile-time-selected songs on Stop events.
One lead of the buzzer goes to Arduino `D8`, the other to Arduino `GND`.
That's it — no shared ground to worry about (the buzzer is powered
directly from the pin, drawing < 10 mA).

```
Arduino D8 ─── piezo (+)
Arduino GND ── piezo (–)
```

Optional: a 100–220 Ω resistor in series on the `D8` line to soften
volume. Not needed to protect the pin — piezos draw negligible current.

Arduino's `tone()` API uses one hardware timer and can play only one
frequency at a time (no chords). `tone(pin, freq, duration)` is
non-blocking — it programs the timer and returns immediately. The
`SongPlayer` class in the firmware sequences notes by calling `tone()`
once per note and advancing on a `millis()` timer.

If the buzzer is unplugged, `tone()` on `D8` is a silent no-op and
the firmware keeps running normally.
```

- [ ] **Step 5.2: Commit**

```bash
git add docs/hardware.md
git commit -m "$(cat <<'EOF'
docs(hardware): document piezo buzzer wiring and tone() semantics

Two-pin wiring (D8 + GND), optional series resistor for volume, and
a note about tone() being single-voice / non-blocking to save the
next reader from rediscovering the same thing in the Arduino
reference.

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Update CLAUDE.md — buzzer in the architecture + how to swap songs

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 6.1: Extend the Architecture diagram**

Find the Architecture ASCII diagram (the one showing `Claude Code (Stop/SubagentStop hook)` → `MQTT broker` → `Arduino + LED strip`) and replace the `Arduino + LED strip` line with:

```
  Arduino + LED strip + piezo buzzer
```

- [ ] **Step 6.2: Extend the Hardware section**

In the Hardware section of CLAUDE.md, add a new bullet at the end of the list:

```markdown
- Passive piezo buzzer on `D8` (one lead to D8, other to GND) — plays the currently-bound song on Stop events
```

- [ ] **Step 6.3: Add a "Swapping the Stop song" subsection**

Insert this just before the "Conventions" section:

```markdown
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
```

- [ ] **Step 6.4: Commit**

```bash
git add CLAUDE.md
git commit -m "$(cat <<'EOF'
docs: describe buzzer in architecture and add swap-song recipe

- Extend the architecture diagram so "Arduino + LED strip + piezo
  buzzer" reflects the new sibling channel.
- Add a Hardware bullet for the D8 buzzer.
- New "Swapping the Stop song" section walks through the one-line
  edit convention and mentions the comment-to-silence fallback.

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Self-review

**Spec coverage:**

| Spec requirement | Task(s) |
|---|---|
| `SongPlayer` parallel to `AnimationEngine`, non-blocking | 1, 2 |
| `Note` / `Song` types with `freq_hz=0` as rest | 1 |
| Non-blocking state machine using `millis()` | 2 |
| `play()` is a no-op while already playing (retrigger rule) | 2 |
| Happy Birthday song data (array + struct) | 3 |
| `SONG_FOR_STOP` compile-time binding via one-line edit | 4 |
| Pin D8 | 4 |
| Co-play with LED (both fire on Stop) | 4 |
| Only Stop plays a song (others stay silent) | 4 |
| `docs/hardware.md` describes the buzzer wiring | 5 |
| `CLAUDE.md` reflects architecture + swap instructions | 6 |
| Retrigger: observable with `make trigger-stop` twice in a row | 4.7 |
| Other events stay silent: observable with `make trigger-subagent/notify` | 4.7 |

No gaps.

**Placeholder scan:** no TBD/TODO/"handle edge cases" anywhere. Each task has exact file paths, exact code, exact commands, and concrete expected observations.

**Type / name consistency:**
- `Note { uint16_t freq_hz; uint16_t duration_ms; }` — same throughout.
- `Song { const Note* notes; size_t count; const char* name; }` — same throughout.
- `SongPlayer` methods `begin(uint8_t)`, `play(const Song&)`, `tick()`, `isPlaying() const` — consistent in Tasks 1, 2, and 4.
- Private members `pin_`, `current_`, `noteIdx_`, `noteStartedAt_`, `GAP_MS` — declared in 1.1, used in 2.1.
- `HAPPY_BIRTHDAY` — declared extern in `songs.h` (3.1), defined in `songs.cpp` (3.2), referenced in `.ino` (4.2).
- `BUZZER_PIN = 8` matches the hardware doc (5.1) and `SONG_FOR_STOP` usage (4.2).

No inconsistencies.

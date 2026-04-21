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

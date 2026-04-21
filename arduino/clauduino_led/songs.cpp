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

// Auto-extracted from hidup-jokowi.mp3 via scripts/mp3_to_song.py
// (--fmin 150 --fmax 1000 --min-note-ms 80). Polyphonic source, so
// pitches are approximations of the dominant melodic contour rather
// than a clean transcription.
const Note HIDUP_JOKOWI_NOTES[] = {
  {  392,   93 },
  {  311,  186 },
  {  349,  255 },
  {  330,   93 },
  {  349,  209 },
  {  466,  232 },
  {  175,  116 },
  {  392,  139 },
  {  311,  139 },
  {  175,  139 },
  {  415,  163 },
};

}  // namespace

const Song HAPPY_BIRTHDAY = {
  HAPPY_BIRTHDAY_NOTES,
  sizeof(HAPPY_BIRTHDAY_NOTES) / sizeof(Note),
  "Happy Birthday"
};

const Song HIDUP_JOKOWI = {
  HIDUP_JOKOWI_NOTES,
  sizeof(HIDUP_JOKOWI_NOTES) / sizeof(Note),
  "Hidup Jokowi"
};

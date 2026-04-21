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

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
  void renderStop();
  void clear_();
};

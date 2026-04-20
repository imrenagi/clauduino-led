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

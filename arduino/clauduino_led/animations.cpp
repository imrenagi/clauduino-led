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

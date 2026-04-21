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

// Stop: rainbow comet (length 8) chases head-to-tail, hue offset +120° per pass, 3 passes.
void AnimationEngine::renderStop() {
  constexpr uint16_t STEP_MS     = 20;
  constexpr uint8_t  COMET_LEN   = 8;
  constexpr uint8_t  HUE_PER_LED = 21;   // ~30° in the 0-255 FastLED scale
  constexpr uint8_t  HUE_PER_PASS = 85;  // ~120°
  constexpr uint8_t  TOTAL_PASSES = 6;

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

// SubagentStop: single cyan comet, one pass, dimmer than Stop.
void AnimationEngine::renderSubagentStop() {
  constexpr uint16_t STEP_MS     = 20;
  constexpr uint8_t  COMET_LEN   = 6;
  constexpr uint8_t  HEAD_BRIGHT = 180;  // softer than Stop's 255

  unsigned long now = millis();
  if (now - lastStep_ < STEP_MS) return;
  lastStep_ = now;

  fill_solid(leds_, NUM_LEDS, CRGB::Black);

  for (uint8_t i = 0; i < COMET_LEN; ++i) {
    int16_t idx = (int16_t)step_ - (int16_t)i;
    if (idx < 0 || idx >= NUM_LEDS) continue;
    uint8_t brightness = HEAD_BRIGHT - (i * (HEAD_BRIGHT / COMET_LEN));
    leds_[idx] = CHSV(128, 255, brightness);  // hue 128 ≈ cyan
  }
  FastLED.show();

  step_++;
  if (step_ >= NUM_LEDS + COMET_LEN) {
    clear_();
    FastLED.show();
    current_ = Event::None;
  }
}

// Notification: amber breathe, 3 cycles ~1.5s each.
void AnimationEngine::renderNotification() {
  constexpr uint16_t STEP_MS       = 16;   // ~60 fps feel
  constexpr uint16_t CYCLE_MS      = 1500;
  constexpr uint8_t  CYCLES        = 3;
  constexpr uint8_t  AMBER_HUE     = 32;
  constexpr uint8_t  PEAK_BRIGHT   = 200;

  unsigned long now = millis();
  if (now - lastStep_ < STEP_MS) return;
  lastStep_ = now;

  unsigned long elapsed = now - startedAt_;
  if (elapsed >= (unsigned long)CYCLE_MS * CYCLES) {
    clear_();
    FastLED.show();
    current_ = Event::None;
    return;
  }

  // Map elapsed into [0..255] per cycle, use sine8 for breathing curve.
  uint8_t phase = (uint8_t)((elapsed % CYCLE_MS) * 255UL / CYCLE_MS);
  // sine8(0)=128, sine8(64)=255, sine8(128)=128, sine8(192)=1 — map so breath starts/ends dim.
  uint8_t brightness = ((uint16_t)sin8(phase) * PEAK_BRIGHT) / 255;

  fill_solid(leds_, NUM_LEDS, CHSV(AMBER_HUE, 255, brightness));
  FastLED.show();
}

void AnimationEngine::tick() {
  if (current_ == Event::None) return;
  switch (current_) {
    case Event::Stop:          renderStop(); break;
    case Event::SubagentStop:  renderSubagentStop(); break;
    case Event::Notification:  renderNotification(); break;
    default: break;
  }
}

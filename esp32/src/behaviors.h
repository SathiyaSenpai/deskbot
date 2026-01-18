#ifndef BEHAVIORS_H
#define BEHAVIORS_H

#include <Arduino.h>

struct Behavior {
  const char* name;
  float openness;
  float scaleX;
  float bottomLid;
  float offsetX;
  float offsetY;
  const char* effect; // effect hint for LED/audio
};

// Behavior table ported from web demo (offsetX range ±48, offsetY range ±24)
static const Behavior BEHAVIORS[] = {
  {"calm_idle",         1.0f,  1.0f, 0.5f,   0,    0,   nullptr},
  {"sleepy_idle",       0.5f,  1.0f, 0.3f,   0,   24,   "zzz"},
  {"curious_idle",      1.15f, 1.06f,0.5f,   0,    0,   nullptr},
  {"shy_idle",          0.75f, 0.98f,0.5f, -48,  24,   "blush"},
  {"scanning_idle",     1.02f, 1.0f, 0.5f,   0,    0,   "scan"},

  {"listening_focused", 1.1f,  1.08f,0.5f,   0,  -24,   nullptr},
  {"listening_confused",1.05f, 1.02f,0.5f,   0,    0,   "confused"},
  {"thinking",          0.8f,  0.98f,0.5f, -48,  -24,   "thinking"},
  {"processing",        0.95f, 1.0f, 0.5f,   0,    0,   "processing"},
  {"understanding",     1.08f, 1.05f,0.5f,   0,    0,   "understanding"},
  {"misunderstanding",  0.9f,  0.99f,0.5f,   0,    0,   "misunderstanding"},
  {"startled",          1.25f, 1.15f,0.5f,   0,  -24,   "shock"},
  {"refusing_gently",   0.8f,  0.98f,0.4f,   0,    0,   "refusing"},

  {"happy",             1.0f,  1.0f, 0.8f,   0,    0,   "glow"},
  {"shy_happy",         0.95f, 0.99f,0.7f,  48,  -24,   "hearts"},
  {"soft_sad",          0.55f, 0.88f,0.2f,   0,   24,   nullptr},
  {"disappointed",      0.82f, 0.75f,0.3f, -48,    0,   "disappointed_drop"},
  {"embarrassed",       0.6f,  0.97f,0.3f,  48,   24,   "embarrassed"},
  {"playful_mischief",  1.1f,  1.04f,0.7f,  48,  -24,   "sparkles"},

  {"sleeping_deep",     0.0f,  1.0f, 0.5f,   0,    0,   nullptr},
  {"waking_up_slow",    0.0f,  1.0f, 0.5f,   0,    0,   nullptr},
  {"resting_eyes",      0.4f,  1.0f, 0.4f,   0,    0,   nullptr},
};

static constexpr size_t BEHAVIOR_COUNT = sizeof(BEHAVIORS) / sizeof(BEHAVIORS[0]);

inline const Behavior* findBehavior(const char* name) {
  for (size_t i = 0; i < BEHAVIOR_COUNT; ++i) {
    if (strcmp(BEHAVIORS[i].name, name) == 0) return &BEHAVIORS[i];
  }
  return nullptr;
}

#endif // BEHAVIORS_H

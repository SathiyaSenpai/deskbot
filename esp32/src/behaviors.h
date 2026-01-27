#ifndef BEHAVIORS_H
#define BEHAVIORS_H

#include <Arduino.h>

struct Behavior {
  const char* name;
  float openness;   // 0.0 (closed) to 1.2 (wide open)
  float scaleX;     // Width multiplier (0.8 to 1.2)
  float topLid;     // 0.0 (open) to 1.0 (closed)
  float bottomLid;  // 0.0 (open) to 1.0 (closed)
  float offsetX;    // -5 to 5 (eye position)
  float offsetY;    
  
  uint16_t entryTime; // ms to transition in
  uint16_t holdTime;  // ms to hold (0 = infinite)
  uint16_t exitTime;  // ms to transition out
  const char* ledEffect; 
};

static const Behavior BEHAVIORS[] = {
  // --- IDLE STATES ---
  {"calm_idle",       1.0f, 1.0f, 0.0f, 0.0f, 0, 0,    500, 0,    500,  "cyan"},
  {"sleepy_idle",     0.6f, 1.0f, 0.3f, 0.0f, 0, 3,    1200,0,    1200, "purple"},
  
  // --- EMOTIONS (Timings matched to LED Controller) ---
  // Happy: 5000ms hold (was 2500)
  {"happy",           1.0f, 1.05f, 0.0f, 0.5f, 0, -1,   400, 5000, 500,  "happy"},
  
  // Shy Happy: 5000ms hold
  {"shy_happy",       0.9f, 1.0f, 0.15f, 0.35f, 2, 1,   500, 5000, 500,  "happy"},
  
  // Sad: 8000ms hold (was 3000)
  {"sad",             0.8f, 0.95f, 0.35f, 0.05f, 0, 4,   800, 8000, 800,  "sad"},
  
  // Angry: 4000ms hold (was 2500)
  {"angry",           0.9f, 0.9f, 0.4f, 0.0f,  0, 0,    300, 4000, 500,  "angry"},
  
  // Surprised: 1500ms hold (Keep short)
  {"surprised",       1.2f, 0.9f, 0.0f, 0.0f, 0, -3,    150, 1500, 400,  "surprised"},
  
  // Confused: 3000ms hold
  {"confused",        1.0f, 1.0f, 0.1f, 0.0f, 3, -1,    500, 3000, 500,  "purple"},
  
  // Curious: 4000ms hold (0 means infinite, but we want it to timeout if triggered by sensor)
  {"curious_idle",    1.05f, 1.0f, 0.0f, 0.0f, 4, -2,   400, 4000, 400,  "cyan"},

  // --- FUNCTIONAL STATES ---
  {"listening",       1.1f, 1.0f, 0.0f, 0.0f, 0, 0,    300, 0,    300,  "listening"},
  {"thinking",        1.0f, 1.0f, 0.1f, 0.0f, -8,-8,   400, 0,    400,  "purple"},
  {"speaking",        1.0f, 1.0f, 0.0f, 0.0f, 0, 0,    200, 0,    200,  "cyan"},
  {"sleeping",        0.1f, 1.0f, 0.45f, 0.45f, 0, 8,   2500,0,    2500, "sleeping"},
  {"startled",        1.3f, 0.85f, 0.0f, 0.0f, 0, -4,   100, 1500, 300,  "surprised"},
  {"playful_mischief", 1.0f, 1.0f, 0.0f, 0.3f, 6, 0,   300, 3000, 400,  "happy"},
  
  {nullptr, 0,0,0,0,0,0, 0,0,0, nullptr}
};

inline const Behavior* findBehavior(const char* name) {
  for (int i = 0; BEHAVIORS[i].name != nullptr; i++) {
    if (strcmp(BEHAVIORS[i].name, name) == 0) return &BEHAVIORS[i];
  }
  return &BEHAVIORS[0]; // Default to calm_idle
}

#endif
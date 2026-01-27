#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Adafruit_NeoPixel.h>
#include "pins.h"

// PHASE 10: LED Priority Levels
enum LEDPriority {
  PRIO_ERROR = 5,      // Critical errors
  PRIO_SURPRISE = 4,   // Surprise reaction
  PRIO_EMOTION = 3,    // Emotional states
  PRIO_LISTENING = 2,  // Listening/Active
  PRIO_IDLE = 1        // Breathing/Alive state
};

class LedController {
public:
  void begin() {
    strip_ = Adafruit_NeoPixel(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
    strip_.begin();
    strip_.setBrightness(40);
    strip_.show();
    
    // Start in idle breathing mode
    strncpy(currentMood_, "idle", 15);
    currentMood_[15] = '\0';
    currentPriority_ = PRIO_IDLE;
    startIdleBreathing();
  }

  void setMood(const char* mood) {
    LEDPriority newPrio = getPriority(mood);
    
    // FIXED: Priority system with automatic return to previous state
    if (newPrio < currentPriority_ && stateTimer_ < getMinDuration(currentMood_)) {
      return;
    }
    
    if (strcmp(mood, currentMood_) == 0) return;
    
    // Save previous state for return (only if moving to higher priority)
    if (newPrio > currentPriority_) {
      strncpy(previousMood_, currentMood_, 15);
      previousMood_[15] = '\0';
      previousPriority_ = currentPriority_;
      shouldReturnToPrevious_ = true;
    }
    
    strncpy(currentMood_, mood, 15);
    currentMood_[15] = '\0';
    currentPriority_ = newPrio;
    stateTimer_ = 0;
    
    Serial.printf("[LED] Setting mood: %s (priority %d)\n", mood, newPrio);
    
    // Configure based on mood
    if (strcmp(mood, "idle") == 0 || strcmp(mood, "calm_idle") == 0 || strcmp(mood, "cyan") == 0) {
      startIdleBreathing();
      shouldReturnToPrevious_ = false;  // Idle is the default, don't return from it
    }
    else if (strcmp(mood, "happy") == 0) {
      targetColor_ = strip_.Color(255, 200, 50);
      animMode_ = ANIM_GENTLE_PULSE;
      cycleDuration_ = 3.5f;
      minBrightness_ = 0.3f;
      maxBrightness_ = 0.7f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "surprised") == 0) {
      targetColor_ = strip_.Color(255, 255, 255);
      animMode_ = ANIM_FLASH_ONCE;
      cycleDuration_ = 0.2f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "sad") == 0 || strcmp(mood, "blue") == 0) {
      targetColor_ = strip_.Color(40, 60, 180);
      animMode_ = ANIM_SLOW_BREATHE;
      cycleDuration_ = 6.0f;
      minBrightness_ = 0.05f;
      maxBrightness_ = 0.25f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "sleepy_idle") == 0 || strcmp(mood, "sleeping") == 0 || strcmp(mood, "purple") == 0) {
      targetColor_ = strip_.Color(80, 60, 140);
      animMode_ = ANIM_DEEP_BREATHE;
      cycleDuration_ = 7.0f;
      minBrightness_ = 0.02f;
      maxBrightness_ = 0.15f;
      partialRing_ = true;
      shouldReturnToPrevious_ = false;  // Sleep states don't auto-return
    }
    else if (strcmp(mood, "angry") == 0 || strcmp(mood, "red") == 0) {
      targetColor_ = strip_.Color(255, 40, 0);
      animMode_ = ANIM_SHARP_PULSE;
      cycleDuration_ = 0.8f;
      minBrightness_ = 0.2f;
      maxBrightness_ = 1.0f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "listening") == 0 || strcmp(mood, "green") == 0) {
      targetColor_ = strip_.Color(0, 255, 100);
      animMode_ = ANIM_STEADY_GLOW;
      minBrightness_ = 0.5f;
      maxBrightness_ = 0.6f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "orange") == 0) {
      targetColor_ = strip_.Color(255, 140, 0);
      animMode_ = ANIM_GENTLE_PULSE;
      cycleDuration_ = 3.0f;
      minBrightness_ = 0.3f;
      maxBrightness_ = 0.6f;
      partialRing_ = false;
    }
    else {
      startIdleBreathing();
      shouldReturnToPrevious_ = false;
    }
  }

  void loop(float dt) {
    stateTimer_ += dt;
    
    uint8_t r = (targetColor_ >> 16) & 0xFF;
    uint8_t g = (targetColor_ >> 8) & 0xFF;
    uint8_t b = targetColor_ & 0xFF;
    
    float brightness = calculateBrightness();
    
    // FIXED: Handle flash-once transition - return to previous or idle
    if (animMode_ == ANIM_FLASH_ONCE && stateTimer_ > 0.25f) {
      if (shouldReturnToPrevious_ && strlen(previousMood_) > 0) {
        returnToPreviousState();
      } else {
        setMood("idle");
      }
      return;
    }
    
    // FIXED: Auto-return for high-priority temporary states
    if (shouldReturnToPrevious_ && currentPriority_ >= PRIO_EMOTION) {
      float maxDuration = getMaxDuration(currentMood_);
      if (maxDuration > 0 && stateTimer_ > maxDuration) {
        Serial.printf("[LED] Auto-returning from %s after %.1fs\n", currentMood_, maxDuration);
        returnToPreviousState();
        return;
      }
    }
    
    // Apply brightness
    uint32_t finalColor = strip_.Color(
      r * brightness,
      g * brightness,
      b * brightness
    );
    
    // PHASE 10: Partial ring for sleep (some LEDs off)
    if (partialRing_) {
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i % 3 == 0) {
          strip_.setPixelColor(i, 0);
        } else {
          strip_.setPixelColor(i, finalColor);
        }
      }
    } else {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip_.setPixelColor(i, finalColor);
      }
    }
    
    strip_.show();
  }

  void voiceReact(int level) {
    // Only react if not in high-priority state
    if (currentPriority_ >= PRIO_EMOTION) return;
    
    uint8_t brightness = map(level, 0, 100, 10, 200);
    uint32_t color = strip_.Color(0, brightness, brightness / 2);
    
    for (int i = 0; i < NUM_LEDS; i++) {
      strip_.setPixelColor(i, color);
    }
    strip_.show();
  }

private:
  Adafruit_NeoPixel strip_;
  uint32_t targetColor_ = 0;
  char currentMood_[16] = "idle";
  char previousMood_[16] = "";
  LEDPriority currentPriority_ = PRIO_IDLE;
  LEDPriority previousPriority_ = PRIO_IDLE;
  float stateTimer_ = 0.0f;
  bool shouldReturnToPrevious_ = false;
  
  // Animation parameters
  float cycleDuration_ = 3.0f;
  float minBrightness_ = 0.1f;
  float maxBrightness_ = 0.5f;
  bool partialRing_ = false;
  
  enum AnimMode {
    ANIM_IDLE_BREATHE,
    ANIM_GENTLE_PULSE,
    ANIM_SLOW_BREATHE,
    ANIM_DEEP_BREATHE,
    ANIM_SHARP_PULSE,
    ANIM_FLASH_ONCE,
    ANIM_STEADY_GLOW
  } animMode_ = ANIM_IDLE_BREATHE;

  void startIdleBreathing() {
    targetColor_ = strip_.Color(80, 180, 220);
    animMode_ = ANIM_IDLE_BREATHE;
    cycleDuration_ = 3.0f;
    minBrightness_ = 0.15f;
    maxBrightness_ = 0.45f;
    partialRing_ = false;
    shouldReturnToPrevious_ = false;
  }

  LEDPriority getPriority(const char* mood) {
    if (strcmp(mood, "error") == 0) return PRIO_ERROR;
    if (strcmp(mood, "surprised") == 0) return PRIO_SURPRISE;
    if (strcmp(mood, "happy") == 0 || strcmp(mood, "sad") == 0 || 
        strcmp(mood, "angry") == 0) return PRIO_EMOTION;
    if (strcmp(mood, "listening") == 0) return PRIO_LISTENING;
    return PRIO_IDLE;
  }

  float getMinDuration(const char* mood) {
    if (strcmp(mood, "surprised") == 0) return 0.3f;
    if (strcmp(mood, "angry") == 0) return 1.5f;
    if (strcmp(mood, "happy") == 0) return 2.0f;
    return 0.5f;
  }

  // FIXED: Add maximum duration for auto-return
  float getMaxDuration(const char* mood) {
    if (strcmp(mood, "surprised") == 0) return 1.5f;
    if (strcmp(mood, "happy") == 0) return 5.0f;
    if (strcmp(mood, "sad") == 0) return 8.0f;
    if (strcmp(mood, "angry") == 0) return 4.0f;
    return 0;  // 0 means no auto-return
  }

  float calculateBrightness() {
    float t = stateTimer_;
    
    switch (animMode_) {
      case ANIM_IDLE_BREATHE:
      case ANIM_GENTLE_PULSE: {
        // Smooth sine wave
        float wave = (sin(t * (2.0f * PI / cycleDuration_)) + 1.0f) * 0.5f;
        return minBrightness_ + (wave * (maxBrightness_ - minBrightness_));
      }
      
      case ANIM_SLOW_BREATHE:
      case ANIM_DEEP_BREATHE: {
        // Slower, deeper breathing
        float wave = (sin(t * (2.0f * PI / cycleDuration_)) + 1.0f) * 0.5f;
        // Ease in-out for more organic feel
        wave = wave * wave * (3.0f - 2.0f * wave);
        return minBrightness_ + (wave * (maxBrightness_ - minBrightness_));
      }
      
      case ANIM_SHARP_PULSE: {
        // Quick pulses
        int pulseNum = (int)(t / cycleDuration_) % 3;
        float pulseTime = fmod(t, cycleDuration_) / cycleDuration_;
        
        if (pulseNum < 2) {
          return (pulseTime < 0.3f) ? maxBrightness_ : minBrightness_;
        }
        return minBrightness_;
      }
      
      case ANIM_FLASH_ONCE: {
        if (t < cycleDuration_) return maxBrightness_;
        return minBrightness_;
      }
      
      case ANIM_STEADY_GLOW: {
        // Minimal variation
        float wave = (sin(t * 3.0f) + 1.0f) * 0.5f;
        return minBrightness_ + (wave * 0.1f);
      }
      
      default:
        return minBrightness_;
    }
  }

  void returnToPreviousState() {
    if (strlen(previousMood_) == 0) {
      setMood("idle");
      return;
    }
    
    Serial.printf("[LED] Returning to previous state: %s\n", previousMood_);
    
    // Temporarily save previous state
    char tempMood[16];
    strncpy(tempMood, previousMood_, 15);
    tempMood[15] = '\0';
    
    // Clear previous state tracking
    previousMood_[0] = '\0';
    shouldReturnToPrevious_ = false;
    
    // Set to previous mood
    setMood(tempMood);
  }
};

#endif
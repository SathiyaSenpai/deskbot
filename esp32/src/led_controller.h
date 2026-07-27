#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Adafruit_NeoPixel.h>
#include "pins.h"

class LedController {
public:
  void begin() {
    strip_ = Adafruit_NeoPixel(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
    strip_.begin();
    strip_.setBrightness(40);
    strip_.show();
    
    // Default to idle
    setMood("idle");
  }

  void setMood(const char* mood) {
    // 1. Check if mood is actually changing (Optimization)
    if (strcmp(mood, currentMood_) == 0) return;
    
    // FLASH-ONCE FIX: Save previous mood before setting "surprised" so we can restore
    if (strcmp(mood, "surprised") == 0 || strcmp(mood, "startled") == 0) {
      if (strcmp(currentMood_, "surprised") != 0 && strcmp(currentMood_, "startled") != 0) {
        strncpy(previousMood_, currentMood_, 15);
        previousMood_[15] = '\0';
      }
    }
    
    strncpy(currentMood_, mood, 15);
    currentMood_[15] = '\0';
    stateTimer_ = 0; // Reset animation timer
    flashRestored_ = false; // Reset flash restore flag
    
    Serial.printf("[LED] Setting mood: %s\n", mood);
    
    // 2. Configure Colors & Animation Mode
    if (strcmp(mood, "idle") == 0 || strcmp(mood, "calm_idle") == 0 || strcmp(mood, "cyan") == 0) {
      targetColor_ = strip_.Color(80, 180, 220);
      animMode_ = ANIM_IDLE_BREATHE;
      cycleDuration_ = 3.0f;
      minBrightness_ = 0.15f; maxBrightness_ = 0.45f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "happy") == 0 || strcmp(mood, "shy_happy") == 0) {
      targetColor_ = strip_.Color(255, 200, 50);
      animMode_ = ANIM_GENTLE_PULSE;
      cycleDuration_ = 3.5f;
      minBrightness_ = 0.3f; maxBrightness_ = 0.7f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "surprised") == 0 || strcmp(mood, "startled") == 0) {
      targetColor_ = strip_.Color(255, 255, 255);
      animMode_ = ANIM_FLASH_ONCE;
      cycleDuration_ = 0.2f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "sad") == 0 || strcmp(mood, "blue") == 0) {
      targetColor_ = strip_.Color(40, 60, 180);
      animMode_ = ANIM_SLOW_BREATHE;
      cycleDuration_ = 6.0f;
      minBrightness_ = 0.05f; maxBrightness_ = 0.25f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "sleepy_idle") == 0 || strcmp(mood, "sleeping") == 0 || strcmp(mood, "purple") == 0) {
      targetColor_ = strip_.Color(80, 60, 140);
      animMode_ = ANIM_DEEP_BREATHE;
      cycleDuration_ = 7.0f;
      minBrightness_ = 0.02f; maxBrightness_ = 0.15f;
      partialRing_ = true;
    }
    else if (strcmp(mood, "angry") == 0 || strcmp(mood, "red") == 0) {
      targetColor_ = strip_.Color(255, 40, 0);
      animMode_ = ANIM_SHARP_PULSE;
      cycleDuration_ = 0.8f;
      minBrightness_ = 0.2f; maxBrightness_ = 1.0f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "listening") == 0 || strcmp(mood, "green") == 0) {
      targetColor_ = strip_.Color(0, 255, 100);
      animMode_ = ANIM_STEADY_GLOW;
      minBrightness_ = 0.5f; maxBrightness_ = 0.6f;
      partialRing_ = false;
    }
    else if (strcmp(mood, "orange") == 0) {
      targetColor_ = strip_.Color(255, 140, 0);
      animMode_ = ANIM_GENTLE_PULSE;
      cycleDuration_ = 3.0f;
      minBrightness_ = 0.3f; maxBrightness_ = 0.6f;
      partialRing_ = false;
    }
    else {
      // Default Fallback
      targetColor_ = strip_.Color(80, 180, 220);
      animMode_ = ANIM_IDLE_BREATHE;
    }
  }

  void loop(float dt) {
    stateTimer_ += dt;
    
    // ⚡ CRITICAL FIX: Throttle LED updates to 30 FPS (Every 33ms)
    // This stops the LEDs from spamming the power rail and confusing the sensors
    static unsigned long lastLEDUpdate = 0;
    if (millis() - lastLEDUpdate < 33) return; 
    lastLEDUpdate = millis();

    uint8_t r = (targetColor_ >> 16) & 0xFF;
    uint8_t g = (targetColor_ >> 8) & 0xFF;
    uint8_t b = targetColor_ & 0xFF;
    
    float brightness = calculateBrightness();
    
    // FLASH-ONCE FIX: Auto-restore previous mood after flash completes
    if (animMode_ == ANIM_FLASH_ONCE && stateTimer_ > 0.5f && !flashRestored_) {
        flashRestored_ = true;
        // Restore previous mood after flash effect
        if (strlen(previousMood_) > 0) {
          Serial.printf("[LED] Flash complete, restoring to: %s\n", previousMood_);
          // Don't call setMood directly to avoid recursion - just update internal state
          char moodToRestore[16];
          strncpy(moodToRestore, previousMood_, 15);
          moodToRestore[15] = '\0';
          previousMood_[0] = '\0'; // Clear to prevent loop
          setMood(moodToRestore);
          return;
        }
    }
    
    uint32_t finalColor = strip_.Color(r * brightness, g * brightness, b * brightness);
    
    // Apply to pixels
    if (partialRing_) {
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i % 3 == 0) strip_.setPixelColor(i, 0);
        else strip_.setPixelColor(i, finalColor);
      }
    } else {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip_.setPixelColor(i, finalColor);
      }
    }
    
    strip_.show();
  }

  void voiceReact(int level) {
    // Throttle voice reaction too
    static unsigned long lastVoiceUpdate = 0;
    if (millis() - lastVoiceUpdate < 50) return;
    lastVoiceUpdate = millis();

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
  char previousMood_[16] = ""; // For restoring after flash
  bool flashRestored_ = false; // Prevent multiple restore calls
  float stateTimer_ = 0.0f;
  
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

  float calculateBrightness() {
    float t = stateTimer_;
    
    switch (animMode_) {
      case ANIM_IDLE_BREATHE:
      case ANIM_GENTLE_PULSE: {
        float wave = (sin(t * (2.0f * PI / cycleDuration_)) + 1.0f) * 0.5f;
        return minBrightness_ + (wave * (maxBrightness_ - minBrightness_));
      }
      case ANIM_SLOW_BREATHE:
      case ANIM_DEEP_BREATHE: {
        float wave = (sin(t * (2.0f * PI / cycleDuration_)) + 1.0f) * 0.5f;
        wave = wave * wave * (3.0f - 2.0f * wave); // Cubic ease
        return minBrightness_ + (wave * (maxBrightness_ - minBrightness_));
      }
      case ANIM_SHARP_PULSE: {
        int pulseNum = (int)(t / cycleDuration_) % 3;
        float pulseTime = fmod(t, cycleDuration_) / cycleDuration_;
        if (pulseNum < 2) return (pulseTime < 0.3f) ? maxBrightness_ : minBrightness_;
        return minBrightness_;
      }
      case ANIM_FLASH_ONCE: {
        if (t < cycleDuration_) return maxBrightness_;
        return minBrightness_;
      }
      case ANIM_STEADY_GLOW: {
        float wave = (sin(t * 3.0f) + 1.0f) * 0.5f;
        return minBrightness_ + (wave * 0.1f); // Tiny breathing
      }
      default: return minBrightness_;
    }
  }
};

#endif
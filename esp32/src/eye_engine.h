#ifndef EYE_ENGINE_H
#define EYE_ENGINE_H

#include <U8g2lib.h>
#include <math.h>
#include "behaviors.h"

class EyeEngine {
public:
  explicit EyeEngine(U8G2& display) : display_(display) {
    randomSeed(analogRead(34));
  }

  void startBootSequence() {
    currentWidth_ = 28;
    currentHeight_ = 40;
    targetWidth_ = 28;
    targetHeight_ = 40;
    
    currentOffsetX_ = 0;
    currentOffsetY_ = 0;
    targetOffsetX_ = 0;
    targetOffsetY_ = 0;
    
    topLid_ = 0.0f;
    bottomLid_ = 0.0f;
    targetTopLid_ = 0.0f;
    targetBottomLid_ = 0.0f;
    
    blinkFactor_ = 1.0f;
    blinkTimer_ = 0.0f;
    nextBlink_ = 4.0f;
    
    saccadeX_ = 0;
    saccadeY_ = 0;
    targetSaccadeX_ = 0;
    targetSaccadeY_ = 0;
    
    activeEffect_ = EFFECT_NONE;
    
    Serial.println("[EYE] Initialized - stable eyes");
    Serial.printf("[EYE] Initial state: w=%.1f h=%.1f topLid=%.2f botLid=%.2f\n",
                  currentWidth_, currentHeight_, topLid_, bottomLid_);
  }

  void setTarget(const Behavior* b) {
    if (!b) {
      Serial.println("[EYE] ERROR: NULL behavior pointer!");
      return;
    }
    
    Serial.printf("[EYE] Setting target for: %s\n", b->name);
    
    // Reset effect timer
    effectTimer_ = 0;
    
    // Map behavior to eye parameters
    if (strcmp(b->name, "happy") == 0 || strcmp(b->name, "shy_happy") == 0) {
      targetWidth_ = 30;
      targetHeight_ = 38;
      targetTopLid_ = 0.0f;
      targetBottomLid_ = 0.45f;  // Smile!
      activeEffect_ = EFFECT_HEART;
      Serial.printf("[EYE] HAPPY: Setting bottomLid to 0.45 for smile\n");
    }
    else if (strcmp(b->name, "sad") == 0) {
      targetWidth_ = 26;
      targetHeight_ = 36;
      targetTopLid_ = 0.4f;  // Droopy top
      targetBottomLid_ = 0.0f;
      activeEffect_ = EFFECT_NONE;
      Serial.printf("[EYE] SAD: Setting topLid to 0.4 for droop\n");
    }
    else if (strcmp(b->name, "surprised") == 0 || strcmp(b->name, "startled") == 0) {
      targetWidth_ = 32;
      targetHeight_ = 48;
      targetTopLid_ = 0.0f;
      targetBottomLid_ = 0.0f;
      activeEffect_ = EFFECT_STARS;
      Serial.printf("[EYE] SURPRISED: Wide open\n");
    }
    else if (strcmp(b->name, "curious_idle") == 0) {
      targetWidth_ = 30;
      targetHeight_ = 42;
      targetTopLid_ = 0.0f;
      targetBottomLid_ = 0.0f;
      activeEffect_ = EFFECT_SPARKLE;
      Serial.printf("[EYE] CURIOUS: Alert and sparkly\n");
    }
    else if (strcmp(b->name, "sleepy_idle") == 0) {
      targetWidth_ = 28;
      targetHeight_ = 20;
      targetTopLid_ = 0.40f;
      targetBottomLid_ = 0.0f;
      activeEffect_ = EFFECT_ZZZ;
      Serial.printf("[EYE] SLEEPY: Half closed\n");
    }
    else if (strcmp(b->name, "sleeping") == 0) {
      targetWidth_ = 28;
      targetHeight_ = 6;
      targetTopLid_ = 0.85f;
      targetBottomLid_ = 0.85f;
      activeEffect_ = EFFECT_ZZZ;
      Serial.printf("[EYE] SLEEPING: Almost closed\n");
    }
    else if (strcmp(b->name, "confused") == 0) {
      targetWidth_ = 28;
      targetHeight_ = 38;
      targetTopLid_ = 0.2f;
      targetBottomLid_ = 0.0f;
      activeEffect_ = EFFECT_QUESTION;
      Serial.printf("[EYE] CONFUSED: Slightly tilted\n");
    }
    else if (strcmp(b->name, "thinking") == 0) {
      targetWidth_ = 28;
      targetHeight_ = 38;
      targetTopLid_ = 0.0f;
      targetBottomLid_ = 0.3f;
      activeEffect_ = EFFECT_THINKING_DOTS;
      Serial.printf("[EYE] THINKING: Concentrated\n");
    }
    else if (strcmp(b->name, "listening") == 0) {
      targetWidth_ = 30;
      targetHeight_ = 44;
      targetTopLid_ = 0.0f;
      targetBottomLid_ = 0.0f;
      activeEffect_ = EFFECT_SCAN_BEAM;
      Serial.printf("[EYE] LISTENING: Alert and scanning\n");
    }
    else if (strcmp(b->name, "playful_mischief") == 0) {
      targetWidth_ = 28;
      targetHeight_ = 36;
      targetTopLid_ = 0.0f;
      targetBottomLid_ = 0.3f;
      activeEffect_ = EFFECT_SPARKLE;
      Serial.printf("[EYE] PLAYFUL: Mischievous look\n");
    }
    else {
      // calm_idle or unknown
      targetWidth_ = 28;
      targetHeight_ = 40;
      targetTopLid_ = 0.0f;
      targetBottomLid_ = 0.0f;
      activeEffect_ = EFFECT_NONE;
      Serial.printf("[EYE] CALM/DEFAULT: Neutral\n");
    }
    
    targetOffsetX_ = b->offsetX;
    targetOffsetY_ = b->offsetY;
    
    Serial.printf("[EYE] Target set: w=%.1f h=%.1f top=%.2f bot=%.2f offset=(%.1f,%.1f) effect=%d\n",
                  targetWidth_, targetHeight_, targetTopLid_, targetBottomLid_,
                  targetOffsetX_, targetOffsetY_, (int)activeEffect_);
  }

  void update(float dt) {
    // Smooth morphing
    const float f = 0.15f;
    currentWidth_ += (targetWidth_ - currentWidth_) * f;
    currentHeight_ += (targetHeight_ - currentHeight_) * f;
    currentOffsetX_ += (targetOffsetX_ - currentOffsetX_) * f;
    currentOffsetY_ += (targetOffsetY_ - currentOffsetY_) * f;
    
    // CRITICAL: Smooth lid transitions
    topLid_ += (targetTopLid_ - topLid_) * f;
    bottomLid_ += (targetBottomLid_ - bottomLid_) * f;

    // Smooth saccade
    saccadeX_ += (targetSaccadeX_ - saccadeX_) * 0.2f;
    saccadeY_ += (targetSaccadeY_ - saccadeY_) * 0.2f;

    // Update effect timer
    effectTimer_ += dt;
    
    // Blink only in neutral calm state
    bool isNeutralCalm = (targetTopLid_ < 0.05f && 
                          targetBottomLid_ < 0.05f && 
                          activeEffect_ == EFFECT_NONE);
    
    if (isNeutralCalm) {
      updateBlink(dt);
    } else {
      blinkFactor_ = 1.0f;
      blinkTimer_ = 0.0f;
    }
    
    // Saccades only in neutral calm state
    if (isNeutralCalm && targetHeight_ > 35) {
      updateSaccades(dt);
    } else {
      targetSaccadeX_ = 0;
      targetSaccadeY_ = 0;
    }
    
    // Debug output every second
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 1000) {
      Serial.printf("[EYE] Current: w=%.1f h=%.1f topLid=%.2f/%.2f botLid=%.2f/%.2f blink=%.2f effect=%d\n",
                    currentWidth_, currentHeight_, 
                    topLid_, targetTopLid_,
                    bottomLid_, targetBottomLid_,
                    blinkFactor_, (int)activeEffect_);
      lastDebug = millis();
    }
  }

  void render() {
    display_.clearBuffer();
    
    drawEye(32, 32, false);  // Left eye
    drawEye(96, 32, true);   // Right eye with effects
    
    display_.sendBuffer();
  }

private:
  U8G2& display_;
  
  float currentWidth_ = 28;
  float currentHeight_ = 40;
  float targetWidth_ = 28;
  float targetHeight_ = 40;
  
  float currentOffsetX_ = 0;
  float currentOffsetY_ = 0;
  float targetOffsetX_ = 0;
  float targetOffsetY_ = 0;
  
  float topLid_ = 0.0f;
  float bottomLid_ = 0.0f;
  float targetTopLid_ = 0.0f;
  float targetBottomLid_ = 0.0f;
  
  float blinkFactor_ = 1.0f;
  float blinkTimer_ = 0.0f;
  float nextBlink_ = 4.0f;
  
  float saccadeX_ = 0;
  float saccadeY_ = 0;
  float saccadeTimer_ = 0;
  float targetSaccadeX_ = 0; 
  float targetSaccadeY_ = 0;
  
  enum EffectType {
    EFFECT_NONE,
    EFFECT_HEART,
    EFFECT_STARS,
    EFFECT_SPARKLE,
    EFFECT_ZZZ,
    EFFECT_QUESTION,
    EFFECT_THINKING_DOTS,
    EFFECT_SCAN_BEAM
  } activeEffect_ = EFFECT_NONE;
  
  float effectTimer_ = 0;

  void updateBlink(float dt) {
    blinkTimer_ += dt;
    
    if (blinkTimer_ > nextBlink_) {
      float seq = blinkTimer_ - nextBlink_;
      
      if (seq < 0.15f) {
        float t = seq / 0.15f;
        t = t * t * (3.0f - 2.0f * t);
        blinkFactor_ = 1.0f - t;
      }
      else if (seq < 0.20f) {
        blinkFactor_ = 0.0f;
      }
      else if (seq < 0.40f) {
        float t = (seq - 0.20f) / 0.20f;
        t = t * t * (3.0f - 2.0f * t);
        blinkFactor_ = t;
      }
      else {
        blinkTimer_ = 0;
        nextBlink_ = 3.5f + (random(30) / 10.0f);
        if (random(100) < 6) nextBlink_ = 0.5f;
        blinkFactor_ = 1.0f;
      }
    }
  }

  void updateSaccades(float dt) {
    saccadeTimer_ += dt;
    
    if (saccadeTimer_ > 2.5f) {
      if (random(100) < 30) {
        saccadeTimer_ = 0;
        targetSaccadeX_ = random(31) - 15; 
        targetSaccadeY_ = random(9) - 4;   
      } 
      else if (random(100) < 10) {
        targetSaccadeX_ = 0;
        targetSaccadeY_ = 0;
        saccadeTimer_ = 0;
      }
    }
  }
  
  void drawEye(int cx, int cy, bool drawEffects) {
    // Calculate eye dimensions (blink affects height only)
    int w = constrain((int)currentWidth_, 20, 40);
    int h = constrain((int)(currentHeight_ * blinkFactor_), 4, 52);
    
    // Calculate position
    int totalOffsetX = constrain((int)(currentOffsetX_ + saccadeX_), -10, 10);
    int totalOffsetY = constrain((int)(currentOffsetY_ + saccadeY_), -8, 8);
    
    int x = cx + totalOffsetX - (w / 2);
    int y = cy + totalOffsetY - (h / 2);
    
    // Keep on screen
    x = constrain(x, 0, 128 - w);
    y = constrain(y, 0, 64 - h);
    
    // Dynamic radius to prevent glitches when eye is very small
    int r = (h < 16) ? (h / 2) : 8;
    
    // STEP 1: Draw white eye base
    display_.setDrawColor(1);
    display_.drawRBox(x, y, w, h, r);
    
    // STEP 2: Draw BLACK eyelid overlays
    display_.setDrawColor(0);
    
    // Top lid (covers from top downward)
    int topH = (int)(h * topLid_);
    if (topH > 0) {
      display_.drawBox(x, y, w, topH);
    }
    
    // Bottom lid (covers from bottom upward)
    int botH = (int)(h * bottomLid_);
    if (botH > 0) {
      display_.drawBox(x, y + h - botH, w, botH);
    }
    
    // Debug: Print lid calculations
    static unsigned long lastLidDebug = 0;
    if (millis() - lastLidDebug > 2000 && drawEffects) {
      Serial.printf("[EYE RENDER] Eye at (%d,%d) size %dx%d, topLid=%dpx (%.2f), botLid=%dpx (%.2f)\n",
                    x, y, w, h, topH, topLid_, botH, bottomLid_);
      lastLidDebug = millis();
    }
    
    // STEP 3: Draw effects on right eye only
    if (drawEffects && activeEffect_ != EFFECT_NONE) {
      renderEffect(cx, cy);
    }
  }

  void renderEffect(int cx, int cy) {
    display_.setDrawColor(1);

    switch (activeEffect_) {
      case EFFECT_ZZZ: {
        display_.setFont(u8g2_font_ncenB08_tr);  // Use guaranteed font
        
        float t1 = fmod(effectTimer_, 3.0f);
        float y1 = 25.0f - (t1 * 8.0f);
        if (y1 > 0 && y1 < 50) {
          display_.drawStr(cx + 10, (int)y1, "z");
        }

        float t2 = fmod(effectTimer_ + 1.0f, 3.0f);
        float y2 = 25.0f - (t2 * 8.0f);
        if (y2 > 0 && y2 < 50) {
          display_.drawStr(cx + 18, (int)y2, "Z");
        }

        float t3 = fmod(effectTimer_ + 2.0f, 3.0f);
        float y3 = 25.0f - (t3 * 8.0f);
        if (y3 > 0 && y3 < 50) {
          display_.drawStr(cx + 26, (int)y3, "z");
        }
        break;
      }
      
      case EFFECT_HEART: {
        display_.setFont(u8g2_font_ncenB08_tr);
        // Slower, more visible heart
        int heartY = 50 - (int)(fmod(effectTimer_ * 8.0f, 40.0f));
        if (heartY > 10 && heartY < 55) {
          display_.drawStr(cx + 18, heartY, "<3");
        }
        break;
      }
      
      case EFFECT_STARS: {
        // Persistent stars (not animated, just there)
        display_.setFont(u8g2_font_ncenB08_tr);
        display_.drawStr(cx + 12, cy - 8, "*");
        display_.drawStr(cx + 22, cy + 8, "*");
        display_.drawStr(cx + 18, cy + 18, "*");
        break;
      }
      
      case EFFECT_SPARKLE: {
        // Blinking sparkles
        if ((int)(effectTimer_ * 4) % 2 == 0) {
          display_.drawDisc(cx + 18, cy - 10, 2);
          display_.drawDisc(cx + 25, cy - 5, 2);
          display_.drawDisc(cx + 20, cy + 10, 2);
        }
        break;
      }

      case EFFECT_QUESTION: {
        display_.setFont(u8g2_font_ncenB08_tr);
        int qY = 50 - (int)(fmod(effectTimer_ * 12.0f, 35.0f));
        if (qY > 8 && qY < 58) {
          display_.drawStr(cx + 15, qY, "?");
        }
        break;
      }
      
      case EFFECT_THINKING_DOTS: {
        int dotCount = ((int)(effectTimer_ * 2) % 3) + 1;
        for (int i = 0; i < dotCount; i++) {
          display_.drawDisc(cx + 15 + (i * 6), cy - 8, 2);
        }
        break;
      }
      
      case EFFECT_SCAN_BEAM: {
        int beamY = 20 + (int)(sin(effectTimer_ * 3) * 12);
        display_.drawBox(cx - 12, beamY, 24, 2);
        break;
      }
      
      default:
        break;
    }
  }
};

#endif
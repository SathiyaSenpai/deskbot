#ifndef EYE_ENGINE_H
#define EYE_ENGINE_H

#include <U8g2lib.h>
#include <math.h>
#include "behaviors.h"

struct EyeState {
  float openness = 1.0f;
  float scaleX = 1.0f;
  float bottomLid = 0.5f;
  float offsetX = 0.0f;
  float offsetY = 0.0f;

  float targetOpenness = 1.0f;
  float targetScaleX = 1.0f;
  float targetBottomLid = 0.5f;
  float targetOffsetX = 0.0f;
  float targetOffsetY = 0.0f;
};

class EyeEngine {
public:
  explicit EyeEngine(U8G2& display) : display_(display) {}

  void setBehavior(const char* name) {
    const Behavior* b = findBehavior(name);
    if (!b) return;
    currentBehavior_ = b->name;
    state_.targetOpenness = b->openness;
    state_.targetScaleX = b->scaleX;
    state_.targetBottomLid = b->bottomLid;
    state_.targetOffsetX = b->offsetX;
    state_.targetOffsetY = b->offsetY;
  }

  const char* getBehavior() const { return currentBehavior_; }

  void update(float dt) {
    // Exponential smoothing (same formula as demo)
    const float smoothing = 12.0f;
    const float factor = 1.0f - expf(-smoothing * dt);

    state_.openness    += (state_.targetOpenness    - state_.openness)    * factor;
    state_.scaleX      += (state_.targetScaleX      - state_.scaleX)      * factor;
    state_.bottomLid   += (state_.targetBottomLid   - state_.bottomLid)   * factor;
    state_.offsetX     += (state_.targetOffsetX     - state_.offsetX)     * factor;
    state_.offsetY     += (state_.targetOffsetY     - state_.offsetY)     * factor;
  }

  void render() {
    display_.clearBuffer();

    // Eye geometry (128x64 OLED). Two eyes, each ~46x48 with gap.
    const int eyeWidth = 46;
    const int eyeHeight = 48;
    const int centerY = 32;
    const int leftCenterX = 38;
    const int rightCenterX = 90;

    // Offset scale: demo eye 96x112 -> preview 41x48 used 0.43; keep 0.43 here too
    const float offsetScale = 0.43f;
    const float dx = state_.offsetX * offsetScale;
    const float dy = state_.offsetY * offsetScale;

    // Scale transforms
    const float sx = state_.scaleX;
    const float sy = state_.openness;

    drawEye(leftCenterX + dx, centerY + dy, eyeWidth, eyeHeight, sx, sy, /*isHappy*/ isHappy());
    drawEye(rightCenterX + dx, centerY + dy, eyeWidth, eyeHeight, sx, sy, /*isHappy*/ isHappy());

    display_.sendBuffer();
  }

private:
  U8G2& display_;
  EyeState state_;
  const char* currentBehavior_ = "calm_idle";

  bool isHappy() const {
    return (strcmp(currentBehavior_, "happy") == 0) && (state_.bottomLid > 0.5f);
  }

  void drawEye(int cx, int cy, int w, int h, float sx, float sy, bool happy) {
    // Compute scaled dimensions
    const int scaledW = (int)(w * sx);
    const int scaledH = (int)(h * sy);
    const int x = cx - scaledW / 2;
    const int y = cy - scaledH / 2;

    // Draw white eye fill (rect with rounded corners)
    display_.setDrawColor(1);
    display_.drawRBox(x, y, scaledW, scaledH, 6);

    // Draw pupil (fixed small rectangle in center)
    const int pupilW = scaledW / 5;
    const int pupilH = scaledH / 3;
    const int pupilX = cx - pupilW / 2;
    const int pupilY = cy - pupilH / 2;
    display_.setDrawColor(0);
    display_.drawBox(pupilX, pupilY, pupilW, pupilH);

    // Happy smile (bottom arc) only for happy
    if (happy) {
      const int smileY = y + scaledH - 4;
      display_.setDrawColor(0);
      display_.drawCircle(cx - scaledW / 4, smileY, 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
      display_.drawCircle(cx + scaledW / 4, smileY, 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    }
  }
};

#endif // EYE_ENGINE_H

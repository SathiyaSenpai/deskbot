#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <ESP32Servo.h>
#include "pins.h"

// ============================================================================
// SERVO CONTROLLER - Non-blocking direct control
// No delay() calls — WiFi, WebSockets, and Audio keep running smoothly
// ============================================================================

class ServoController {
public:
  void begin() {
    servo_.setPeriodHertz(50);
    servo_.attach(PIN_SERVO, 500, 2400); // Full SG90 range
    servo_.write(90); // Center position
    currentAngle_ = 90;
    gestureActive_ = false;
    Serial.printf("[SERVO] Initialized on pin %d at 90°\n", PIN_SERVO);
  }

  void setAngle(int angle) {
    if (angle < 0)   angle = 0;
    if (angle > 180) angle = 180;
    currentAngle_ = angle;
    gestureActive_ = false; // Cancel any active gesture
    servo_.write(angle);
  }

  void setTarget(int angle, unsigned long /*returnDelay*/ = 0) {
    setAngle(angle);
  }

  void setIdleMovement(int angle, unsigned long /*returnDelay*/ = 3000) {
    setAngle(angle);
  }

  void returnToCenter() {
    setAngle(90);
  }

  // Non-blocking gesture trigger
  void triggerGesture(const char* name, unsigned long /*behaviorDuration*/ = 3000) {
    if (strcmp(name, "shake") == 0 || strcmp(name, "nod") == 0) {
      gestureActive_ = true;
      gestureStep_ = 0;
      lastStepTime_ = millis();
      servo_.write(75);
      currentAngle_ = 75;
    } else if (strcmp(name, "tilt") == 0) {
      setAngle(105);
    }
  }

  bool isMoving() { 
    return gestureActive_; 
  }

  // Non-blocking loop update
  void loop(float /*dt*/) {
    if (!gestureActive_) return;

    unsigned long now = millis();
    if (now - lastStepTime_ >= 200) {
      lastStepTime_ = now;
      gestureStep_++;

      if (gestureStep_ == 1) {
        servo_.write(105);
        currentAngle_ = 105;
      } else if (gestureStep_ == 2) {
        servo_.write(75);
        currentAngle_ = 75;
      } else {
        servo_.write(90);
        currentAngle_ = 90;
        gestureActive_ = false;
      }
    }
  }

  float getCurrentAngle() { return (float)currentAngle_; }

private:
  Servo servo_;
  int currentAngle_ = 90;
  bool gestureActive_ = false;
  int gestureStep_ = 0;
  unsigned long lastStepTime_ = 0;
};

#endif

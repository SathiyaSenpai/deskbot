#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <ESP32Servo.h>
#include "pins.h"

// ============================================================================
// SERVO CONTROLLER - Optimized for Cardboard Body (Left/Right Only)
// ============================================================================
class ServoController {
public:
  void begin() {
    servo_.setPeriodHertz(50); 
    servo_.attach(PIN_SERVO, 600, 2300); 
    targetAngle_ = 90.0f;
    currentAngle_ = 90.0f;
    servo_.write(90);
    Serial.printf("[SERVO] Initialized on pin %d at 90° (Range: 60-120° for cardboard)\n", PIN_SERVO);
  }

  // Set target angle with behavior-synced auto-return
  void setTarget(int angle, unsigned long returnDelay = 0) {
    // CARDBOARD SAFE: Constrain to 60-120° to prevent tearing
    targetAngle_ = constrain(angle, 60, 120);
    
    // Set up auto-return if delay specified
    if (returnDelay > 0) {
      returnToCenter_ = true;
      returnTime_ = millis() + returnDelay;
    }
    
    // CRITICAL FIX: Don't reset moveState if a gesture is active
    if (moveState_ == MOVE_IDLE) {
      Serial.printf("[SERVO] Target set: %d° (return in %lums)\n", angle, returnDelay);
    }
  }
  
  // Overload for simple calls without return delay
  void setTarget(int angle) {
    setTarget(angle, 0);
  }

  // Trigger behavior-synced gestures (cardboard-optimized: left-right only)
  void triggerGesture(const char* name, unsigned long behaviorDuration = 3000) {
    Serial.printf("[SERVO] Gesture triggered: %s (behavior duration: %lums)\n", name, behaviorDuration);
    
    // NOTE: "nod" removed - requires vertical movement not possible with cardboard
    if (strcmp(name, "nod") == 0) {
      // Convert nod to a gentle shake for cardboard compatibility
      moveState_ = MOVE_SHAKE;
      gestureTimer_ = 0;
      gestureSpeed_ = 10.0f; // Slower for "nod" converted to gentle shake
      gestureDuration_ = 0.5f;
      returnTime_ = millis() + behaviorDuration;
      returnToCenter_ = true;
      Serial.println("[SERVO] NOD converted to gentle SHAKE for cardboard");
    } 
    else if (strcmp(name, "shake") == 0) {
      moveState_ = MOVE_SHAKE;
      gestureTimer_ = 0;
      gestureSpeed_ = 15.0f;
      gestureDuration_ = 0.8f;
      returnTime_ = millis() + behaviorDuration;
      returnToCenter_ = true;
      Serial.println("[SERVO] SHAKE gesture starting");
    } 
    else if (strcmp(name, "tilt") == 0) {
      // Tilt right (curious look)
      targetAngle_ = 105; 
      moveState_ = MOVE_IDLE;
      returnTime_ = millis() + behaviorDuration;
      returnToCenter_ = true;
      Serial.println("[SERVO] TILT to 105° (cardboard safe)");
    }
  }

  void loop(float dt) {
    unsigned long now = millis();
    
    // 1. Handle auto-return to center
    if (returnToCenter_ && now >= returnTime_ && moveState_ == MOVE_IDLE) {
      if (abs(targetAngle_ - 90.0f) > 2.0f) {
        Serial.println("[SERVO] Auto-returning to center (90°)");
        targetAngle_ = 90.0f;
      }
      returnToCenter_ = false;
    }
    
    // 2. Procedural Gestures (left-right only for cardboard)
    if (moveState_ != MOVE_IDLE) {
      gestureTimer_ += dt;
      
      if (moveState_ == MOVE_SHAKE) {
        // Shake: side to side within safe range
        float wave = cos(gestureTimer_ * gestureSpeed_) * 15.0f; // ±15° from center
        targetAngle_ = constrain(90.0f + wave, 60.0f, 120.0f);
        
        if (gestureTimer_ > gestureDuration_) {
          moveState_ = MOVE_IDLE;
          // Don't immediately return - let the behavior timing handle it
          Serial.println("[SERVO] SHAKE complete, waiting for behavior timeout");
        }
      }
    }

    // 3. Smooth Movement Physics
    float diff = targetAngle_ - currentAngle_;
    
    // Faster movement for large differences, smoother for small
    float speed = (abs(diff) > 15) ? 10.0f : 5.0f; 
    
    if (abs(diff) > 0.5f) {
      currentAngle_ += diff * (speed * dt);
      currentAngle_ = constrain(currentAngle_, 60.0f, 120.0f); // Safety clamp
      servo_.write((int)currentAngle_);
      
      // Debug output for movement
      static unsigned long lastDebug = 0;
      if (millis() - lastDebug > 500) {
        Serial.printf("[SERVO] Moving: current=%.1f° target=%.1f°\n", 
                     currentAngle_, targetAngle_);
        lastDebug = millis();
      }
    }
  }

  // Check if servo is currently moving
  bool isMoving() {
    return (moveState_ != MOVE_IDLE) || (abs(targetAngle_ - currentAngle_) > 2.0f);
  }

  // Get current angle for debugging
  float getCurrentAngle() {
    return currentAngle_;
  }
  
  // Force return to center (called when behavior ends)
  void returnToCenter() {
    targetAngle_ = 90.0f;
    returnToCenter_ = false;
    moveState_ = MOVE_IDLE;
    Serial.println("[SERVO] Forced return to center");
  }

private:
  Servo servo_;
  float currentAngle_ = 90.0f;
  float targetAngle_ = 90.0f;
  float gestureTimer_ = 0.0f;
  float gestureSpeed_ = 15.0f;
  float gestureDuration_ = 0.8f;
  
  // Auto-return system
  bool returnToCenter_ = false;
  unsigned long returnTime_ = 0;
  
  enum MoveState { 
    MOVE_IDLE, 
    MOVE_SHAKE 
  } moveState_ = MOVE_IDLE;
};

#endif
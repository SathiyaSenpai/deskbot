#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <ESP32Servo.h>
#include "pins.h"

class ServoController {
public:
  void begin() {
    servo_.setPeriodHertz(50); 
    servo_.attach(PIN_SERVO, 600, 2300); 
    targetAngle_ = 90.0f;
    currentAngle_ = 90.0f;
    servo_.write(90);
    Serial.printf("[SERVO] Initialized on pin %d at 90°\n", PIN_SERVO);
  }

  void setTarget(int angle) {
    targetAngle_ = constrain(angle, 15, 165);
    // CRITICAL FIX: Don't reset moveState if a gesture is active
    if (moveState_ == MOVE_IDLE) {
      Serial.printf("[SERVO] Target set: %d°\n", angle);
    }
  }

  void triggerGesture(const char* name) {
    Serial.printf("[SERVO] Gesture triggered: %s\n", name);
    
    if (strcmp(name, "nod") == 0) {
      moveState_ = MOVE_NOD;
      gestureTimer_ = 0;
      Serial.println("[SERVO] NOD gesture starting");
    } 
    else if (strcmp(name, "shake") == 0) {
      moveState_ = MOVE_SHAKE;
      gestureTimer_ = 0;
      Serial.println("[SERVO] SHAKE gesture starting");
    } 
    else if (strcmp(name, "tilt") == 0) {
      targetAngle_ = 110; 
      moveState_ = MOVE_IDLE;
      Serial.println("[SERVO] TILT to 110°");
    }
  }

  void loop(float dt) {
    // 1. Procedural Gestures
    if (moveState_ != MOVE_IDLE) {
      gestureTimer_ += dt;
      
      if (moveState_ == MOVE_NOD) {
        // Nod: up and down motion
        float wave = sin(gestureTimer_ * 12.0f) * 15.0f; 
        targetAngle_ = 90.0f + wave;
        
        if (gestureTimer_ > 0.6f) { 
          moveState_ = MOVE_IDLE;
          targetAngle_ = 90.0f;
          Serial.println("[SERVO] NOD complete");
        }
      } 
      else if (moveState_ == MOVE_SHAKE) {
        // Shake: side to side
        float wave = cos(gestureTimer_ * 15.0f) * 20.0f;
        targetAngle_ = 90.0f + wave;
        
        if (gestureTimer_ > 0.8f) {
          moveState_ = MOVE_IDLE;
          targetAngle_ = 90.0f;
          Serial.println("[SERVO] SHAKE complete");
        }
      }
    }

    // 2. Smooth Movement Physics
    float diff = targetAngle_ - currentAngle_;
    
    // Faster movement for large differences
    float speed = (abs(diff) > 20) ? 12.0f : 6.0f; 
    
    if (abs(diff) > 0.5f) {
      currentAngle_ += diff * (speed * dt);
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

  // NEW: Check if servo is currently moving
  bool isMoving() {
    return (moveState_ != MOVE_IDLE) || (abs(targetAngle_ - currentAngle_) > 2.0f);
  }

  // NEW: Get current angle for debugging
  float getCurrentAngle() {
    return currentAngle_;
  }

private:
  Servo servo_;
  float currentAngle_ = 90.0f;
  float targetAngle_ = 90.0f;
  float gestureTimer_ = 0.0f;
  
  enum MoveState { 
    MOVE_IDLE, 
    MOVE_NOD, 
    MOVE_SHAKE 
  } moveState_ = MOVE_IDLE;
};

#endif
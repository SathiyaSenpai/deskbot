#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "pins.h"

struct SensorData {
  uint16_t light = 0;
  bool motion = false;
  uint16_t distance_mm = 0;
  bool touchHead = false;
  bool touchSide = false;
  int soundLevel = 0; 
};

class SensorManager {
public:
  void begin() {
    pinMode(PIN_PIR, INPUT);
    pinMode(PIN_LDR, INPUT);
    pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_ECHO, INPUT);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    
    Serial.println("[SENSORS] Initialized (Simple ultrasonic)");
    Serial.printf("  PIR: %d\n", PIN_PIR);
    Serial.printf("  Ultrasonic: Trig=%d, Echo=%d\n", PIN_ULTRASONIC_TRIG, PIN_ULTRASONIC_ECHO);
    Serial.printf("  Touch: Head=%d, Side=%d, Threshold=%d\n", PIN_TOUCH_HEAD, PIN_TOUCH_SIDE, TOUCH_THRESHOLD);
    
    // Test ultrasonic sensor immediately
    Serial.println("[ULTRASONIC] Running connection test...");
    testUltrasonicConnection();
  }

  void update() {
    // Update distance reading every 200ms (throttled)
    static unsigned long lastDistanceRead = 0;
    if (millis() - lastDistanceRead > 200) {
      lastDistanceRead = millis();
      lastDistance_ = readDistanceSimple();
    }
  }

  SensorData read() {
    SensorData d;
    
    // 1. Read ambient sensors (fast, non-blocking)
    d.light = analogRead(PIN_LDR);
    d.motion = digitalRead(PIN_PIR) == HIGH;
    
    // 2. Read TOUCH with simpler verification
    int touchHead = touchRead(PIN_TOUCH_HEAD);
    int touchSide = touchRead(PIN_TOUCH_SIDE);
    
    d.touchHead = (touchHead < TOUCH_THRESHOLD);
    d.touchSide = (touchSide < TOUCH_THRESHOLD);
    
    // Debug output for touch calibration (less frequent)
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 3000) {
      Serial.printf("[TOUCH] Head: %d, Side: %d (threshold: %d)\n", 
                    touchHead, touchSide, TOUCH_THRESHOLD);
      lastDebug = millis();
    }

    // 3. Use cached distance (updated by update() method)
    d.distance_mm = lastDistance_;
    
    d.soundLevel = 0; // Will be set separately if mic enabled
    return d;
  }

private:
  uint16_t lastDistance_ = 0;

  void testUltrasonicConnection() {
    // Test if sensor is connected properly
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
      delayMicroseconds(2);
      digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
      delayMicroseconds(10);
      digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
      
      unsigned long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 10000); // 10ms timeout
      
      Serial.printf("[ULTRASONIC] Test %d: duration=%lu us", i+1, duration);
      
      if (duration > 0) {
        uint16_t distance = (duration * 343) / 2000;
        Serial.printf(" -> %d mm\n", distance);
      } else {
        Serial.println(" -> NO ECHO");
      }
      delay(100);
    }
  }

  uint16_t readDistanceSimple() {
    // Simple single reading with short timeout to minimize freeze
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    
    // Shorter timeout (5ms) to reduce potential freeze
    unsigned long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 5000);
    
    if (duration > 0 && duration < 20000) { // Valid range check
      uint16_t distance = (duration * 343) / 2000;
      
      // Filter out unrealistic values
      if (distance >= 5 && distance <= 400) { // 5mm to 400cm range
        static unsigned long lastGoodReading = 0;
        static uint16_t lastGoodDistance = 0;
        
        // Debug output for good readings
        if (millis() - lastGoodReading > 1000) {
          if (abs((int)distance - (int)lastGoodDistance) > 20) {
            Serial.printf("[ULTRASONIC] Distance: %d mm\n", distance);
            lastGoodReading = millis();
            lastGoodDistance = distance;
          }
        }
        
        return distance;
      }
    }
    
    // Return 0 for invalid readings
    return 0;
  }
};

#endif
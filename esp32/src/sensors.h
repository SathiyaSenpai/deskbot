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
    pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_ECHO, INPUT);
    pinMode(PIN_LDR, INPUT);
    
    // Initialize ultrasonic to LOW
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    
    Serial.println("[SENSORS] Initialized");
    Serial.printf("  PIR: %d\n", PIN_PIR);
    Serial.printf("  Ultrasonic: Trig=%d, Echo=%d\n", PIN_ULTRASONIC_TRIG, PIN_ULTRASONIC_ECHO);
    Serial.printf("  Touch: Head=%d, Side=%d, Threshold=%d\n", PIN_TOUCH_HEAD, PIN_TOUCH_SIDE, TOUCH_THRESHOLD);
  }

  // FIXED: Improved reading sequence with better debounce
  SensorData read() {
    SensorData d;
    
    // 1. Read ambient sensors first (no interference)
    d.light = analogRead(PIN_LDR);
    d.motion = digitalRead(PIN_PIR) == HIGH;
    
    // 2. Read TOUCH with triple verification (eliminate false triggers)
    int touchHead1 = touchRead(PIN_TOUCH_HEAD);
    int touchSide1 = touchRead(PIN_TOUCH_SIDE);
    
    delayMicroseconds(500);
    
    int touchHead2 = touchRead(PIN_TOUCH_HEAD);
    int touchSide2 = touchRead(PIN_TOUCH_SIDE);
    
    delayMicroseconds(500);
    
    int touchHead3 = touchRead(PIN_TOUCH_HEAD);
    int touchSide3 = touchRead(PIN_TOUCH_SIDE);
    
    // All three readings must be below threshold
    d.touchHead = (touchHead1 < TOUCH_THRESHOLD && 
                   touchHead2 < TOUCH_THRESHOLD && 
                   touchHead3 < TOUCH_THRESHOLD);
    
    d.touchSide = (touchSide1 < TOUCH_THRESHOLD && 
                   touchSide2 < TOUCH_THRESHOLD && 
                   touchSide3 < TOUCH_THRESHOLD);
    
    // Debug output for touch calibration
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 2000) {
      Serial.printf("[TOUCH] Head: %d/%d/%d (avg:%d), Side: %d/%d/%d (avg:%d)\n", 
                    touchHead1, touchHead2, touchHead3, (touchHead1+touchHead2+touchHead3)/3,
                    touchSide1, touchSide2, touchSide3, (touchSide1+touchSide2+touchSide3)/3);
      lastDebug = millis();
    }

    // 3. Read DISTANCE LAST (ultrasonic generates noise)
    d.distance_mm = measureDistance();
    
    
    d.soundLevel = 0; // Will be set separately if mic enabled
    return d;

  }

  uint16_t measureDistance() {
    // FIXED: Improved ultrasonic measurement with timeout and averaging
    const int numReadings = 3;
    long totalDuration = 0;
    int validReadings = 0;
    
    for (int i = 0; i < numReadings; i++) {
      digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
      delayMicroseconds(2);
      digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
      delayMicroseconds(10);
      digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
      
      // Timeout after 30ms (max ~5m range)
      long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 30000);
      
      if (duration > 0 && duration < 25000) {  // Valid range: 4mm to 4m
        totalDuration += duration;
        validReadings++;
      }
      
      if (i < numReadings - 1) delayMicroseconds(100);
    }
    
    if (validReadings == 0) return 0;  // No valid reading
    
    // Calculate average distance
    long avgDuration = totalDuration / validReadings;
    uint16_t distance = (avgDuration * 343) / 2000;  // Speed of sound: 343 m/s
    
    // Filter unrealistic values
    if (distance < 20 || distance > 4000) return 0;
    
    // Debug output
    static unsigned long lastDistDebug = 0;
    static uint16_t lastDist = 0;
    if (millis() - lastDistDebug > 1000 && abs(distance - lastDist) > 50) {
      Serial.printf("[ULTRASONIC] Distance: %d mm (%d valid readings)\n", distance, validReadings);
      lastDistDebug = millis();
      lastDist = distance;
    }
    
    return distance;
  }

  bool isTriggered() {
    SensorData data = read();
    return data.motion || data.touchHead || data.touchSide || data.distance_mm > 0;
  }
};

#endif
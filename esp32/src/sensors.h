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
  float temperature = 0.0f;
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
    Serial.printf("  Touch: Head=%d, Side=%d, Threshold=%d\n", PIN_TOUCH_HEAD, PIN_TOUCH_SIDE, ROBOT_TOUCH_THRESHOLD);
    
    // Test ultrasonic sensor immediately
    Serial.println("[ULTRASONIC] Running connection test...");
    testUltrasonicConnection();
  }

  void update(bool sleepMode = false) {
    // Update distance reading - throttled based on mode
    // In sleep mode: read every 5 seconds (still detect, but reduce power/freezes)
    // In active mode: read every 200ms for responsiveness
    static unsigned long lastDistanceRead = 0;
    unsigned long readInterval = sleepMode ? 5000 : 200;
    if (millis() - lastDistanceRead > readInterval) {
      lastDistanceRead = millis();
      lastDistance_ = readDistanceSimple();
    }
  }

  SensorData read(float rtcTemp = 0.0f) {
    SensorData d;
    
    // 1. Read ambient sensors (fast, non-blocking)
    d.light = analogRead(PIN_LDR);
    d.motion = digitalRead(PIN_PIR) == HIGH;
    
    // 2. Read TOUCH with 2-sample debouncing for direct ESP32 jumper wire capacitance
    int touchHead1 = touchRead(PIN_TOUCH_HEAD);
    int touchSide1 = touchRead(PIN_TOUCH_SIDE);
    delayMicroseconds(500); // Brief delay for sample consistency
    int touchHead2 = touchRead(PIN_TOUCH_HEAD);
    int touchSide2 = touchRead(PIN_TOUCH_SIDE);
    
    d.touchHead = (touchHead1 < ROBOT_TOUCH_THRESHOLD) && (touchHead2 < ROBOT_TOUCH_THRESHOLD);
    d.touchSide = (touchSide1 < ROBOT_TOUCH_THRESHOLD) && (touchSide2 < ROBOT_TOUCH_THRESHOLD);
    
#if DEBUG_VERBOSE
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 3000) {
      Serial.printf("[TOUCH] Head: %d/%d, Side: %d/%d (threshold: %d)\n", 
                    touchHead1, touchHead2, touchSide1, touchSide2, ROBOT_TOUCH_THRESHOLD);
      lastDebug = millis();
    }
#endif

    // 3. Use cached distance (updated by update() method)
    d.distance_mm = lastDistance_;
    
    // 4. Set sound level and temperature
    d.soundLevel = 0; // Will be set separately if mic enabled
    d.temperature = rtcTemp;
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
    // 10us trigger pulse to HC-SR04
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    
    // 10ms timeout for short desk distance (~1.7m max)
    unsigned long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 10000);
    
    if (duration > 0 && duration < 12000) {
      uint16_t distance = (duration * 343) / 2000; // distance in mm
      
      // Short desk distance range: 20mm (2cm) to 1000mm (100cm)
      if (distance >= 20 && distance <= 1000) {
        static unsigned long lastLog = 0;
        if (millis() - lastLog > 2000) {
          Serial.printf("[ULTRASONIC] Distance: %d mm (%d cm)\n", distance, distance / 10);
          lastLog = millis();
        }
        return distance;
      }
    }
    
    return 0;
  }
};

#endif
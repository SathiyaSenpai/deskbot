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
    
    Serial.println("[SENSORS] Initialized (HC-SR04 Ultrasonic)");
    Serial.printf("  PIR: %d | LDR: %d\n", PIN_PIR, PIN_LDR);
    Serial.printf("  Ultrasonic: Trig=%d, Echo=%d\n", PIN_ULTRASONIC_TRIG, PIN_ULTRASONIC_ECHO);
    Serial.printf("  Touch: Head=%d, Side=%d, Threshold=%d\n", PIN_TOUCH_HEAD, PIN_TOUCH_SIDE, ROBOT_TOUCH_THRESHOLD);
    
    Serial.println("[ULTRASONIC] Running connection test...");
    testUltrasonicConnection();
  }

  void update(bool sleepMode = false) {
    static unsigned long lastDistanceRead = 0;
    if (millis() - lastDistanceRead > 200) {
      lastDistanceRead = millis();
      lastDistance_ = readDistanceSimple();
    }
  }

  SensorData read(float rtcTemp = 0.0f) {
    SensorData d;
    
    // 1. Read ambient sensors
    d.light = analogRead(PIN_LDR);
    d.motion = digitalRead(PIN_PIR) == HIGH;
    
    // 2. Read touch sensors
    int touchHead1 = touchRead(PIN_TOUCH_HEAD);
    int touchSide1 = touchRead(PIN_TOUCH_SIDE);
    delayMicroseconds(500);
    int touchHead2 = touchRead(PIN_TOUCH_HEAD);
    int touchSide2 = touchRead(PIN_TOUCH_SIDE);
    
    d.touchHead = (touchHead1 < ROBOT_TOUCH_THRESHOLD) && (touchHead2 < ROBOT_TOUCH_THRESHOLD);
    d.touchSide = (touchSide1 < ROBOT_TOUCH_THRESHOLD) && (touchSide2 < ROBOT_TOUCH_THRESHOLD);

    // 3. Distance reading
    d.distance_mm = lastDistance_;
    
    // 4. Sound & Temperature
    d.soundLevel = 0;
    d.temperature = rtcTemp;
    return d;
  }

private:
  uint16_t lastDistance_ = 0;

  void testUltrasonicConnection() {
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
      delayMicroseconds(4);
      digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
      delayMicroseconds(10);
      digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
      
      unsigned long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 20000);
      Serial.printf("[ULTRASONIC] Test %d: duration=%lu us", i+1, duration);
      
      if (duration > 0) {
        uint16_t distance = (duration * 343) / 2000;
        Serial.printf(" -> %d mm (%d cm)\n", distance, distance / 10);
      } else {
        Serial.println(" -> NO ECHO");
      }
      delay(100);
    }
  }

  uint16_t readDistanceSimple() {
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    delayMicroseconds(4);
    digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    
    unsigned long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 25000);
    
    if (duration > 0 && duration < 30000) {
      uint16_t distance = (duration * 343) / 2000;
      if (distance >= 20 && distance <= 2000) {
        return distance;
      }
    }
    
    return 0;
  }
};

#endif
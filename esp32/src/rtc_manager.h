#pragma once

#include <Arduino.h>

// ============================================================================
// RTC Manager - Stopwatch and Alarm functionality
// ============================================================================

class RTCManager {
private:
  // Stopwatch state
  bool stopwatchRunning_ = false;
  unsigned long stopwatchStartTime_ = 0;
  unsigned long stopwatchElapsed_ = 0; // Accumulated time when paused
  
  // Alarm state
  bool alarmSet_ = false;
  bool alarmTriggered_ = false;
  int alarmHour_ = -1;
  int alarmMinute_ = -1;
  unsigned long lastTimeCheck_ = 0;
  
public:
  void begin() {
    Serial.println(F("[RTC] Manager initialized"));
  }
  
  // ========== STOPWATCH ==========
  void stopwatchStart() {
    if (!stopwatchRunning_) {
      stopwatchStartTime_ = millis();
      stopwatchRunning_ = true;
      Serial.println(F("[RTC] Stopwatch started"));
    }
  }
  
  void stopwatchStop() {
    if (stopwatchRunning_) {
      stopwatchElapsed_ += millis() - stopwatchStartTime_;
      stopwatchRunning_ = false;
      Serial.println(F("[RTC] Stopwatch stopped"));
    }
  }
  
  void stopwatchReset() {
    stopwatchRunning_ = false;
    stopwatchElapsed_ = 0;
    Serial.println(F("[RTC] Stopwatch reset"));
  }
  
  bool isStopwatchRunning() const {
    return stopwatchRunning_;
  }
  
  void getStopwatchTime(int& minutes, int& seconds, int& centiseconds) {
    unsigned long totalTime = stopwatchElapsed_;
    if (stopwatchRunning_) {
      totalTime += millis() - stopwatchStartTime_;
    }
    
    centiseconds = (totalTime / 10) % 100;
    seconds = (totalTime / 1000) % 60;
    minutes = (totalTime / 60000) % 60;
  }
  
  // ========== ALARM ==========
  void setAlarm(int hour, int minute) {
    if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
      alarmHour_ = hour;
      alarmMinute_ = minute;
      alarmSet_ = true;
      alarmTriggered_ = false;
      Serial.printf("[RTC] Alarm set for %02d:%02d\n", hour, minute);
    } else {
      Serial.println(F("[RTC] Invalid alarm time"));
    }
  }
  
  void dismissAlarm() {
    alarmTriggered_ = false;
    alarmSet_ = false;
    Serial.println(F("[RTC] Alarm dismissed"));
  }
  
  void checkAlarm() {
    // Simple time simulation - in real project would use RTC chip
    // For demo, trigger alarm 10 seconds after setting
    static unsigned long alarmSetTime = 0;
    
    if (alarmSet_ && !alarmTriggered_) {
      if (alarmSetTime == 0) {
        alarmSetTime = millis();
      } else if (millis() - alarmSetTime > 10000) { // 10 seconds for demo
        alarmTriggered_ = true;
        Serial.println(F("[RTC] ALARM TRIGGERED!"));
      }
    } else if (!alarmSet_) {
      alarmSetTime = 0;
    }
  }
  
  bool isAlarmTriggered() const {
    return alarmTriggered_;
  }
  
  void showCurrentTime() {
    // For demo, show uptime as time
    unsigned long uptime = millis() / 1000;
    int hours = (uptime / 3600) % 24;
    int minutes = (uptime / 60) % 60;
    Serial.printf("[RTC] Current time: %02d:%02d (uptime)\n", hours, minutes);
  }
  
  // Get current time components for display
  void getCurrentTime(int& hours, int& minutes, int& seconds) {
    unsigned long uptime = millis() / 1000;
    hours = (uptime / 3600) % 24;
    minutes = (uptime / 60) % 60;
    seconds = uptime % 60;
  }
};
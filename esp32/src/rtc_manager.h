#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

// ============================================================================
// RTC Manager - DS3231 Real Time Clock, Stopwatch, Alarm & Temperature
// ============================================================================

class RTCManager {
private:
  RTC_DS3231 rtc_;
  bool rtcFound_ = false;
  float lastTemperature_ = 0.0f;
  unsigned long lastTempReadTime_ = 0;

  // Stopwatch state
  bool stopwatchRunning_ = false;
  unsigned long stopwatchStartTime_ = 0;
  unsigned long stopwatchElapsed_ = 0; // Accumulated time when paused
  
  // Alarm state
  bool alarmSet_ = false;
  bool alarmTriggered_ = false;
  int alarmHour_ = -1;
  int alarmMinute_ = -1;
  int lastAlarmTriggerMinute_ = -1; // Prevents re-triggering in the same minute
  
public:
  void begin(int sda = -1, int scl = -1) {
    Serial.println(F("[RTC] Initializing DS3231 hardware module..."));
    
    // Initialize Wire once with custom pins BEFORE RTClib's Adafruit_I2CDevice does it
    // RTClib internally calls Wire.begin() again which corrupts the ESP32 I2C mutex
    // if Wire was already initialized. We pass pins here so Wire.begin() is only ever
    // called once with the correct pins.
    if (sda >= 0 && scl >= 0) {
      Wire.begin(sda, scl);
    }
    
    // Attempt to initialize DS3231 — RTClib takes ownership of Wire from here
    if (!rtc_.begin()) {
      Serial.println(F("[RTC] ERROR: Couldn't find DS3231 RTC module on I2C!"));
      rtcFound_ = false;
    } else {
      rtcFound_ = true;
      Serial.println(F("[RTC] DS3231 RTC module connected successfully!"));
      
      if (rtc_.lostPower()) {
        Serial.println(F("[RTC] RTC lost power, setting time to compile timestamp..."));
        rtc_.adjust(DateTime(F(__DATE__), F(__TIME__)));
      }
      
      DateTime now = rtc_.now();
      Serial.printf("[RTC] Current DS3231 Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                    now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
      
      // Read initial temperature
      lastTemperature_ = rtc_.getTemperature();
      Serial.printf("[RTC] DS3231 Internal Temperature: %.2f °C\n", lastTemperature_);
    }
  }
  
  bool isConnected() const { return rtcFound_; }

  float getTemperature() {
    if (!rtcFound_) return 0.0f;
    // Read temperature at most once every 5 seconds to avoid I2C bus congestion
    if (millis() - lastTempReadTime_ > 5000 || lastTempReadTime_ == 0) {
      lastTempReadTime_ = millis();
      lastTemperature_ = rtc_.getTemperature();
    }
    return lastTemperature_;
  }

  float getCachedTemperature() const {
    return lastTemperature_;
  }

  void syncTime(int year, int month, int day, int hour, int minute, int second) {
    if (!rtcFound_) {
      Serial.println(F("[RTC] Cannot sync time - DS3231 not found"));
      return;
    }
    rtc_.adjust(DateTime(year, month, day, hour, minute, second));
    Serial.printf("[RTC] Synced DS3231 Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  year, month, day, hour, minute, second);
  }
  
  void getFormattedTime(char* buffer, size_t len) {
    if (!rtcFound_ || !buffer || len < 9) {
      if (buffer && len > 0) buffer[0] = '\0';
      return;
    }
    DateTime now = rtc_.now();
    snprintf(buffer, len, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
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
      lastAlarmTriggerMinute_ = -1;
      Serial.printf("[RTC] DS3231 Alarm set for %02d:%02d\n", hour, minute);
    } else {
      Serial.println(F("[RTC] Invalid alarm time"));
    }
  }
  
  void dismissAlarm() {
    alarmTriggered_ = false;
    alarmSet_ = false;
    lastAlarmTriggerMinute_ = -1;
    Serial.println(F("[RTC] Alarm dismissed"));
  }
  
  void checkAlarm() {
    if (!alarmSet_ || alarmTriggered_) return;
    
    if (rtcFound_) {
      DateTime now = rtc_.now();
      if (now.hour() == alarmHour_ && now.minute() == alarmMinute_) {
        // Prevent re-triggering multiple times during the same minute
        if (lastAlarmTriggerMinute_ != now.minute()) {
          lastAlarmTriggerMinute_ = now.minute();
          alarmTriggered_ = true;
          Serial.printf("[RTC] DS3231 HARDWARE ALARM TRIGGERED! Time is %02d:%02d\n", now.hour(), now.minute());
        }
      }
    } else {
      // Fallback if DS3231 disconnected: check uptime simulation
      static unsigned long alarmSetTime = 0;
      if (alarmSetTime == 0) alarmSetTime = millis();
      else if (millis() - alarmSetTime > 30000) { // Trigger 30s after setting if no hardware RTC
        alarmTriggered_ = true;
        Serial.println(F("[RTC] ALARM TRIGGERED (Fallback simulation)!"));
      }
    }
  }
  
  bool isAlarmTriggered() const {
    return alarmTriggered_;
  }
  
  void showCurrentTime() {
    if (rtcFound_) {
      DateTime now = rtc_.now();
      Serial.printf("[RTC] Current DS3231 Time: %02d:%02d:%02d | Temp: %.2f °C\n",
                    now.hour(), now.minute(), now.second(), getTemperature());
    } else {
      unsigned long uptime = millis() / 1000;
      int hours = (uptime / 3600) % 24;
      int minutes = (uptime / 60) % 60;
      Serial.printf("[RTC] Current time: %02d:%02d (uptime fallback)\n", hours, minutes);
    }
  }
};
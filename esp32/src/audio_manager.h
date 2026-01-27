#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "Audio.h" // Requires 'esphome/ESP32-audioI2S' library
#include "pins.h"
#include "config.h"

// Define I2S Pins (Must match your wiring)
#define I2S_DOUT      27
#define I2S_BCLK      26
#define I2S_LRC       25

class AudioManager {
public:
  Audio audio; // The library object

  void begin() {
    // 1. Setup I2S Pins
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    
    // 2. Set Volume (Range 0-21 for this library)
    audio.setVolume(18); 
  }

  // ⚡ CRITICAL: This is the function main.cpp is looking for!
  void loop() {
    audio.loop();
  }

  void playFromURL(const char* url) {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("🎵 Streaming: "); Serial.println(url);
        audio.connecttohost(url);
    } else {
        Serial.println("❌ No WiFi for Audio");
    }
  }
  
  // Asynchronous wrapper (Same as playFromURL)
  void playFromURLAsync(const char* url) {
      playFromURL(url);
  }

  bool isRunning() {
      return audio.isRunning();
  }
};

// Global library callback (Required for debugging)
void audio_info(const char *info) {
    Serial.print("audio_info: "); Serial.println(info);
}

void audio_eof_stream(const char *info) {
    Serial.print("audio_eof: "); Serial.println(info);
}

#endif
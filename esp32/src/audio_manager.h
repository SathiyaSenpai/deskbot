#pragma once

#include <Arduino.h>
#include "Audio.h"
#include "config.h"
#include "pins.h"

class AudioManager {
private:
    Audio audio;
    bool isInitialized;
    unsigned long playbackStartTime;
    static const unsigned long MAX_PLAYBACK_TIME = 60000; // 60s max playback safety timeout

public:
    AudioManager() : isInitialized(false), playbackStartTime(0) {}

    void begin() {
        if (isInitialized) return;
        
        Serial.println(F("[AUDIO] Initializing ESP32-audioI2S library..."));
        Serial.printf("[AUDIO] Pins: BCLK=%d, LRC=%d, DOUT=%d\n", PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
        
        audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
        audio.setVolume(AUDIO_VOLUME); // Range 0..21
        
        isInitialized = true;
        Serial.println(F("[AUDIO] ESP32-audioI2S initialized successfully"));
    }

    void playURL(const String& url) {
        if (!isInitialized) begin();
        
        stop(); // Stop any current playback cleanly
        
        Serial.print(F("[AUDIO] Playing URL: "));
        Serial.println(url);
        
        if (url.length() > 0) {
            audio.connecttohost(url.c_str());
            playbackStartTime = millis();
        }
    }

    void update() {
        if (!isInitialized) return;
        
        // Loop audio task processing
        audio.loop();
        
        // Safety timeout to prevent infinite stuck stream
        if (audio.isRunning() && (millis() - playbackStartTime > MAX_PLAYBACK_TIME)) {
            Serial.println(F("[AUDIO] Playback timeout - stopping"));
            stop();
        }
    }

    void stop() {
        if (isInitialized && audio.isRunning()) {
            audio.stopSong();
            Serial.println(F("[AUDIO] Playback stopped"));
        }
    }

    bool getIsPlaying() { 
        return isInitialized && audio.isRunning(); 
    }

    void testAudio() {
        if (!isInitialized) begin();
        Serial.println(F("[AUDIO] Audio system online"));
    }

    void speakText(const String& text) {
        Serial.print(F("[AUDIO] Speaking text: "));
        Serial.println(text);
        Serial.println(F("[AUDIO] Use playURL with TTS audio file from server"));
    }
};
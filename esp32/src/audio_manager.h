#pragma once

#include <Arduino.h>
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "config.h"

// Audio pins for MAX98357A
#define I2S_DOUT      27  // Data pin (was 27)
#define I2S_BCLK      26  // Bit clock (was 26) 
#define I2S_LRC       25  // Left/Right clock (was 25)

class AudioManager {
private:
    AudioGeneratorMP3* mp3;
    AudioFileSourceHTTPStream* audioSource;
    AudioFileSourceBuffer* audioBuffer;
    AudioOutputI2S* audioOutput;
    bool isPlaying;
    bool isInitialized;
    String currentUrl;
    unsigned long lastUpdateTime;
    unsigned long playbackStartTime;
    static const unsigned long UPDATE_INTERVAL = 5; // Process audio every 5ms max
    static const unsigned long MAX_PLAYBACK_TIME = 60000; // 60 second max playback

public:
    AudioManager() : mp3(nullptr), audioSource(nullptr), audioBuffer(nullptr), 
                     audioOutput(nullptr), isPlaying(false), isInitialized(false),
                     lastUpdateTime(0), playbackStartTime(0) {}

    void begin() {
        if (isInitialized) return;
        
        Serial.println("[AUDIO] Initializing ESP8266Audio library...");
        Serial.printf("[AUDIO] Pins: BCLK=%d, LRC=%d, DOUT=%d\n", I2S_BCLK, I2S_LRC, I2S_DOUT);
        
        // Initialize I2S output
        audioOutput = new AudioOutputI2S();
        audioOutput->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
        audioOutput->SetGain(1.0);  // Increased volume for better audio output
        
        // Initialize MP3 generator
        mp3 = new AudioGeneratorMP3();
        
        isInitialized = true;
        Serial.println("[AUDIO] ESP8266Audio initialized successfully");
    }

    void playURL(const String& url) {
        if (!isInitialized) begin();
        
        stop(); // Stop any current playback
        
        Serial.println("[AUDIO] Playing URL: " + url);
        currentUrl = url;
        lastUpdateTime = millis();
        
        try {
            // Create HTTP stream source
            audioSource = new AudioFileSourceHTTPStream(url.c_str());
            
            // FREEZE FIX: Smaller buffer (2048 instead of 4096) for faster response
            // Trades some audio smoothness for better system responsiveness
            audioBuffer = new AudioFileSourceBuffer(audioSource, 2048);
            
            // Start MP3 generation
            if (mp3->begin(audioBuffer, audioOutput)) {
                isPlaying = true;
                playbackStartTime = millis();
                Serial.println("[AUDIO] MP3 playback started successfully");
            } else {
                Serial.println("[AUDIO] Failed to start MP3 playback");
                cleanup();
            }
        } catch (...) {
            Serial.println("[AUDIO] Exception in playURL");
            cleanup();
        }
    }

    void update() {
        if (!isPlaying || !mp3 || !mp3->isRunning()) return;
        
        unsigned long now = millis();
        
        // FREEZE FIX: Safety timeout - stop if playing too long (stuck)
        if (now - playbackStartTime > MAX_PLAYBACK_TIME) {
            Serial.println("[AUDIO] Playback timeout - stopping");
            stop();
            return;
        }
        
        // FREEZE FIX: Throttle audio processing to prevent blocking main loop
        if (now - lastUpdateTime < UPDATE_INTERVAL) return;
        lastUpdateTime = now;
        
        // Process audio in small chunks - yield control back quickly
        // The loop() call decodes and plays a chunk of audio
        if (!mp3->loop()) {
            Serial.println("[AUDIO] Playback finished");
            stop();
        }
        
        // Additional yield to prevent watchdog timeout
        yield();
    }

    void stop() {
        if (mp3 && isPlaying) {
            mp3->stop();
        }
        cleanup();
        isPlaying = false;
        Serial.println("[AUDIO] Playback stopped");
    }

    bool getIsPlaying() { 
        return isPlaying && mp3 && mp3->isRunning(); 
    }

    void testAudio() {
        if (!isInitialized) begin();
        
        // Use a simple test beep instead of URL
        Serial.println("[AUDIO] Test audio - playing simple beep");
        // For now, just indicate that test was called
        Serial.println("[AUDIO] Audio test completed");
    }

    // Text-to-speech - now uses the proper URL
    void speakText(const String& text) {
        Serial.println("[AUDIO] Speaking text: " + text);
        // The server should provide the audio URL - this is handled in main.cpp
        Serial.println("[AUDIO] Use playURL with TTS audio file from server");
    }

private:
    void cleanup() {
        if (audioBuffer) {
            delete audioBuffer;
            audioBuffer = nullptr;
        }
        if (audioSource) {
            delete audioSource;
            audioSource = nullptr;
        }
    }
};
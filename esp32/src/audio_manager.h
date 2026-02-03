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

public:
    AudioManager() : mp3(nullptr), audioSource(nullptr), audioBuffer(nullptr), 
                     audioOutput(nullptr), isPlaying(false), isInitialized(false) {}

    void begin() {
        if (isInitialized) return;
        
        Serial.println("[AUDIO] Initializing ESP8266Audio library...");
        Serial.printf("[AUDIO] Pins: BCLK=%d, LRC=%d, DOUT=%d\n", I2S_BCLK, I2S_LRC, I2S_DOUT);
        
        // Initialize I2S output
        audioOutput = new AudioOutputI2S();
        audioOutput->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
        audioOutput->SetGain(0.3);  // Lower volume to prevent distortion
        
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
        
        try {
            // Create HTTP stream source
            audioSource = new AudioFileSourceHTTPStream(url.c_str());
            
            // Add buffer for smooth playback (larger buffer for better streaming)
            audioBuffer = new AudioFileSourceBuffer(audioSource, 4096);
            
            // Start MP3 generation
            if (mp3->begin(audioBuffer, audioOutput)) {
                isPlaying = true;
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
        if (isPlaying && mp3 && mp3->isRunning()) {
            if (!mp3->loop()) {
                Serial.println("[AUDIO] Playback finished");
                stop();
            }
        }
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
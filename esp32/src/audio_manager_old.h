#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <math.h>

// I2S Pins for MAX98357A
#define I2S_BCLK      26
#define I2S_LRC       25
#define I2S_DOUT      27

// Audio settings
#define AUDIO_SAMPLE_RATE   44100
#define I2S_PORT      I2S_NUM_0
#define BUFFER_SIZE   256

class AudioManager {
private:
  bool initialized = false;
  int16_t buffer[BUFFER_SIZE];
  bool isPlaying = false;

public:
  void begin() {
    Serial.println("[AUDIO] Initializing Direct I2S Audio...");
    Serial.printf("[AUDIO] Pins: BCLK=%d, LRC=%d, DOUT=%d\n", I2S_BCLK, I2S_LRC, I2S_DOUT);
    
    // Configure I2S
    i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = AUDIO_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCLK,
      .ws_io_num = I2S_LRC,
      .data_out_num = I2S_DOUT,
      .data_in_num = I2S_PIN_NO_CHANGE
    };

    // Install and configure I2S
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
      Serial.printf("[AUDIO] ERROR: I2S install failed: %d\n", err);
      return;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
      Serial.printf("[AUDIO] ERROR: I2S pin config failed: %d\n", err);
      return;
    }

    i2s_zero_dma_buffer(I2S_PORT);
    initialized = true;
    Serial.println("[AUDIO] ✓ Direct I2S ready!");
  }

  void loop() {
    // Not needed for direct I2S
  }

  // Play a single tone
  void playTone(int frequency, int duration_ms, int amplitude = 12000) {
    if (!initialized) return;
    
    double phase = 0.0;
    double phase_increment = 2.0 * PI * frequency / SAMPLE_RATE;
    unsigned long start_time = millis();
    size_t bytes_written;
    
    while (millis() - start_time < duration_ms) {
      for (int i = 0; i < BUFFER_SIZE; i += 2) {
        int16_t sample = (int16_t)(amplitude * sin(phase));
        buffer[i] = sample;      // Left
        buffer[i + 1] = sample;  // Right
        phase += phase_increment;
        if (phase >= 2.0 * PI) phase -= 2.0 * PI;
      }
      i2s_write(I2S_PORT, buffer, sizeof(buffer), &bytes_written, portMAX_DELAY);
    }
    i2s_zero_dma_buffer(I2S_PORT);
  }

  // Simple Text-to-Speech using morse-like patterns
  void speakText(const char* text) {
    if (!initialized) return;
    
    Serial.printf("[AUDIO] 🗣️ Speaking: %s\n", text);
    
    int len = strlen(text);
    for (int i = 0; i < len && i < 50; i++) {  // Limit to 50 chars
      char c = tolower(text[i]);
      
      if (c == ' ') {
        delay(200);  // Word break
      } else if (c >= 'a' && c <= 'z') {
        // Each letter gets a unique frequency
        int freq = 300 + (c - 'a') * 30;  // 300-1050Hz range
        playTone(freq, 150, 8000);
        delay(50);
      } else if (c >= '0' && c <= '9') {
        // Numbers get lower frequencies  
        int freq = 200 + (c - '0') * 20;  // 200-380Hz range
        playTone(freq, 200, 8000);
        delay(50);
      } else if (c == '!' || c == '?') {
        // Punctuation gets special tones
        playTone(800, 100, 6000);
        playTone(600, 100, 6000);
        delay(100);
      } else {
        delay(100);  // Other characters
      }
    }
    
    // End with confirmation beep
    playTone(880, 100, 8000);
    delay(50);
    playTone(1100, 100, 8000);
  }

  // For compatibility with existing code
  void playFromURL(const char* url) {
    // Extract text from the URL if it contains TTS
    if (strstr(url, "tts_")) {
      speakText("Message received");
    } else {
      playNotification();
    }
  }
  
  void playFromURLAsync(const char* url) {
    playFromURL(url);
  }

  void stop() {
    if (initialized) {
      i2s_zero_dma_buffer(I2S_PORT);
    }
    isPlaying = false;
  }

  bool isRunning() {
    return isPlaying;
  }
  
  bool isInitialized() {
    return initialized;
  }

  void playTestTone() {
    Serial.println("[AUDIO] Playing test speech...");
    speakText("Hello world! Audio test successful!");
  }

  void playNotification() {
    playTone(880, 100, 8000);
    delay(100);
    playTone(880, 100, 8000);
  }
};

#endif
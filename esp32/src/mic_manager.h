#ifndef MIC_MANAGER_H
#define MIC_MANAGER_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "pins.h"
#include "config.h"

#define I2S_MIC_PORT I2S_NUM_1
#define MIC_SAMPLE_RATE 16000
#define MIC_BUFFER_LEN 64 // Samples per buffer

class MicManager {
private:
  bool initialized = false;
  bool isRecordingState = false;
  int16_t sampleBuffer[MIC_BUFFER_LEN]; // Class member instead of stack allocation

public:
  void begin() {
    #if !ENABLE_MICROPHONE
      Serial.println(F("[MIC] Disabled in config"));
      return;
    #endif
    initialized = false;
    isRecordingState = false;
    startRecording(); // Install driver ONCE at boot to prevent heap fragmentation!
    Serial.println(F("[MIC] Ready and initialized at boot"));
  }

  bool startRecording() {
    if (initialized) {
      if (!isRecordingState) {
        i2s_start(I2S_MIC_PORT);
        isRecordingState = true;
      }
      return true;
    }

    i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = MIC_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // 16-bit saves 50% DMA memory vs 32-bit
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 3,
      .dma_buf_len = MIC_BUFFER_LEN,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
      .bck_io_num = PIN_I2S_MIC_SCK,
      .ws_io_num = PIN_I2S_MIC_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = PIN_I2S_MIC_SD
    };

    esp_err_t err = i2s_driver_install(I2S_MIC_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
      Serial.printf("[MIC] Driver install failed: %d\n", err);
      return false;
    }

    err = i2s_set_pin(I2S_MIC_PORT, &pin_config);
    if (err != ESP_OK) {
      Serial.printf("[MIC] Pin config failed: %d\n", err);
      i2s_driver_uninstall(I2S_MIC_PORT);
      return false;
    }

    initialized = true;
    isRecordingState = true;
    Serial.println(F("[MIC] Installed & recording started"));
    return true;
  }

  void stopRecording() {
    if (initialized && isRecordingState) {
      i2s_stop(I2S_MIC_PORT);
      isRecordingState = false;
      // Do NOT uninstall driver! Keeps DMA memory stable and prevents heap fragmentation crashes.
    }
  }

  int getLoudness() {
    if (!initialized || !isRecordingState) {
      if (!startRecording()) return 0;
    }

    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_MIC_PORT, sampleBuffer, sizeof(sampleBuffer), &bytes_read, 10);
    if (err != ESP_OK || bytes_read == 0) return 0;

    uint64_t sum = 0;
    int sample_count = bytes_read / sizeof(int16_t);
    if (sample_count == 0) return 0;
    
    for (int i = 0; i < sample_count; i++) {
      int32_t sample = (int32_t)sampleBuffer[i];
      sum += (uint64_t)(sample * sample);
    }
    
    float rms = sqrtf((float)sum / sample_count);
    int vol = (int)(rms / 50.0f); 
    return (vol > 100) ? 100 : vol;
  }

  size_t readChunk(uint8_t* buffer, size_t maxBytes) {
    if (!initialized) return 0;
    size_t bytes_read = 0;
    i2s_read(I2S_MIC_PORT, buffer, maxBytes, &bytes_read, 10);
    return bytes_read;
  }

  bool isReady() { return initialized; }
  bool isRecording() { return isRecordingState; }
};

#endif
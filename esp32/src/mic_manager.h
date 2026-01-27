#ifndef MIC_MANAGER_H
#define MIC_MANAGER_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "pins.h"
#include "config.h"

#define I2S_MIC_PORT I2S_NUM_1
#define SAMPLE_RATE 16000
#define BUFFER_LEN 32  // Reduced for memory efficiency

class MicManager {
private:
  bool initialized = false;  // Start as false, set to true only after successful init

public:
  void begin() {
    initialized = false;  // Reset on begin
    #if !ENABLE_MICROPHONE
      Serial.println("[MIC] Disabled in config");
      return;
    #endif

    i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 2,  // Reduced from 4
      .dma_buf_len = BUFFER_LEN,
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
      return;
    }

    err = i2s_set_pin(I2S_MIC_PORT, &pin_config);
    if (err != ESP_OK) {
      Serial.printf("[MIC] Pin config failed: %d\n", err);
      i2s_driver_uninstall(I2S_MIC_PORT);
      return;
    }

    initialized = true;
    Serial.println("[MIC] Initialized successfully");
  }

  int getLoudness() {
    if (!initialized) return 0;

    int32_t samples[BUFFER_LEN];
    size_t bytes_read = 0;
    
    esp_err_t err = i2s_read(I2S_MIC_PORT, samples, sizeof(samples), &bytes_read, 10);
    if (err != ESP_OK || bytes_read == 0) return 0;

    uint64_t sum = 0;
    int sample_count = bytes_read / sizeof(int32_t);
    
    for (int i = 0; i < sample_count; i++) {
      int32_t sample = samples[i] >> 14;  // Normalize 24-bit to 16-bit
      sum += (uint64_t)(sample * sample);
    }
    
    float rms = sqrt((double)sum / sample_count);
    int vol = (int)(rms / 50.0); 
    return (vol > 100) ? 100 : vol;
  }

  bool isReady() { return initialized; }
};

#endif
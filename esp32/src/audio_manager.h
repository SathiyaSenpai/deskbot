#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "pins.h"
#include "config.h"

class AudioManager {
public:
  bool begin() {
    return initOutput();
  }

  // Simple I2S tone playback (for quick feedback)
  void playTone(uint16_t frequency, uint16_t durationMs) {
    const int sampleRate = 16000;
    const int samples = (sampleRate * durationMs) / 1000;
    int16_t buffer[256];
    const int16_t amp = (int16_t)(32700 * volumeScale());

    for (int i = 0; i < samples; i += 256) {
      int chunkSize = min(256, samples - i);
      for (int j = 0; j < chunkSize; j++) {
        float t = float(i + j) / sampleRate;
        buffer[j] = int16_t(sinf(2.0f * PI * frequency * t) * amp);
      }
      size_t written;
      i2s_write(I2S_PORT_OUT, buffer, chunkSize * sizeof(int16_t), &written, portMAX_DELAY);
    }
  }

  void playChirp() {
    playTone(800, 60);
    delay(30);
    playTone(1200, 60);
    delay(30);
    playTone(1600, 80);
  }
  
  void playHappy() {
    playTone(523, 100);  // C5
    delay(50);
    playTone(659, 100);  // E5
    delay(50);
    playTone(784, 150);  // G5
  }
  
  void playSad() {
    playTone(392, 200);  // G4
    delay(100);
    playTone(330, 300);  // E4
  }
  
  void playAlert() {
    for (int i = 0; i < 3; i++) {
      playTone(1000, 100);
      delay(100);
    }
  }
  
  void playStartup() {
    playTone(262, 100);  // C4
    delay(50);
    playTone(330, 100);  // E4
    delay(50);
    playTone(392, 100);  // G4
    delay(50);
    playTone(523, 200);  // C5
  }

  // Fire-and-forget streaming; spawns a task so the main loop keeps running
  bool playFromURLAsync(const char* url) {
    if (playing_) {
      Serial.println("[Audio] Already playing, ignoring new request");
      return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[Audio] WiFi not connected, can't stream");
      return false;
    }

    playing_ = true;
    cancelRequested_ = false;

    auto* args = new StreamTaskArgs{this, String(url)};
    BaseType_t ok = xTaskCreatePinnedToCore(
      &AudioManager::streamTask,
      "audio_stream",
      4096,
      args,
      1,
      nullptr,
      1  // run on APP CPU to leave PRO CPU free
    );

    if (ok != pdPASS) {
      Serial.println("[Audio] Failed to create stream task");
      playing_ = false;
      delete args;
      return false;
    }

    return true;
  }
  
  /**
   * Set volume (0-100)
   * Note: MAX98357A has fixed gain, so we attenuate in software
   */
  void setVolume(uint8_t vol) {
    volume_ = constrain(vol, 0, 100);
  }
  
  uint8_t getVolume() {
    return volume_;
  }

  void stopPlayback() {
    if (!playing_) return;
    cancelRequested_ = true;
  }

private:
  static constexpr i2s_port_t I2S_PORT_OUT = I2S_NUM_0;
  static constexpr uint32_t STREAM_TIMEOUT_MS = 20000;
  uint8_t volume_ = AUDIO_VOLUME;
  bool playing_ = false;
  volatile bool cancelRequested_ = false;

  struct StreamTaskArgs {
    AudioManager* self;
    String url;
  };

  static void streamTask(void* param) {
    auto* args = static_cast<StreamTaskArgs*>(param);
    args->self->playFromURLBlocking(args->url.c_str());
    args->self->playing_ = false;
    args->self->cancelRequested_ = false;
    delete args;
    vTaskDelete(nullptr);
  }

  bool initOutput() {
    i2s_config_t config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 512,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0
    };

    i2s_pin_config_t pins = {
      .bck_io_num = PIN_I2S_BCLK,
      .ws_io_num = PIN_I2S_LRC,
      .data_out_num = PIN_I2S_DOUT,
      .data_in_num = I2S_PIN_NO_CHANGE
    };

    if (i2s_driver_install(I2S_PORT_OUT, &config, 0, nullptr) != ESP_OK) {
      Serial.println("I2S output driver install failed");
      return false;
    }
    if (i2s_set_pin(I2S_PORT_OUT, &pins) != ESP_OK) {
      Serial.println("I2S output pin config failed");
      return false;
    }
    
    Serial.println("[Audio] I2S initialized");
    return true;
  }

  /**
   * Blocking WAV stream (used inside task)
   */
  bool playFromURLBlocking(const char* url) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
      Serial.printf("[Audio] HTTP error: %d\n", httpCode);
      http.end();
      return false;
    }
    
    WiFiClient* stream = http.getStreamPtr();
    int contentLength = http.getSize();
    Serial.printf("[Audio] Streaming: %s (%d bytes)\n", url, contentLength);
    
    uint8_t header[44];
    stream->readBytes(header, 44);
    
    uint8_t buffer[512];
    int16_t samples[256];
    
    uint32_t startMs = millis();
    while (stream->available() > 0 || http.connected()) {
      if (cancelRequested_) {
        Serial.println("[Audio] Playback cancelled");
        break;
      }
      if (millis() - startMs > STREAM_TIMEOUT_MS) {
        Serial.println("[Audio] Playback timeout");
        break;
      }
      int toRead = stream->available();
      toRead = toRead > 0 ? min(toRead, 512) : 512;
      int bytesRead = stream->readBytes(buffer, toRead);
      if (bytesRead == 0) {
        vTaskDelay(1);
        continue;
      }
      const float gain = volumeScale();
      int sampleCount = bytesRead / 2;
      const int16_t* src = reinterpret_cast<int16_t*>(buffer);
      for (int i = 0; i < sampleCount; ++i) {
        int32_t s = (int32_t)(src[i] * gain);
        samples[i] = (int16_t)constrain(s, -32768, 32767);
      }
      size_t written;
      i2s_write(I2S_PORT_OUT, samples, sampleCount * sizeof(int16_t), &written, portMAX_DELAY);
      vTaskDelay(1); // yield to keep other tasks alive
    }
    
    http.end();
    Serial.println("[Audio] Playback complete");
    return true;
  }

  float volumeScale() const {
    return constrain(volume_, (uint8_t)0, (uint8_t)100) / 100.0f;
  }
};

#endif // AUDIO_MANAGER_H

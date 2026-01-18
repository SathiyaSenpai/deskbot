#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "pins.h"

struct SensorData {
  uint16_t light = 0;
  bool motion = false;
  uint16_t distance_mm = 0;
  bool touchHead = false;
  bool touchSide = false;
  uint16_t soundLevel = 0; // crude amplitude from mic (if enabled)
};

class SensorManager {
public:
  void begin() {
    pinMode(PIN_PIR, INPUT);
    pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_ECHO, INPUT);
    initMic();
  }

  SensorData read() {
    SensorData d;
    d.light = analogRead(PIN_LDR);
    d.motion = digitalRead(PIN_PIR) == HIGH;
    d.distance_mm = measureDistance();
    d.touchHead = touchRead(PIN_TOUCH_HEAD) < TOUCH_THRESHOLD;
    d.touchSide = touchRead(PIN_TOUCH_SIDE) < TOUCH_THRESHOLD;
    d.soundLevel = sampleMicLevel();
    return d;
  }

private:
  static constexpr i2s_port_t MIC_PORT = I2S_NUM_1;
  bool micReady_ = false;

  uint16_t measureDistance() {
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);

    long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 20000); // 20ms timeout ~3.4m
    if (duration == 0) return 0;
    // distance = duration * speed_of_sound / 2 (343 m/s)
    uint16_t distMm = (uint16_t)((duration * 343) / 2000); // microseconds to mm
    return distMm;
  }

  void initMic() {
    i2s_config_t config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0
    };

    i2s_pin_config_t pins = {
      .bck_io_num = PIN_I2S_MIC_SCK,
      .ws_io_num = PIN_I2S_MIC_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = PIN_I2S_MIC_SD
    };

    if (i2s_driver_install(MIC_PORT, &config, 0, nullptr) != ESP_OK) return;
    if (i2s_set_pin(MIC_PORT, &pins) != ESP_OK) return;
    micReady_ = true;
  }

  uint16_t sampleMicLevel() {
    if (!micReady_) return 0;

    int16_t samples[128];
    size_t bytesRead = 0;
    // Non-blocking read; if no data, return 0
    if (i2s_read(MIC_PORT, samples, sizeof(samples), &bytesRead, 2) != ESP_OK || bytesRead == 0) {
      return 0;
    }

    int count = bytesRead / sizeof(int16_t);
    uint32_t acc = 0;
    for (int i = 0; i < count; ++i) {
      acc += abs(samples[i]);
    }
    uint16_t avg = count > 0 ? acc / count : 0;
    return avg;
  }
};

#endif // SENSORS_H

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Adafruit_NeoPixel.h>
#include "pins.h"

class LedController {
public:
  void begin() {
    strip_ = Adafruit_NeoPixel(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
    strip_.begin();
    strip_.setBrightness(40);
    strip_.show();
  }

  void setColor(uint32_t color) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip_.setPixelColor(i, color);
    }
    strip_.show();
  }

  void setRGB(uint8_t r, uint8_t g, uint8_t b) {
    setColor(strip_.Color(r, g, b));
  }

  void pulse(uint32_t color, uint8_t brightness) {
    strip_.setBrightness(brightness);
    setColor(color);
  }

private:
  Adafruit_NeoPixel strip_;
};

#endif // LED_CONTROLLER_H

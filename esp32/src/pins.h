#ifndef PINS_H
#define PINS_H

// ============================================================================
// ESP32 PIN CONFIGURATION - Desk Robot (Updated with Touch)
// ============================================================================

// I2C Bus (OLED + RTC)
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22

// I2S Audio Output (MAX98357A)
#define PIN_I2S_BCLK        26
#define PIN_I2S_LRC         25
#define PIN_I2S_DOUT        27

// I2S Microphone Input
#define PIN_I2S_MIC_SCK     14
#define PIN_I2S_MIC_WS      12  // Shares touch channel T3, keep mic priority
#define PIN_I2S_MIC_SD      35

// WS2812 RGB LED Ring
#define PIN_NEOPIXEL        13
#define NUM_LEDS            16

// Sensors
#define PIN_PIR             33
#define PIN_LDR             34      // ADC input
#define PIN_ULTRASONIC_TRIG 23
#define PIN_ULTRASONIC_ECHO 5

// Actuators
#define PIN_SERVO           18
#define PIN_BUZZER          19

// Touch sensors (capacitive)
#define PIN_TOUCH_HEAD      4   // T0 - Primary touch (head)
#define PIN_TOUCH_SIDE      15   // T2 - Secondary touch (side)
#define ROBOT_TOUCH_THRESHOLD     40  // Lower = more sensitive

#endif // PINS_H
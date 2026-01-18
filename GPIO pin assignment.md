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
#define PIN_I2S_MIC_WS      15  // ⚠ Conflicts with Touch T3
#define PIN_I2S_MIC_SD      32

// WS2812 RGB LED Ring
#define PIN_NEOPIXEL        13
#define NUM_LEDS            16

// Sensors
#define PIN_PIR             33
#define PIN_LDR             34      // ADC input
#define PIN_ULTRASONIC_TRIG 12
#define PIN_ULTRASONIC_ECHO 35

// Actuators
#define PIN_SERVO           18
#define PIN_BUZZER          19

// ============== NEW: TOUCH SENSORS ==============
// ESP32 capacitive touch pins (no external components!)
#define PIN_TOUCH_HEAD      4   // T0 - Primary touch (head pat)
#define PIN_TOUCH_SIDE      2   // T2 - Secondary touch (side)
// Note: GPIO15 (T3) used for I2S mic, can't use for touch

// Touch threshold (calibrate based on your setup)
#define TOUCH_THRESHOLD     40  // Lower = more sensitive

#endif // PINS_H
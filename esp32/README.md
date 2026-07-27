# Nisya AI Companion (ESP32)

A fully autonomous AI companion robot powered by ESP32 and a Mobile Node.js Brain.

## 🌟 Features
* **Smooth Eye Engine:** Custom OLED animation engine (squircle eyes).
* **Mobile-First Design:** Connects automatically to a Phone Hotspot.
* **Termux Brain:** Offloads AI processing to an Android phone running Node.js.
* **Crash-Proof:** Optimized power management to prevent brownouts on USB power.
* **Offline Mode:** Continues to interact (Touch/Motion) even without WiFi.

## 🛠️ Hardware & Effective Usage
* **ESP32 (Dev Module):** Main controller with WiFi/Hotspot connection, WebSocket FreeRTOS queue, and brownout protection.
* **SH1106 OLED Display (1.3"):** Renders dynamic squircle eyes, blinking animations, emotional expressions, and stopwatch.
* **SG90 Servo x1:** Non-blocking 50Hz servo controller for gestures (shake, tilt, droop, jolt) and idle eye tracking.
* **Two jumperwire directly from esp32 for touch sensing:** Direct capacitive touch sensing on GPIO 4 (Head/T0) and GPIO 15 (Side/T3) with 2-sample noise debouncing.
* **Speaker & I2S Amp:** Audio output via I2S DMA with time-multiplexing for sound effects and TTS streaming.
* **Buzzer:** Non-blocking tone sequences for startup, emotions, sleep, and DS3231 alarm ringing.
* **ldr sensor module:** Ambient light detection for automatic dark sleep mode and UI reporting.
* **DS3231 Real Time Clock Memory Module:** Hardware I2C RTC with automatic server time syncing, room temperature monitoring, and hardware alarm ringing.
* **PIR Motion Detection Sensor:** Presence detection with crowd-proof hold timers and automatic waking.
* **Ultrasonic Sensor:** Distance measurement (Trig/Echo) with timeout filtering and proximity zoning (Near/Far).
* **16Bit RGB LED (WS2812B):** 16-LED NeoPixel ring with 7 animation modes, voice reactivity, and mood colors.
* **Smartelex i2s mems microphone:** 16-bit DMA microphone with volume-based VAD and audio chunk streaming over WebSockets for speech recognition.

## 🚀 How to Run (Termux)
1.  Install Termux on Android.
2.  Run `pkg install nodejs`.
3.  Clone this repo.
4.  Run `node server.js`.
5.  Turn on Hotspot and power up the robot.
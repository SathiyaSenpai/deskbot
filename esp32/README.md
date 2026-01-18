# ESP32 Deskbot Firmware (Offline + WebSocket)

## Quick Flash Steps
1. Open this folder in PlatformIO (VS Code) or Arduino IDE (set board: ESP32 Dev Module).
2. Edit `src/config.h` with your WiFi SSID/PASSWORD and WebSocket host/port (use LAN IP for free/offline).
3. Connect USB power (mobile charger is fine) and flash.
4. Open Serial Monitor @115200 to see WiFi + WebSocket status.

## Hardware Pins
- OLED I2C: SDA 21, SCL 22
- MAX98357A: BCLK 26, LRC 25, DOUT 27
- Mic (I2S): SCK 14, WS 15, SD 32
- WS2812 ring: GPIO 13 (16 LEDs)
- Servo: GPIO 18
- Buzzer: GPIO 19 (optional)
- PIR: GPIO 33
- LDR (ADC): GPIO 34
- Ultrasonic: TRIG 12, ECHO 35
- Touch: Head T0=GPIO4, Side T2=GPIO2

## Behavior Engine
- Behaviors match the web demo: offsets ±48/±24, openness/scale/bottomLid with smoothing.
- Eye render is procedural (no bitmaps) for 128x64 OLED.
- WebSocket message `set_behavior` changes expression remotely.

## Sensors & Actuators
- Sensors push every 0.5s via `sensor_data` (light, motion, distance, touch, sound placeholder).
- LED ring controlled via `set_led {r,g,b}`.
- Servo via `set_servo {angle}`.
- Audio chirp via `play_sound {name}`.

## Power Notes
- No battery needed: power from a phone charger over USB (5V) is fine.
- If using a servo + LEDs at high brightness, ensure a 2A USB supply.

## WebSocket Protocol (free, local)
- Host/port/path in `config.h`; run a simple Node server on your laptop/RPi.
- On connect: device sends `{type:"hello", device, version}`.
- Status heartbeat every 5s: `{type:"robot_status", state, behavior}`.
- Sensor updates every 0.5s: `{type:"sensor_data", ...}`.
- Commands accepted: `set_behavior`, `set_led`, `set_servo`, `play_sound`.

## Offline Behavior
- If WiFi/WebSocket is absent, the eyes still animate locally; sensors still read.

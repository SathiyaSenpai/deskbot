# DeskBot AI Companion (ESP32)

A fully autonomous AI desktop companion robot powered by ESP32 and a Mobile Node.js Brain.

## 🌟 Features
* **Smooth Eye Engine:** Custom OLED animation engine (squircle eyes).
* **Mobile-First Design:** Connects automatically to a Phone Hotspot.
* **Termux Brain:** Offloads AI processing to an Android phone running Node.js.
* **Crash-Proof:** Optimized power management to prevent brownouts on USB power.
* **Offline Mode:** Continues to interact (Touch/Motion) even without WiFi.

## 🛠️ Hardware
* ESP32 (Dev Module)
* SH1106 OLED Display (1.3")
* SG90 Servo
* TTP223 Touch Sensor
* Speaker
* I2S Amp
* Buzzer
* LDR Module
* PIR Motion Detection Sensor
* Ultrasonic Sensor
* 16Bit RGB LED (WS2812B)

## AI Services Module for DeskBot

### Supports:
 * - Edge TTS (FREE, works everywhere - PC & Phone!)
 * - Piper TTS (local, free - PC only)
 * - Ollama LLM (local, free), GROQ API OR Gemini API

### TTS Priority:
 *  Edge TTS (default - FREE, online, great quality)
 *  Piper TTS (if installed locally)
 *  Browser TTS (fallback)

## Configuration

### Before starting the server, you must configure your local network and API credentials.

 *  Open your `src/config file`

 *  Update the Wi-Fi Name and password to match your mobile hotspot.

 *  Also update you inet address to WS_HOST (You can find you inet address by running `ifconfig` in termux)
  
 *  Open `server/public/ai-services.js`

 *  Insert your API key into the required field.

## 🚀 How to Run (Termux)
1.  Install Termux on Android.
2.  Run `pkg install nodejs`
3.  Run `npm -v` to check npm
4. `npm install express`
5.  Clone this repo.
6.  Go to server folder `cd server`
7.  Run `node server.js`
8.  Turn on Hotspot and power up the robot.

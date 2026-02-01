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

## 🚀 How to Run (Termux)
1.  Install Termux on Android.
2.  Run `pkg install nodejs`.
3.  Clone this repo.
4.  Run `node server.js`.
5.  Turn on Hotspot and power up the robot.
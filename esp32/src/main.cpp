#include <Arduino.h>
#include <WiFi.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>
#include "config.h"
#include "pins.h"
#include "behaviors.h"
#include "eye_engine.h"
#include "sensors.h"
#include "led_controller.h"
#include "servo_controller.h"
#include "websocket_client.h"
#include "audio_manager.h"
#include "wifi_manager.h"

// OLED: choose driver (SSD1306 128x64). If you see a vertical offset, switch to SH1106.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);

EyeEngine eye(display);
SensorManager sensors;
LedController leds;
ServoController servo;
RobotWebSocket ws;
AudioManager audioMgr;

// Timing
unsigned long lastFrameMs = 0;
unsigned long lastSensorMs = 0;
unsigned long lastPingMs = 0;
unsigned long lastWsReconnectMs = 0;
unsigned long buttonHoldStart = 0;
const uint32_t FRAME_INTERVAL = 33;     // ~30 fps
const uint32_t SENSOR_INTERVAL = 500;    // sensor push every 0.5s
const uint32_t PING_INTERVAL = 5000;     // status every 5s
const uint32_t RESET_HOLD_TIME = 5000;   // Hold button 5s to reset WiFi

// Current robot state
const char* robotState = "idle";
bool inSetupMode = false;

// Touch debounce helpers
bool headTouchedDebounced();

// Forward declarations
void handleMessage(const char* type, JsonDocument& doc);
void showSetupScreen();
void showConnectingScreen(const char* ssid);
void showConnectedScreen();

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n🤖 EMO Deskbot Starting...");

  // Initialize display first (for status messages)
  display.begin();
  display.setContrast(255);
  display.setFont(u8g2_font_6x10_tf);

  // Initialize WiFi Manager
  wifiManager.begin();
  
  // Check if reset button is held during boot
  pinMode(PIN_TOUCH_HEAD, INPUT);
  if (touchRead(PIN_TOUCH_HEAD) < 40) {  // Touch detected
    Serial.println("[Boot] Reset button held, starting setup portal");
    inSetupMode = true;
    showSetupScreen();
    wifiManager.startPortal();
  } else {
    // Try auto-connect
    showConnectingScreen(WIFI_SSID);
    if (!wifiManager.autoConnect()) {
      Serial.println("[Boot] WiFi failed, starting setup portal");
      inSetupMode = true;
      showSetupScreen();
      wifiManager.startPortal();
    }
  }

  // Initialize other components
  sensors.begin();
  leds.begin();
  servo.begin();
  audioMgr.begin();

  // Set initial behavior
  eye.setBehavior("calm_idle");

  // Setup WebSocket with dynamic server from WiFiManager
  if (wifiManager.isConnected) {
    ws.setServer(wifiManager.getServerIP().c_str(), wifiManager.getServerPort());
    ws.setCallback(handleMessage);
    ws.begin();
    showConnectedScreen();
    audioMgr.playChirp();
  }

  Serial.println("Ready! Type 'help' for commands.");
}

// Forward declaration
void handleSerialCommand(String cmd);

void loop() {
  // Handle WiFi setup portal if active
  if (wifiManager.isPortalRunning()) {
    wifiManager.handlePortal();
    
    // Show setup animation
    eye.setBehavior("curious_idle");
    eye.update(0.033);
    eye.render();
    delay(33);
    return;
  }

  const unsigned long now = millis();

  // Attempt WS reconnect when WiFi is up
  if (WiFi.status() == WL_CONNECTED && !ws.connected() && (now - lastWsReconnectMs) >= WS_RECONNECT_INTERVAL) {
    lastWsReconnectMs = now;
    wifiManager.currentIP = WiFi.localIP().toString();
    ws.setServer(wifiManager.getServerIP().c_str(), wifiManager.getServerPort());
    ws.begin();
    showConnectedScreen();
  }

  ws.loop();

  // Handle Serial commands for testing without WebSocket
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      handleSerialCommand(cmd);
    }
  }
  
  // Check for WiFi reset (hold touch for 5 seconds)
  if (headTouchedDebounced()) {
    if (buttonHoldStart == 0) {
      buttonHoldStart = millis();
    } else if (millis() - buttonHoldStart > RESET_HOLD_TIME) {
      Serial.println("[Reset] Touch held 5s, resetting WiFi...");
      leds.setRGB(255, 0, 0);  // Red flash
      delay(500);
      wifiManager.resetCredentials();  // This restarts ESP32
    }
  } else {
    buttonHoldStart = 0;
  }

  if (now - lastFrameMs >= FRAME_INTERVAL) {
    lastFrameMs = now;
    eye.update(FRAME_INTERVAL / 1000.0f);
    eye.render();
  }

  if (now - lastSensorMs >= SENSOR_INTERVAL) {
    lastSensorMs = now;
    auto data = sensors.read();
    ws.sendSensors(data);
  }

  if (now - lastPingMs >= PING_INTERVAL) {
    lastPingMs = now;
    ws.sendStatus(robotState, eye.getBehavior());
  }
}

// Simple debounced head-touch read to avoid false resets
bool headTouchedDebounced() {
  static uint8_t stableCount = 0;
  int val = touchRead(PIN_TOUCH_HEAD);
  if (val < TOUCH_THRESHOLD) {
    if (stableCount < 10) stableCount++;
  } else if (val > TOUCH_THRESHOLD + 5 && stableCount > 0) {
    stableCount--;
  }
  return stableCount >= 3;
}

// ============================================================================
// DISPLAY HELPERS
// ============================================================================

void showSetupScreen() {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(20, 15, "EMO Setup Mode");
  display.drawStr(10, 30, "Connect to WiFi:");
  display.drawStr(10, 42, WIFI_MANAGER_AP_NAME);
  display.drawStr(10, 54, "Pass: emo12345");
  display.sendBuffer();
}

void showConnectingScreen(const char* ssid) {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(20, 25, "Connecting to:");
  display.drawStr(10, 40, ssid);
  display.drawStr(40, 55, "...");
  display.sendBuffer();
}

void showConnectedScreen() {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(30, 25, "Connected!");
  display.drawStr(10, 40, wifiManager.currentIP.c_str());
  display.sendBuffer();
  delay(1500);
}

void handleMessage(const char* type, JsonDocument& doc) {
  Serial.printf("[WS] Received: %s\n", type);
  
  if (strcmp(type, "set_behavior") == 0) {
    const char* b = doc["name"] | "calm_idle";
    if (findBehavior(b)) {
      eye.setBehavior(b);
      robotState = "reacting";
      audioMgr.playChirp();
      Serial.printf("[WS] Behavior set to: %s\n", b);
    } else {
      Serial.printf("[WS] Unknown behavior: %s\n", b);
    }
  } else if (strcmp(type, "set_led") == 0) {
    uint8_t r = doc["r"] | 0;
    uint8_t g = doc["g"] | 0;
    uint8_t b = doc["b"] | 0;
    leds.setRGB(r, g, b);
    Serial.printf("[WS] LED set to: %d,%d,%d\n", r, g, b);
  } else if (strcmp(type, "set_servo") == 0) {
    int angle = doc["angle"] | 90;
    servo.setAngle(angle);
    Serial.printf("[WS] Servo set to: %d\n", angle);
  } else if (strcmp(type, "play_sound") == 0) {
    const char* s = doc["name"] | "chirp";
    if (strcmp(s, "chirp") == 0) audioMgr.playChirp();
    else if (strcmp(s, "happy") == 0) audioMgr.playHappy();
    else if (strcmp(s, "sad") == 0) audioMgr.playSad();
    else if (strcmp(s, "alert") == 0) audioMgr.playAlert();
    else audioMgr.playTone(1000, 150);
    Serial.printf("[WS] Sound: %s\n", s);
  } else if (strcmp(type, "play_audio") == 0) {
    const char* url = doc["url"] | "";
    if (strlen(url) > 0) {
      Serial.printf("[WS] Playing audio from: %s\n", url);
      audioMgr.playFromURLAsync(url);
    }
  } else if (strcmp(type, "ping") == 0) {
    // Respond to heartbeat
    StaticJsonDocument<64> pong;
    pong["type"] = "pong";
    ws.send(pong);
  } else if (strcmp(type, "welcome") == 0) {
    Serial.println("[WS] Server acknowledged connection");
  } else {
    Serial.printf("[WS] Unknown message type: %s\n", type);
  }
}

// ============================================================================
// SERIAL COMMAND HANDLER (for testing without WebSocket)
// ============================================================================
void handleSerialCommand(String cmd) {
  Serial.print("> ");
  Serial.println(cmd);

  // Help command
  if (cmd == "help") {
    Serial.println("=== AVAILABLE COMMANDS ===");
    Serial.println("set_behavior:<name>  - Change eye expression");
    Serial.println("  Names: calm_idle, happy, sleepy_idle, curious_idle,");
    Serial.println("         shy_happy, startled, thinking, soft_sad,");
    Serial.println("         listening_focused, processing, embarrassed,");
    Serial.println("         playful_mischief, disappointed, sleeping_deep");
    Serial.println("set_led:<r>,<g>,<b>  - Set LED color (0-255 each)");
    Serial.println("set_servo:<angle>    - Move servo (0-180)");
    Serial.println("play_sound:chirp     - Play chirp sound");
    Serial.println("play_sound:alert     - Play alert sound");
    Serial.println("print_sensors        - Show all sensor values");
    Serial.println("status               - Show current state");
    Serial.println("==========================");
    return;
  }

  // Status command
  if (cmd == "status") {
    Serial.printf("Behavior: %s\n", eye.getBehavior());
    Serial.printf("State: %s\n", robotState);
    Serial.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
    Serial.printf("WebSocket: %s\n", ws.connected() ? "connected" : "disconnected");
    return;
  }

  // Print sensors command
  if (cmd == "print_sensors") {
    auto data = sensors.read();
    Serial.println("=== SENSOR VALUES ===");
    Serial.printf("Light (LDR):     %d (0-4095)\n", data.light);
    Serial.printf("Motion (PIR):    %s\n", data.motion ? "DETECTED" : "none");
    Serial.printf("Distance:        %d mm\n", data.distance_mm);
    Serial.printf("Touch Head:      %s\n", data.touchHead ? "TOUCHED" : "no");
    Serial.printf("Touch Side:      %s\n", data.touchSide ? "TOUCHED" : "no");
    Serial.println("=====================");
    return;
  }

  // set_behavior:name
  if (cmd.startsWith("set_behavior:")) {
    String name = cmd.substring(13);
    name.trim();
    if (findBehavior(name.c_str())) {
      eye.setBehavior(name.c_str());
      robotState = "reacting";
      Serial.printf("OK: Behavior set to '%s'\n", name.c_str());
    } else {
      Serial.printf("ERROR: Unknown behavior '%s'\n", name.c_str());
      Serial.println("Type 'help' to see available behaviors");
    }
    return;
  }

  // set_led:r,g,b
  if (cmd.startsWith("set_led:")) {
    String rgb = cmd.substring(8);
    int r = 0, g = 0, b = 0;
    int idx1 = rgb.indexOf(',');
    int idx2 = rgb.lastIndexOf(',');
    if (idx1 > 0 && idx2 > idx1) {
      r = rgb.substring(0, idx1).toInt();
      g = rgb.substring(idx1 + 1, idx2).toInt();
      b = rgb.substring(idx2 + 1).toInt();
      r = constrain(r, 0, 255);
      g = constrain(g, 0, 255);
      b = constrain(b, 0, 255);
      leds.setRGB(r, g, b);
      Serial.printf("OK: LED set to RGB(%d, %d, %d)\n", r, g, b);
    } else {
      Serial.println("ERROR: Use format set_led:255,0,0");
    }
    return;
  }

  // set_servo:angle
  if (cmd.startsWith("set_servo:")) {
    int angle = cmd.substring(10).toInt();
    angle = constrain(angle, 0, 180);
    servo.setAngle(angle);
    Serial.printf("OK: Servo set to %d degrees\n", angle);
    return;
  }

  // play_sound:name
  if (cmd.startsWith("play_sound:")) {
    String name = cmd.substring(11);
    name.trim();
    if (name == "chirp") {
      audioMgr.playChirp();
      Serial.println("OK: Playing chirp");
    } else if (name == "alert") {
      audioMgr.playTone(1000, 100);
      delay(50);
      audioMgr.playTone(1000, 100);
      delay(50);
      audioMgr.playTone(1000, 100);
      Serial.println("OK: Playing alert");
    } else {
      audioMgr.playTone(800, 150);
      Serial.println("OK: Playing tone");
    }
    return;
  }

  // Unknown command
  Serial.printf("Unknown command: '%s'. Type 'help' for commands.\n", cmd.c_str());
}

// ============================================================================
// FINAL FIXED main.cpp - Fixes "Instant Animation Stop" Bug
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
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
#include "mic_manager.h"
#include "wifi_manager.h"
#include "rtc_manager.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_task_wdt.h"


// --- FUNCTION DECLARATIONS ---
void testAudioSystems();
void processWebSocketMessage(const WsQueueMessage& msg);
void handleMessage(const char* type, JsonDocument& doc);

// --- OBJECTS ---
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);
EyeEngine eye(display);
SensorManager sensors;
LedController leds;
ServoController servo;
RobotWebSocket robotWs;
AudioManager audioMgr;
MicManager micMgr;
WiFiManager wifiMgr;
RTCManager rtcMgr;

// --- SOUND MANAGER ---
class SoundManager {
public:
  void update() {
    if (!active) return;
    
    unsigned long now = millis();
    if (now - lastUpdate >= noteDuration) {
      if (noteIndex < sequenceLength) {
        int freq = sequence[noteIndex * 2];
        noteDuration = sequence[noteIndex * 2 + 1];
        
        if (freq > 0) tone(PIN_BUZZER, freq);
        else noTone(PIN_BUZZER);
        
        lastUpdate = now;
        noteIndex++;
      } else {
        noTone(PIN_BUZZER);
        active = false;
      }
    }
  }

  void play(const char* name) {
    if (active) return;
    
    noteIndex = 0;
    active = true;
    lastUpdate = millis();

    if (strcmp(name, "startup") == 0) {
      setNote(0, 880, 100); setNote(1, 1046, 100); setNote(2, 1318, 200);
      sequenceLength = 3;
    } 
    else if (strcmp(name, "happy") == 0) {
      setNote(0, 1568, 80); setNote(1, 0, 50); setNote(2, 2093, 100);
      sequenceLength = 3;
    }
    else if (strcmp(name, "sad") == 0) {
      setNote(0, 440, 200); setNote(1, 392, 300); setNote(2, 349, 400);
      sequenceLength = 3;
    }
    else if (strcmp(name, "surprised") == 0) {
      setNote(0, 2000, 50); setNote(1, 2500, 50);
      sequenceLength = 2;
    }
    else if (strcmp(name, "curious") == 0) {
      setNote(0, 523, 100); setNote(1, 659, 100); setNote(2, 784, 150);
      sequenceLength = 3;
    }
    else if (strcmp(name, "sleep") == 0) {
      setNote(0, 300, 300); setNote(1, 200, 400);
      sequenceLength = 2;
    }
  }

private:
  bool active = false;
  unsigned long lastUpdate = 0;
  int sequence[20];
  int sequenceLength = 0;
  int noteIndex = 0;
  int noteDuration = 0;

  void setNote(int idx, int freq, int dur) {
    sequence[idx * 2] = freq;
    sequence[idx * 2 + 1] = dur;
  }
} soundFx;

// --- STATE VARIABLES ---
const Behavior* activeBehavior = nullptr;
unsigned long behaviorStartTime = 0;
unsigned long lastInteractionTime = 0;
unsigned long lastIdleCheckTime = 0;
bool inSleepMode = false;
bool inDarkSleepMode = false;

const unsigned long IDLE_TO_SLEEPY_DELAY = 20000;

// --- CROWD-PROOF SETTINGS FOR INTERNATIONAL EVENT ---
const bool PRESENTATION_MODE = true;  // Disable sleep, optimize for crowds
const unsigned long MOTION_COOLDOWN = 3000;   // 3 sec motion cooldown
const int DISTANCE_MIN = 180;         // Ignore very close readings (cm)
const int DISTANCE_MAX = 350;         // Shorter range in crowds
const int VOLUME_THRESHOLD_HIGH = 50; // Less sensitive to crowd noise
const int VOLUME_THRESHOLD_LOW = 25;  // Higher threshold for listening

// --- BEHAVIOR CONTROLLER ---
// FIXED: Accepts 'now' to prevent timing mismatch
void startBehavior(const char* name, unsigned long now) {
  const Behavior* b = findBehavior(name);
  if (!b) return;

  // Prevent re-triggering the same behavior repeatedly
  if (activeBehavior && strcmp(activeBehavior->name, name) == 0) {
      // If it's already playing, just reset the timer (keep it alive)
      behaviorStartTime = now;
      return; 
  }

  Serial.printf("\n[BEHAVIOR] ===== STARTING: %s =====\n", name);

  // Reset interaction timer for non-sleep behaviors
  if (strcmp(name, "sleeping") != 0 && strcmp(name, "sleepy_idle") != 0) {
    inSleepMode = false;
    inDarkSleepMode = false;
    lastInteractionTime = now;
  }

  // Protect active states from sleepy override
  if (strcmp(name, "sleepy_idle") == 0) {
    if (activeBehavior && (strcmp(activeBehavior->name, "happy") == 0 ||
                           strcmp(activeBehavior->name, "surprised") == 0 ||
                           strcmp(activeBehavior->name, "listening") == 0)) {
      return; 
    }
    inSleepMode = true;
  }
  else if (strcmp(name, "sleeping") == 0) {
    inDarkSleepMode = true;
  }

  activeBehavior = b;
  behaviorStartTime = now; // CRITICAL: Use the synchronized 'now' time
  
  // 1. SET EYE TARGET
  eye.setTarget(b);
  
  // 2. SET LED MOOD
  if (b->ledEffect) leds.setMood(b->ledEffect);
  else leds.setMood(name);
  
  // 3. SERVO & SOUND ACTIONS (synced to behavior durations for natural movement)
  // Calculate total behavior duration for auto-return
  unsigned long behaviorDuration = b->entryTime + b->holdTime + b->exitTime;
  if (behaviorDuration == 0) behaviorDuration = 5000; // Default for infinite behaviors
  
  if (strcmp(name, "happy") == 0 || strcmp(name, "shy_happy") == 0) {
    servo.triggerGesture("shake", behaviorDuration); // Shake converted from nod for cardboard
    soundFx.play("happy");
  } 
  else if (strcmp(name, "sad") == 0) {
    servo.setTarget(70, behaviorDuration); // Droop left (cardboard safe: 60-120)
    soundFx.play("sad");
  }
  else if (strcmp(name, "surprised") == 0 || strcmp(name, "startled") == 0) {
    servo.setTarget(75, behaviorDuration); // Quick jolt left
    soundFx.play("surprised");
  }
  else if (strcmp(name, "curious_idle") == 0) {
    servo.triggerGesture("tilt", behaviorDuration);
    soundFx.play("curious");
  }
  else if (strcmp(name, "sleepy_idle") == 0) {
    servo.setTarget(100, behaviorDuration); // Slight tilt
    soundFx.play("sleep");
  }
  else if (strcmp(name, "listening") == 0 || strcmp(name, "calm_idle") == 0) {
    servo.returnToCenter(); // Always center for these
  }
  else if (strcmp(name, "confused") == 0) {
    servo.setTarget(105, behaviorDuration); // Tilt right
  }
  else if (strcmp(name, "thinking") == 0) {
    servo.setTarget(95, behaviorDuration); // Slight tilt
  }
  else if (strcmp(name, "playful_mischief") == 0) {
    servo.triggerGesture("shake", behaviorDuration);
  }

  if (robotWs.isConnected()) {
    robotWs.sendStatus("sync_behavior", name);
  }
}

// Wrapper for calls without timestamp (uses millis)
void startBehavior(const char* name) {
    startBehavior(name, millis());
}

// Track if behavior was triggered from web UI (don't override with sensors)
bool webBehaviorActive = false;
unsigned long webBehaviorTime = 0;

// Process websocket messages from the queue
void processWebSocketMessage(const WsQueueMessage& msg) {
  switch (msg.type) {
    case WS_MSG_SET_BEHAVIOR:
      webBehaviorActive = true;
      webBehaviorTime = millis();
      startBehavior(msg.data, millis());
      break;
    case WS_MSG_SERVO_ACTION:
      servo.setTarget(msg.intValue, 3000);
      lastInteractionTime = millis();
      break;
    case WS_MSG_LED_ACTION:
      Serial.printf("[LED] Web command: %s\n", msg.data);
      if (strcmp(msg.data, "off") == 0) {
        leds.setMood("sleeping");
      } else {
        leds.setMood(msg.data);
      }
      lastInteractionTime = millis();
      break;
    case WS_MSG_PLAY_AUDIO:
      startBehavior("listening");
      audioMgr.playURL(msg.data);
      lastInteractionTime = millis();
      break;
    case WS_MSG_REQUEST_STATE:
      if (activeBehavior && robotWs.isConnected()) {
        robotWs.sendStatus("sync_behavior", activeBehavior->name);
      }
      break;
    case WS_MSG_STOPWATCH_START:
      rtcMgr.stopwatchStart();
      break;
    case WS_MSG_STOPWATCH_STOP:
      rtcMgr.stopwatchStop();
      break;
    case WS_MSG_STOPWATCH_RESET:
      rtcMgr.stopwatchReset();
      break;
    default:
      break;
  }
}


void handleMessage(const char* type, JsonDocument& doc) {
  if (strcmp(type, "set_behavior") == 0) {
    webBehaviorActive = true;
    webBehaviorTime = millis();
    startBehavior(doc["name"], millis()); // FIXED: Use millis() instead of wrapper
  }
  else if (strcmp(type, "servo_action") == 0) {
    servo.setTarget(doc["angle"], 3000); // Auto-return after 3s
    lastInteractionTime = millis();
  }
  else if (strcmp(type, "led_action") == 0) {
    // Handle LED color commands from web UI - supports hex colors
    const char* color = doc["color"];
    if (color) {
      Serial.printf("[LED] Web command: %s\n", color);
      
      if (strcmp(color, "off") == 0) {
        leds.setMood("sleeping");
      } 
      else if (strcmp(color, "#ff0000") == 0) leds.setMood("red");
      else if (strcmp(color, "#00ff00") == 0) leds.setMood("green");
      else if (strcmp(color, "#0000ff") == 0) leds.setMood("blue");
      else if (strcmp(color, "#ffff00") == 0) leds.setMood("happy");
      else if (strcmp(color, "#ff00ff") == 0) leds.setMood("purple");
      else if (strcmp(color, "#00ffff") == 0) leds.setMood("cyan");
      else if (strcmp(color, "#ffffff") == 0) leds.setMood("surprised");
      else leds.setMood(color);
    }
    lastInteractionTime = millis();
  }
  else if (strcmp(type, "play_audio") == 0) {
    startBehavior("listening");
    
    // Check if there's text to speak
    if (doc.containsKey("text")) {
      const char* text = doc["text"];
      Serial.printf("[AUDIO] Speaking text: %s\n", text);
      audioMgr.speakText(text);
    } else if (doc.containsKey("url")) {
      // Play audio from URL
      audioMgr.playURL(doc["url"]);
    }
  }
  else if (strcmp(type, "request_state") == 0) {
    if (activeBehavior && robotWs.isConnected()) {
      robotWs.sendStatus("sync_behavior", activeBehavior->name);
    }
  }
  // ============= STOPWATCH COMMANDS =============
  else if (strcmp(type, "stopwatch_start") == 0) {
    rtcMgr.stopwatchStart();
  }
  else if (strcmp(type, "stopwatch_stop") == 0) {
    rtcMgr.stopwatchStop();
  }
  else if (strcmp(type, "stopwatch_reset") == 0) {
    rtcMgr.stopwatchReset();
  }
  // ============= AUDIO TEST COMMAND =============
  else if (strcmp(type, "test_audio") == 0) {
    Serial.println("[AUDIO] Starting audio system test...");
    startBehavior("listening"); // Visual feedback
    testAudioSystems();
    lastInteractionTime = millis();
  }
  // ============= WAKE UP COMMANDS =============
  else if (strcmp(type, "wake_up") == 0) {
    Serial.println("[WAKE] Wake up command received");
    // Force the robot out of sleep state
    inSleepMode = false;
    lastInteractionTime = millis();
    
    // Show appropriate expression based on the wake-up reason
    if (doc.containsKey("expression")) {
      const char* expression = doc["expression"];
      Serial.printf("[WAKE] Setting expression: %s\n", expression);
      startBehavior(expression);
    } else {
      // Default wake-up behavior
      startBehavior("wake_up");
    }
  }
  else if (strcmp(type, "stay_awake") == 0) {
    Serial.println("[WAKE] Stay awake command received");
    // Keep the robot active for specified duration
    unsigned long duration = 25000; // Default 25 seconds
    if (doc.containsKey("duration")) {
      duration = doc["duration"];
    }
    
    Serial.printf("[WAKE] Staying awake for %lu ms\n", duration);
    lastInteractionTime = millis();
    inSleepMode = false;
    
    // Start random movement behavior
    startBehavior("random_movement");
  }
}

// --- AUDIO TESTING FUNCTION ---
void testAudioSystems() {
  Serial.println("\n=== AUDIO SYSTEM TEST START ===");
  
  // Test 1: Buzzer Sound Sequence
  Serial.println("[TEST 1] Testing buzzer on PIN 19...");
  for (int i = 0; i < 3; i++) {
    tone(PIN_BUZZER, 1000 + (i * 200), 300);
    delay(400);
    noTone(PIN_BUZZER);
    delay(100);
  }
  
  // Test 2: I2S Speaker streaming
  Serial.println("[TEST 2] Testing I2S speaker (streaming)...");
  if (WiFi.status() == WL_CONNECTED) {
    // Play a short test audio from internet
    audioMgr.testAudio();
    Serial.println("[TEST 2] Streaming test audio...");
  } else {
    Serial.println("[TEST 2] No WiFi - skipping stream test");
  }
  
  Serial.println("=== AUDIO SYSTEM TEST COMPLETE ===\n");
  
  delay(500);
  startBehavior("calm_idle");
}

// --- SETUP ---
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n========================================");
  Serial.println("  DESKBOT COMPANION - FINAL FIX");
  Serial.println("========================================\n");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  display.begin();
  leds.begin();
  servo.begin();
  sensors.begin();
  rtcMgr.begin();
  soundFx.play("startup");

  Serial.println("[INIT] WiFi...");
  wifiMgr.begin();
  if (wifiMgr.autoConnect()) {
      // Initialize I2S Audio
      audioMgr.begin();
      
      robotWs.setServer(wifiMgr.getServerIP().c_str(), wifiMgr.getServerPort());
      robotWs.begin();
  }
  
  eye.startBootSequence();
  startBehavior("calm_idle");
  lastInteractionTime = millis();
  lastIdleCheckTime = millis();
  
  // Disable default WDT and use manual feeding during sleep
  esp_task_wdt_init(30, false); // 30 second timeout, don't panic on timeout
}

void loop() {
  static unsigned long lastTime = 0;
  static unsigned long lastWiFiCheck = 0;
  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0f;
  lastTime = now;

  // RESTART FIX: Periodic WiFi health check to prevent silent disconnection
  if (now - lastWiFiCheck > 30000) { // Every 30 seconds
    lastWiFiCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WIFI] Connection lost, reconnecting...");
      WiFi.reconnect();
    }
  }

  // 1. Critical Loops
  if (WiFi.status() == WL_CONNECTED) {
    robotWs.loop();
    audioMgr.update();
    
    // Process WebSocket messages from queue
    WsQueueMessage wsMsg;
    while (robotWs.getMessage(wsMsg)) {
      processWebSocketMessage(wsMsg);
    }
  } else {
    wifiMgr.handlePortal();
  }

  // 2. Component Updates
  eye.update(dt);
  leds.loop(dt);
  servo.loop(dt);
  soundFx.update();
  
  // SLEEP FIX: Skip ultrasonic during sleep to prevent micro-freezes
  bool inSleepState = inSleepMode || inDarkSleepMode;
  sensors.update(inSleepState); // Skip ultrasonic when sleeping
  
  // Update stopwatch display if running
  if (rtcMgr.isStopwatchRunning()) {
    int m, s, c;
    rtcMgr.getStopwatchTime(m, s, c);
    eye.showStopwatch(m, s, c);
  } else {
    eye.hideStopwatch();
  }
  
  eye.render(); // Render after all updates
  
  // FREEZE FIX: Call audio update again after render to keep playback smooth
  if (WiFi.status() == WL_CONNECTED && audioMgr.getIsPlaying()) {
    audioMgr.update();
  }

  // 3. Sensor Logic (Crowd-Proof)
  static unsigned long lastSensor = 0;
  static unsigned long lastMotionTrigger = 0;
  static unsigned long lastVolumeTrigger = 0;
  
  // SENSOR DEBOUNCE: Extended for crowd environments
  if (now - lastSensor > (PRESENTATION_MODE ? 200 : 100)) {
    lastSensor = now;
    SensorData d = sensors.read();
    bool activityDetected = false;
    bool servoIsMoving = servo.isMoving();
    
    // 1. TOUCH
    if (d.touchHead) {
      // Only trigger if we aren't ALREADY happy (prevents restart spam)
      if (!activeBehavior || strcmp(activeBehavior->name, "happy") != 0) {
          Serial.println("\n[TOUCH] HEAD TOUCHED!");
          startBehavior("happy", now); 
      }
      activityDetected = true;
    } 
    else if (d.touchSide) {
      if (!activeBehavior || strcmp(activeBehavior->name, "shy_happy") != 0) {
          Serial.println("\n[TOUCH] SIDE TOUCHED!");
          startBehavior("shy_happy", now);
      }
      activityDetected = true;
    } 

    // Skip sensor triggers if web UI just sent a behavior command (let it play fully)
    // Use actual behavior duration instead of fixed 3000ms
    unsigned long behaviorProtection = 3000; // Default
    if (webBehaviorActive && activeBehavior) {
      behaviorProtection = activeBehavior->entryTime + activeBehavior->holdTime + activeBehavior->exitTime + 500;
    }
    bool allowSensorTrigger = !webBehaviorActive || (now - webBehaviorTime > behaviorProtection);
    
    // Clear web behavior flag once protection period ends
    if (webBehaviorActive && (now - webBehaviorTime > behaviorProtection)) {
      webBehaviorActive = false;
    }

    // 2. MOTION (crowd-proof with cooldown)
    if (allowSensorTrigger && d.motion && (now - lastMotionTrigger > MOTION_COOLDOWN)) {
       if (!activeBehavior || (strcmp(activeBehavior->name, "surprised") != 0 && strcmp(activeBehavior->name, "listening") != 0)) {
          Serial.println("\n[MOTION] DETECTED! (crowd-proof)");
          startBehavior("surprised", now);
          lastMotionTrigger = now;
       }
       activityDetected = true;
    }
    
    // 3. DISTANCE (crowd-proof ranges)
    if (allowSensorTrigger && d.distance_mm > DISTANCE_MIN && d.distance_mm < (DISTANCE_MIN + 50)) {
      if (!activeBehavior || strcmp(activeBehavior->name, "surprised") != 0) {
        Serial.printf("\n[DISTANCE] Close: %dmm (crowd-proof)\n", d.distance_mm);
        startBehavior("surprised", now);
      }
      activityDetected = true;
    } 
    else if (allowSensorTrigger && d.distance_mm > (DISTANCE_MIN + 50) && d.distance_mm < DISTANCE_MAX) {
      if (!activeBehavior || strcmp(activeBehavior->name, "curious_idle") != 0) {
        Serial.printf("\n[DISTANCE] Medium: %dmm (crowd-proof)\n", d.distance_mm);
        startBehavior("curious_idle", now);
      }
      activityDetected = true;
    }

    // 4. MICROPHONE (crowd-proof thresholds)
    #if ENABLE_MICROPHONE
    if (!servoIsMoving && (now - lastVolumeTrigger > 2000)) { // 2 sec audio cooldown
      int vol = micMgr.getLoudness();
      if (vol > VOLUME_THRESHOLD_HIGH) {
        startBehavior("surprised", now);
        activityDetected = true;
        lastVolumeTrigger = now;
      } 
      else if (vol > VOLUME_THRESHOLD_LOW) {
        startBehavior("listening", now);
        activityDetected = true;
        lastVolumeTrigger = now;
      }
      if (vol > 20) leds.voiceReact(vol); // Higher threshold for LED react
    }
    #endif

    // 5. Reset sleep timers
    if (activityDetected) {
      lastInteractionTime = now;
      inSleepMode = false;
      inDarkSleepMode = false;
    }

    // 6. DARKNESS LOGIC
    if (now - lastInteractionTime > 15000) { 
      if (d.light > 3000) { 
         if (!inDarkSleepMode) { 
           startBehavior("sleeping", now);
         }
      } 
      else if (inDarkSleepMode) {
         startBehavior("calm_idle", now);
      }
    }
    
    // RESTART FIX: Send sensors less frequently during sleep (every 2 sec instead of 200ms)
    static unsigned long lastSensorSend = 0;
    unsigned long sensorSendInterval = (inSleepMode || inDarkSleepMode) ? 2000 : 500;
    if (robotWs.isConnected() && (now - lastSensorSend > sensorSendInterval)) {
      robotWs.sendSensors(d);
      lastSensorSend = now;
    }
  }
  
  // 4. Idle Management (Presentation Mode)
  if (now - lastIdleCheckTime > 1000) {
    lastIdleCheckTime = now;
    unsigned long idleTime = now - lastInteractionTime;
    unsigned long sleepDelay = PRESENTATION_MODE ? (IDLE_TO_SLEEPY_DELAY * 3) : IDLE_TO_SLEEPY_DELAY; // 60 sec in presentation mode
    
    if (!inDarkSleepMode && activeBehavior && idleTime > sleepDelay && !inSleepMode) {
      if (strcmp(activeBehavior->name, "sleepy_idle") != 0 && 
          strcmp(activeBehavior->name, "sleeping") != 0) {
        startBehavior("sleepy_idle", now);
      }
    }
  }
  
  // 5. AUTO-RETURN (FIXED MATH)
  if (activeBehavior && activeBehavior->holdTime > 0) {
    unsigned long elapsed = now - behaviorStartTime;
    unsigned long totalDuration = activeBehavior->entryTime + 
                                   activeBehavior->holdTime + 
                                   activeBehavior->exitTime;
    
    // For web behaviors, check if they should naturally end
    if (webBehaviorActive && elapsed > totalDuration + 500) {
      Serial.printf("\n[WEB-BEHAVIOR] %s finished (%lums) -> clearing webBehaviorActive\n", 
                    activeBehavior->name, elapsed);
      webBehaviorActive = false; // Clear the flag so AUTO-RETURN can work
    }
    
    // Normal AUTO-RETURN logic (only if not web behavior)
    if (!webBehaviorActive && elapsed > totalDuration + 500) {
      Serial.printf("\n[AUTO-RETURN] %s finished (%lums) -> calm_idle\n", 
                    activeBehavior->name, elapsed);
      
      // Only return if we're not already calm
      if (strcmp(activeBehavior->name, "calm_idle") != 0) {
        startBehavior("calm_idle", now);
      }
    }
  }
  
  // RESTART FIX: Feed watchdog and add small delay during sleep
  // This prevents WDT timeout during long idle periods
  if (inSleepMode || inDarkSleepMode) {
    esp_task_wdt_reset(); // Feed the watchdog
    delay(10); // Small delay to let background tasks (WiFi, WS heartbeat) run
  }
  
  yield();
}
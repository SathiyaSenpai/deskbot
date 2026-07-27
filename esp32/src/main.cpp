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
    else if (strcmp(name, "alarm") == 0) {
      setNote(0, 2048, 150); setNote(1, 0, 100); setNote(2, 2048, 150); setNote(3, 0, 100); setNote(4, 2560, 300);
      sequenceLength = 5;
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
  else if (strcmp(name, "sleeping") == 0) {
    // CRITICAL: Return servo to center and stop all movement during sleep
    servo.returnToCenter();
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
      } 
      else if (strcmp(msg.data, "#ff0000") == 0) leds.setMood("red");
      else if (strcmp(msg.data, "#00ff00") == 0) leds.setMood("green");
      else if (strcmp(msg.data, "#0000ff") == 0) leds.setMood("blue");
      else if (strcmp(msg.data, "#ffff00") == 0) leds.setMood("happy");
      else if (strcmp(msg.data, "#ff00ff") == 0) leds.setMood("purple");
      else if (strcmp(msg.data, "#00ffff") == 0) leds.setMood("cyan");
      else if (strcmp(msg.data, "#ffffff") == 0) leds.setMood("surprised");
      else leds.setMood(msg.data);
      lastInteractionTime = millis();
      break;
    case WS_MSG_PLAY_AUDIO:
      #if ENABLE_MICROPHONE
      micMgr.stopRecording(); // Time-multiplexing: stop mic driver during audio playback
      #endif
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
    case WS_MSG_SET_ALARM:
      rtcMgr.setAlarm(msg.intValue, msg.intValue2);
      if (robotWs.isConnected()) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Alarm set for %02d:%02d", msg.intValue, msg.intValue2);
        robotWs.sendStatus("alarm_set", buf);
      }
      break;
    case WS_MSG_DISMISS_ALARM:
      rtcMgr.dismissAlarm();
      if (robotWs.isConnected()) {
        robotWs.sendStatus("alarm_dismissed", "Alarm dismissed");
      }
      break;
    case WS_MSG_SYNC_TIME: {
      int y = 2026, mo = 7, d = 27, h = 12, min = 0, sec = 0;
      if (sscanf(msg.data, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &min, &sec) == 6) {
        rtcMgr.syncTime(y, mo, d, h, min, sec);
      }
      break;
    }
    default:
      break;
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
  // Configure WDT FIRST — extends timeout from default 5s to 30s
  // This prevents WDT resets during WiFi connect, I2C probe, and audio init
  esp_task_wdt_init(30, false);
  esp_task_wdt_add(NULL); // Subscribe main task to WDT
  esp_task_wdt_reset();   // Feed immediately

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n========================================");
  Serial.println("  NISYA COMPANION - FINAL FIX");
  Serial.println("========================================\n");

  // NOTE: Wire is initialized inside rtcMgr.begin() with correct pins.
  // Do NOT call Wire.begin() separately — calling it twice corrupts the ESP32 I2C
  // driver mutex (_impl pointer), causing a LoadProhibited crash on next I2C access.
  rtcMgr.begin(PIN_I2C_SDA, PIN_I2C_SCL); // Initializes Wire once, then DS3231
  display.begin();
  leds.begin();
  servo.begin();
  sensors.begin();
  #if ENABLE_MICROPHONE
  micMgr.begin();
  #endif
  soundFx.play("startup");

  Serial.println("[INIT] WiFi...");
  wifiMgr.begin();
  if (wifiMgr.autoConnect()) {
      // Initialize I2S Audio
      audioMgr.begin();
      
      robotWs.setServer(wifiMgr.getServerIP().c_str(), wifiMgr.getServerPort());
      robotWs.begin();
  } else {
      Serial.println(F("[WIFI] AutoConnect failed -> Starting AP Portal (Nisya-Setup)"));
      wifiMgr.startPortal();
      // Initialize audio in AP mode as well for offline operation
      audioMgr.begin();
  }
  
  eye.startBootSequence();
  startBehavior("calm_idle");
  lastInteractionTime = millis();
  lastIdleCheckTime = millis();
  
  Serial.printf("[MEM] Post-Init Free Heap: %d bytes | Max Alloc Block: %d bytes\n", 
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

void loop() {
  esp_task_wdt_reset(); // Feed WDT every loop — prevents reset under heavy load

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
  
  // Sensors update: read ultrasonic every 5s in sleep, 200ms when active
  bool inSleepState = inSleepMode || inDarkSleepMode;
  sensors.update(inSleepState); // Slower interval in sleep mode, but still reads
  
  // Check DS3231 Hardware Alarm
  rtcMgr.checkAlarm();
  if (rtcMgr.isAlarmTriggered()) {
    rtcMgr.dismissAlarm(); // Clear trigger flag
    Serial.println("\n[ALARM] DS3231 ALARM RINGING!");
    startBehavior("wake_up", now);
    soundFx.play("alarm");
    if (robotWs.isConnected()) {
      robotWs.sendStatus("alarm_triggered", "DS3231 Alarm Ringing");
    }
    lastInteractionTime = now;
    inSleepMode = false;
    inDarkSleepMode = false;
  }
  
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
  static bool previousMotionState = false;
  static bool motionPresenceActive = false; // Tracks if someone is present (with hold time)
  static unsigned long motionStartTime = 0;
  static unsigned long lastMotionHigh = 0; // Last time PIR was HIGH
  static unsigned long lastIdleMovement = 0;
  const unsigned long MOTION_CONTINUOUS_RETRIGGER = 300000; // 5 minutes for continuous presence
  const unsigned long MOTION_HOLD_TIME = 10000; // 10 sec - ignore brief LOW periods (person still there)
  const unsigned long MOTION_AWAY_CONFIRM = 15000; // 15 sec of no motion = person left
  
  // DYNAMIC TOUCH DYNAMICS STATE
  static unsigned long lastTouchTime = 0;
  static unsigned long touchRefractoryWindow = 0;
  static int touchStreakCount = 0;
  static unsigned long lastTouchStreakTime = 0;
  static bool pendingTouchReaction = false;
  static unsigned long touchReactionTargetTime = 0;
  static const char* pendingTouchBehavior = nullptr;

  // Process pending organic touch reaction (simulates 50-180ms nerve response latency)
  if (pendingTouchReaction && now >= touchReactionTargetTime) {
    pendingTouchReaction = false;
    if (pendingTouchBehavior) {
      Serial.printf("[TOUCH-DYNAMICS] Triggering organic reaction '%s' (Streak: %d)\n", 
                    pendingTouchBehavior, touchStreakCount);
      startBehavior(pendingTouchBehavior, now);
    }
  }
  
  // SENSOR DEBOUNCE: Every 5 seconds in sleep mode, normal rate when active
  unsigned long sensorInterval = inSleepState ? 5000 : (PRESENTATION_MODE ? 200 : 100);
  if (now - lastSensor > sensorInterval) {
    lastSensor = now;
    SensorData d = sensors.read(rtcMgr.getTemperature()); // Always read sensors with DS3231 temp
    bool activityDetected = false;
    bool servoIsMoving = servo.isMoving();
    
    // 1. DYNAMIC TOUCH HANDLING
    // Reset streak count if no touch for > 12 seconds
    if (touchStreakCount > 0 && (now - lastTouchStreakTime > 12000)) {
      touchStreakCount = 0;
      Serial.println(F("[TOUCH-DYNAMICS] Streak reset (idle > 12s)"));
    }

    if (d.touchHead || d.touchSide) {
      activityDetected = true;
      // Check if we are outside the dynamic refractory window
      if (now - lastTouchTime >= touchRefractoryWindow) {
        lastTouchTime = now;
        lastTouchStreakTime = now;
        touchStreakCount++;

        const char* behaviorToTrigger = "happy";
        
        if (touchStreakCount >= 4) {
          // OVER-PETTING / TICKLED: Companion enters tickled/playful state & long cool-down
          behaviorToTrigger = (random(0, 2) == 0) ? "playful_mischief" : "confused";
          touchRefractoryWindow = random(8000, 12000); // 8 to 12 second cool-down
          Serial.printf("\n[TOUCH-DYNAMICS] Over-petted (Streak %d)! Tickle reaction '%s', Refractory: %lu ms\n", 
                        touchStreakCount, behaviorToTrigger, touchRefractoryWindow);
        } else if (touchStreakCount == 2 || touchStreakCount == 3) {
          // CONTINUOUS PETTING: Affection response
          behaviorToTrigger = d.touchHead ? "happy" : "curious_idle";
          touchRefractoryWindow = random(3000, 5000); // 3 to 5 second window
          Serial.printf("\n[TOUCH-DYNAMICS] Continuous petting (Streak %d): '%s', Refractory: %lu ms\n", 
                        touchStreakCount, behaviorToTrigger, touchRefractoryWindow);
        } else {
          // FIRST TOUCH: Initial friendly reaction
          behaviorToTrigger = d.touchHead ? "happy" : "shy_happy";
          touchRefractoryWindow = random(3500, 6500); // 3.5 to 6.5 second window
          Serial.printf("\n[TOUCH-DYNAMICS] Initial touch (Streak 1): '%s', Refractory: %lu ms\n", 
                        behaviorToTrigger, touchRefractoryWindow);
        }

        // Queue reaction with organic delay (50ms - 180ms)
        pendingTouchReaction = true;
        pendingTouchBehavior = behaviorToTrigger;
        touchReactionTargetTime = now + random(50, 180);
      }
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

    // 2. MOTION (smart presence detection - ignores small movements when present)
    // Track when PIR was last HIGH
    if (d.motion) {
      lastMotionHigh = now;
    }
    
    // Determine if someone is truly present (with hold time to ignore flickers)
    bool wasPresent = motionPresenceActive;
    
    if (d.motion && !motionPresenceActive) {
      // New person arrived
      motionPresenceActive = true;
      motionStartTime = now;
    } else if (!d.motion && motionPresenceActive) {
      // PIR went LOW - but is person still there? (small movements cause brief LOW)
      if (now - lastMotionHigh > MOTION_AWAY_CONFIRM) {
        // No motion for 15 sec = person left
        motionPresenceActive = false;
        Serial.println("[MOTION] Person left (15s no motion)");
      }
      // Otherwise, keep motionPresenceActive = true (person still there, just not moving)
    }
    
    // Only trigger on TRUE new arrival (not small movements while present)
    bool isNewArrival = motionPresenceActive && !wasPresent;
    bool continuousPresenceRetrigger = motionPresenceActive && wasPresent && 
                                       (now - motionStartTime > MOTION_CONTINUOUS_RETRIGGER);
    
    if (allowSensorTrigger && (isNewArrival || continuousPresenceRetrigger) && 
        (now - lastMotionTrigger > MOTION_COOLDOWN)) {
       if (!activeBehavior || (strcmp(activeBehavior->name, "surprised") != 0 && strcmp(activeBehavior->name, "listening") != 0)) {
          if (isNewArrival) {
            Serial.println("\n[MOTION] NEW PERSON ARRIVED!");
          } else {
            Serial.println("\n[MOTION] CONTINUOUS PRESENCE RE-TRIGGER (5+ min)");
            motionStartTime = now; // Reset timer
          }
          startBehavior("surprised", now);
          lastMotionTrigger = now;
       }
       activityDetected = true;
    }
    
    previousMotionState = d.motion; // Update raw motion state for debugging
    
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

    // 4. MICROPHONE & VOICE STREAMING (Time-multiplexed)
    #if ENABLE_MICROPHONE
    bool isAudioPlaying = audioMgr.getIsPlaying();
    if (isAudioPlaying) {
      // Ensure mic is uninstalled while speaker plays to save DMA RAM & avoid feedback
      if (micMgr.isReady()) micMgr.stopRecording();
    } else if (!servoIsMoving && (now - lastVolumeTrigger > 1500)) {
      int vol = micMgr.getLoudness();
      if (vol > MIC_VAD_THRESHOLD) {
        startBehavior("listening", now);
        activityDetected = true;
        lastVolumeTrigger = now;
        
        // Stream audio chunk to websocket if connected
        if (robotWs.isConnected() && micMgr.isReady()) {
          uint8_t pcmBuf[256];
          size_t bytesRead = micMgr.readChunk(pcmBuf, sizeof(pcmBuf));
          if (bytesRead > 0) {
            robotWs.sendAudioChunk(pcmBuf, bytesRead);
          }
        }
      } 
      else if (vol > VOLUME_THRESHOLD_LOW) {
        startBehavior("listening", now);
        activityDetected = true;
        lastVolumeTrigger = now;
      }
      if (vol > 20) leds.voiceReact(vol); // Voice visual reaction
    }
    #endif

    // 5. Reset sleep timers
    if (activityDetected) {
      lastInteractionTime = now;
      inSleepMode = false;
      inDarkSleepMode = false;
    }
    
    // 6. AUTONOMOUS IDLE EYE MOVEMENTS (when calm and no activity)
    // CRITICAL: Disable autonomous movements during sleep to prevent jitter
    if (!activityDetected && !inSleepMode && !inDarkSleepMode && activeBehavior && 
        strcmp(activeBehavior->name, "calm_idle") == 0 && 
        (now - lastIdleMovement > 8000 + random(5000))) { // 8-13 second intervals
      
      // Trigger gentle random servo movements during calm idle
      int randomAngle = 90 + random(-12, 13); // ±12° from center (78-102°)
      servo.setIdleMovement(randomAngle, 3000); // 3 second gentle movement
      lastIdleMovement = now;
      
      Serial.printf("[IDLE] Autonomous eye movement to %d°\n", randomAngle);
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
  // AUDIO WAKE FIX: Keep awake while audio is playing
  bool audioPlaying = audioMgr.getIsPlaying();
  if (audioPlaying) {
    lastInteractionTime = now; // Reset idle timer while audio plays
  }
  
  if (now - lastIdleCheckTime > 1000) {
    lastIdleCheckTime = now;
    unsigned long idleTime = now - lastInteractionTime;
    unsigned long sleepDelay = PRESENTATION_MODE ? (IDLE_TO_SLEEPY_DELAY * 3) : IDLE_TO_SLEEPY_DELAY; // 60 sec in presentation mode
    
    // Don't go to sleep if audio is playing
    if (!audioPlaying && !inDarkSleepMode && activeBehavior && idleTime > sleepDelay && !inSleepMode) {
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
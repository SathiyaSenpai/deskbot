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
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

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

void handleMessage(const char* type, JsonDocument& doc) {
  if (strcmp(type, "set_behavior") == 0) {
    startBehavior(doc["name"]);
  }
  else if (strcmp(type, "servo_action") == 0) {
    servo.setTarget(doc["angle"], 3000); // Auto-return after 3s
    lastInteractionTime = millis();
  }
  else if (strcmp(type, "led_action") == 0) {
    // Handle LED color commands from web UI
    const char* color = doc["color"];
    if (color) {
      if (strcmp(color, "off") == 0) {
        leds.setMood("sleeping"); // Dim mode
      } else {
        leds.setMood(color); // Direct color name
      }
      Serial.printf("[LED] Web command: %s\n", color);
    }
    lastInteractionTime = millis();
  }
  else if (strcmp(type, "play_audio") == 0) {
    startBehavior("listening");
    audioMgr.playFromURLAsync(doc["url"]);
  }
  else if (strcmp(type, "request_state") == 0) {
    if (activeBehavior && robotWs.isConnected()) {
      robotWs.sendStatus("sync_behavior", activeBehavior->name);
    }
  }
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
  soundFx.play("startup");

  Serial.println("[INIT] WiFi...");
  wifiMgr.begin();
  if (wifiMgr.autoConnect()) {
      audioMgr.begin();
      robotWs.setServer(wifiMgr.getServerIP().c_str(), wifiMgr.getServerPort());
      robotWs.setCallback(handleMessage);
      robotWs.begin();
  }
  
  eye.startBootSequence();
  startBehavior("calm_idle");
  lastInteractionTime = millis();
  lastIdleCheckTime = millis();
}

void loop() {
  static unsigned long lastTime = 0;
  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0f;
  lastTime = now;

  // 1. Critical Loops
  if (WiFi.status() == WL_CONNECTED) {
    robotWs.loop();
    audioMgr.loop();
  } else {
    wifiMgr.handlePortal();
  }

  // 2. Component Updates
  eye.update(dt);
  eye.render();
  leds.loop(dt);
  servo.loop(dt);
  soundFx.update();
  sensors.update(); // NON-BLOCKING: Updates async ultrasonic sensor

  // 3. Sensor Logic
  static unsigned long lastSensor = 0;
  
  // SENSOR DEBOUNCE: Prevents glitchy repeated triggers
  if (now - lastSensor > 100) {
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

    // 2. MOTION
    if (d.motion) {
       if (!activeBehavior || (strcmp(activeBehavior->name, "surprised") != 0 && strcmp(activeBehavior->name, "listening") != 0)) {
          Serial.println("\n[MOTION] DETECTED!");
          startBehavior("surprised", now);
       }
       activityDetected = true;
    }
    
    // 3. DISTANCE
    if (d.distance_mm > 0 && d.distance_mm < 150) {
      if (!activeBehavior || strcmp(activeBehavior->name, "surprised") != 0) {
        Serial.printf("\n[DISTANCE] Close: %dmm\n", d.distance_mm);
        startBehavior("surprised", now);
      }
      activityDetected = true;
    } 
    else if (d.distance_mm > 150 && d.distance_mm < 400) {
      if (!activeBehavior || strcmp(activeBehavior->name, "curious_idle") != 0) {
        Serial.printf("\n[DISTANCE] Medium: %dmm\n", d.distance_mm);
        startBehavior("curious_idle", now);
      }
      activityDetected = true;
    }

    // 4. MICROPHONE
    #if ENABLE_MICROPHONE
    if (!servoIsMoving) {
      int vol = micMgr.getLoudness();
      if (vol > 40) {
        startBehavior("surprised", now);
        activityDetected = true;
      } 
      else if (vol > 15) {
        startBehavior("listening", now);
        activityDetected = true;
      }
      if (vol > 10) leds.voiceReact(vol);
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
    
    if (robotWs.isConnected()) robotWs.sendSensors(d);
  }
  
  // 4. Idle Management
  if (now - lastIdleCheckTime > 1000) {
    lastIdleCheckTime = now;
    unsigned long idleTime = now - lastInteractionTime;
    
    if (!inDarkSleepMode && activeBehavior && idleTime > IDLE_TO_SLEEPY_DELAY && !inSleepMode) {
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
    
    // Add 500ms grace period to allow smooth morphing completion
    if (elapsed > totalDuration + 500) {
      Serial.printf("\n[AUTO-RETURN] %s finished (%lums) -> calm_idle\n", 
                    activeBehavior->name, elapsed);
      
      // Only return if we're not already calm
      if (strcmp(activeBehavior->name, "calm_idle") != 0) {
        startBehavior("calm_idle", now);
      }
    }
}
  yield();
}
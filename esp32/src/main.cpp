// ============================================================================
// COMPLETE FIXED main.cpp - ALL TIMING ISSUES RESOLVED
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
unsigned long behaviorStartTime = 0;  // CRITICAL: Renamed for clarity
unsigned long lastInteractionTime = 0;
unsigned long lastIdleCheckTime = 0;
bool inSleepMode = false;
bool inDarkSleepMode = false;

const unsigned long IDLE_TO_SLEEPY_DELAY = 20000;

// --- BEHAVIOR CONTROLLER ---
void startBehavior(const char* name) {
  const Behavior* b = findBehavior(name);
  if (!b) return;

  Serial.printf("\n[BEHAVIOR] ===== STARTING: %s =====\n", name);

  // Reset interaction timer for non-sleep behaviors
  if (strcmp(name, "sleeping") != 0 && strcmp(name, "sleepy_idle") != 0) {
    inSleepMode = false;
    inDarkSleepMode = false;
    lastInteractionTime = millis();
    Serial.println("[BEHAVIOR] Reset interaction timer");
  }

  // Protect active states from sleepy override
  if (strcmp(name, "sleepy_idle") == 0) {
    if (activeBehavior && (strcmp(activeBehavior->name, "happy") == 0 ||
                           strcmp(activeBehavior->name, "surprised") == 0 ||
                           strcmp(activeBehavior->name, "listening") == 0)) {
      Serial.println("[BEHAVIOR] Blocked sleepy (active emotion)");
      return; 
    }
    inSleepMode = true;
  }
  else if (strcmp(name, "sleeping") == 0) {
    inDarkSleepMode = true;
  }

  activeBehavior = b;
  behaviorStartTime = millis();  // CRITICAL: Record start time
  
  // Calculate total duration
  unsigned long totalDuration = b->entryTime + b->holdTime + b->exitTime;
  Serial.printf("[BEHAVIOR] Timing: entry=%dms hold=%dms exit=%dms TOTAL=%dms\n",
                b->entryTime, b->holdTime, b->exitTime, totalDuration);
  
  // 1. SET EYE TARGET
  Serial.println("[BEHAVIOR] Setting eye target...");
  eye.setTarget(b);
  
  // 2. SET LED MOOD
  Serial.print("[BEHAVIOR] Setting LED mood: ");
  if (b->ledEffect) {
    Serial.println(b->ledEffect);
    leds.setMood(b->ledEffect);
  } else {
    Serial.println(name);
    leds.setMood(name);
  }
  
  // 3. SERVO & SOUND ACTIONS
  if (strcmp(name, "happy") == 0) {
    Serial.println("[BEHAVIOR] Servo: NOD gesture");
    servo.triggerGesture("nod");
    soundFx.play("happy");
  } 
  else if (strcmp(name, "shy_happy") == 0) {
    Serial.println("[BEHAVIOR] Servo: NOD gesture (shy)");
    servo.triggerGesture("nod");
    soundFx.play("happy");
  }
  else if (strcmp(name, "sad") == 0) {
    Serial.println("[BEHAVIOR] Servo: DROOP (120°)");
    servo.setTarget(120); 
    soundFx.play("sad");
  }
  else if (strcmp(name, "surprised") == 0 || strcmp(name, "startled") == 0) {
    Serial.println("[BEHAVIOR] Servo: ALERT (80°)");
    servo.setTarget(80); 
    soundFx.play("surprised");
  }
  else if (strcmp(name, "curious_idle") == 0) {
    Serial.println("[BEHAVIOR] Servo: TILT gesture");
    servo.triggerGesture("tilt");
    soundFx.play("curious");
  }
  else if (strcmp(name, "sleepy_idle") == 0 || strcmp(name, "sleeping") == 0) {
    Serial.println("[BEHAVIOR] Servo: SLEEPY (110°)");
    servo.setTarget(110);
    if (strcmp(name, "sleepy_idle") == 0) {
      soundFx.play("sleep");
    }
  }
  else if (strcmp(name, "listening") == 0) {
    Serial.println("[BEHAVIOR] Servo: CENTER (90°)");
    servo.setTarget(90);
  }
  else if (strcmp(name, "calm_idle") == 0) {
    Serial.println("[BEHAVIOR] Servo: CENTER (90°)");
    servo.setTarget(90);
  }
  else if (strcmp(name, "confused") == 0) {
    Serial.println("[BEHAVIOR] Servo: TILT (110°)");
    servo.setTarget(110);
  }
  else if (strcmp(name, "thinking") == 0) {
    Serial.println("[BEHAVIOR] Servo: TILT (100°)");
    servo.setTarget(100);
  }
  else if (strcmp(name, "playful_mischief") == 0) {
    Serial.println("[BEHAVIOR] Servo: SHAKE gesture");
    servo.triggerGesture("shake");
  }

  if (robotWs.isConnected()) {
    robotWs.sendStatus("sync_behavior", name);
  }
  
  Serial.printf("[BEHAVIOR] ===== STARTED: %s =====\n\n", name);
}

void handleMessage(const char* type, JsonDocument& doc) {
  if (strcmp(type, "set_behavior") == 0) {
    startBehavior(doc["name"]);
  }
  else if (strcmp(type, "servo_action") == 0) {
    servo.setTarget(doc["angle"]);
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
  Serial.println("  EMO DESKBOT - TIMING FIX VERSION");
  Serial.println("========================================\n");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  
  Serial.println("[INIT] Display...");
  display.begin();
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(10, 30, "DeskBot Init...");
  display.sendBuffer();
  
  Serial.println("[INIT] LEDs...");
  leds.begin();
  
  Serial.println("[INIT] Servo...");
  servo.begin();
  
  Serial.println("[INIT] Sensors...");
  sensors.begin();
  
  Serial.println("[INIT] Sound...");
  soundFx.play("startup");

  Serial.println("[INIT] WiFi...");
  wifiMgr.begin();
  if (wifiMgr.autoConnect()) {
      Serial.println("[INIT] Audio...");
      audioMgr.begin();
      robotWs.setServer(wifiMgr.getServerIP().c_str(), wifiMgr.getServerPort());
      robotWs.setCallback(handleMessage);
      robotWs.begin();
  }
  
  Serial.println("[INIT] Eyes...");
  eye.startBootSequence();
  
  Serial.println("[INIT] Starting calm_idle...");
  startBehavior("calm_idle");
  lastInteractionTime = millis();
  lastIdleCheckTime = millis();
  
  Serial.println("\n========================================");
  Serial.println("  INITIALIZATION COMPLETE!");
  Serial.println("========================================\n");
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
    
    static bool initialStateSent = false;
    static unsigned long connectionTime = 0;
    if (robotWs.isConnected() && activeBehavior && !initialStateSent) {
      if (connectionTime == 0) connectionTime = now;
      if (now - connectionTime > 500) {
        robotWs.sendStatus("sync_behavior", activeBehavior->name);
        initialStateSent = true;
      }
    }
    if (!robotWs.isConnected()) {
      initialStateSent = false;
      connectionTime = 0;
    }
  } else {
    wifiMgr.handlePortal();
  }

  // 2. Component Updates - MUST UPDATE EVERY FRAME
  eye.update(dt);
  eye.render();
  leds.loop(dt);
  servo.loop(dt);
  soundFx.update();

  // 3. Sensor Logic
  static unsigned long lastSensor = 0;
  static int sustainedSoundCounter = 0;
  static bool wasMotionDetected = false;
  static bool wasTouchDetected = false;
  static uint16_t lastDistance = 0;
  static int darknessCounter = 0;
  static int lightCounter = 0;

  if (now - lastSensor > 100) {
    lastSensor = now;
    SensorData d = sensors.read();
    bool activityDetected = false;
    
    // Microphone protection during servo movement only
    bool servoIsMoving = servo.isMoving();
    
    // 1. TOUCH - Always highest priority
    if (d.touchHead && !wasTouchDetected) {
      wasTouchDetected = true;
      Serial.println("\n[TOUCH] HEAD TOUCHED!");
      startBehavior("happy"); 
      activityDetected = true;
    } 
    else if (d.touchSide && !wasTouchDetected) {
      wasTouchDetected = true;
      Serial.println("\n[TOUCH] SIDE TOUCHED!");
      startBehavior("shy_happy");
      activityDetected = true;
    } 
    else if (!d.touchHead && !d.touchSide) {
      wasTouchDetected = false;
    }

    // 2. MOTION - Always responsive
    if (d.motion && !wasMotionDetected) {
      wasMotionDetected = true;
      Serial.println("\n[MOTION] DETECTED!");
      startBehavior("surprised");
      activityDetected = true;
    } else if (!d.motion) {
      wasMotionDetected = false;
    }
    
    // 3. DISTANCE - Always responsive
    if (d.distance_mm > 0 && d.distance_mm < 150) {
      if (lastDistance >= 150 || lastDistance == 0) {
        Serial.printf("\n[DISTANCE] Close: %dmm\n", d.distance_mm);
        startBehavior("surprised");
        activityDetected = true;
      }
    } 
    else if (d.distance_mm > 150 && d.distance_mm < 400) {
      if (lastDistance >= 400 || lastDistance < 150) {
        Serial.printf("\n[DISTANCE] Medium: %dmm\n", d.distance_mm);
        startBehavior("curious_idle");
        activityDetected = true;
      }
    }
    lastDistance = d.distance_mm;

    // 4. MICROPHONE - Only when servo not moving
    #if ENABLE_MICROPHONE
    int vol = micMgr.getLoudness();
    
    if (!servoIsMoving) {
      if (vol > 40) {
        Serial.printf("\n[MIC] Loud sound: %d\n", vol);
        startBehavior("surprised");
        activityDetected = true;
      } 
      else if (vol > 15) {
        sustainedSoundCounter++;
        if (sustainedSoundCounter > 10) {
          Serial.printf("\n[MIC] Sustained: %d\n", vol);
          startBehavior("listening");
          activityDetected = true;
        }
      } else {
        if (sustainedSoundCounter > 0) sustainedSoundCounter--;
      }
      
      if (vol > 10) leds.voiceReact(vol);
    }
    #endif

    // 5. Reset timers on activity
    if (activityDetected) {
      lastInteractionTime = now;
      darknessCounter = 0;
      lightCounter = 0;
      inSleepMode = false;
      inDarkSleepMode = false;
    }

    // 6. DARKNESS LOGIC
    if (now - lastInteractionTime > 15000) { 
      if (d.light > 3000) { 
        darknessCounter++;
        lightCounter = 0;
        
        if (darknessCounter > 30 && !inDarkSleepMode) { 
          Serial.println("\n[DARK] Going to sleep...");
          startBehavior("sleeping");
        }
      } 
      else {
        lightCounter++;
        if (lightCounter > 10) { 
          darknessCounter = 0;
          if (inDarkSleepMode) {
            Serial.println("\n[LIGHT] Waking up!");
            startBehavior("calm_idle");
          }
        }
      }
    } else {
      darknessCounter = 0;
      lightCounter = 0;
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
        Serial.println("\n[IDLE] 20s timeout -> Sleepy");
        startBehavior("sleepy_idle");
      }
    }
  }
  
  // 5. CRITICAL FIX: Auto-return from timed behaviors
  // ONLY check this if behavior has a holdTime > 0
  if (activeBehavior && activeBehavior->holdTime > 0) {
    unsigned long elapsed = now - behaviorStartTime;
    unsigned long totalDuration = activeBehavior->entryTime + 
                                   activeBehavior->holdTime + 
                                   activeBehavior->exitTime;
    
    // Debug every second during behavior
    static unsigned long lastBehaviorDebug = 0;
    if (now - lastBehaviorDebug > 1000) {
      Serial.printf("[BEHAVIOR TIMING] %s: elapsed=%lums / total=%lums (%.1f%%)\n",
                    activeBehavior->name, elapsed, totalDuration, 
                    (float)elapsed / totalDuration * 100.0f);
      lastBehaviorDebug = now;
    }
    
    if (elapsed > totalDuration) {
      Serial.printf("\n[AUTO-RETURN] %s completed (ran for %lums) -> calm_idle\n", 
                    activeBehavior->name, elapsed);
      startBehavior("calm_idle");
    }
  }
  
  yield();
}
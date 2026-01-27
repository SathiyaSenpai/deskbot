# Eye System Issues Analysis

## CRITICAL ISSUES

### 1. **Eye Always in Sleepy State**

**Location:** `esp32/src/main.cpp`

**Root Causes:**
- **Issue 1.1:** `activeBehavior` is initialized to `nullptr` and never set to `calm_idle` on boot (line 106)
- **Issue 1.2:** `startBootSequence()` only initializes eye geometry but doesn't set a behavior (line 191)
- **Issue 1.3:** `lastInteractionTime` is initialized to `0` (line 197), so on first loop iteration, `now - lastInteractionTime` will be a large number (>20000ms), immediately triggering `sleepy_idle` (line 207-208)
- **Issue 1.4:** No initial call to `startBehavior("calm_idle")` after boot sequence

**Code References:**
- Line 106: `const Behavior* activeBehavior = nullptr;`
- Line 191: `eye.startBootSequence();` (no behavior set)
- Line 197: `unsigned long lastInteractionTime = 0;`
- Line 207: `if (now - lastInteractionTime > IDLE_TIMEOUT)` - This will trigger immediately on boot

---

### 2. **No Idle Animation on Boot**

**Location:** `esp32/src/main.cpp` and `esp32/src/eye_engine.h`

**Root Causes:**
- **Issue 2.1:** `startBootSequence()` in `eye_engine.h` (line 14-39) only initializes eye geometry but doesn't set a behavior target
- **Issue 2.2:** No call to `startBehavior("calm_idle")` after boot in `setup()` function
- **Issue 2.3:** Eye engine's `setTarget()` is never called with `calm_idle` behavior on initialization

**Code References:**
- `esp32/src/main.cpp` line 191: `eye.startBootSequence();` - Only initializes geometry
- `esp32/src/eye_engine.h` line 14-39: `startBootSequence()` - Doesn't set behavior
- Missing: `startBehavior("calm_idle");` after line 191

---

### 3. **Connection/Disconnection Loop**

**Location:** `esp32/src/websocket_client.h` and `esp32/src/main.cpp`

**Root Causes:**
- **Issue 3.1:** WebSocket client has aggressive reconnect interval (5 seconds) at line 66
- **Issue 3.2:** Heartbeat enabled with 15s interval, 3s timeout, 2 retries (line 67) - if server doesn't respond, it disconnects
- **Issue 3.3:** No connection state validation before sending messages
- **Issue 3.4:** `begin()` is called immediately after WiFi connection without checking if server is actually reachable
- **Issue 3.5:** The websocket library's auto-reconnect might be conflicting with manual connection attempts

**Code References:**
- `esp32/src/websocket_client.h` line 66: `ws.setReconnectInterval(5000);`
- `esp32/src/websocket_client.h` line 67: `ws.enableHeartbeat(15000, 3000, 2);`
- `esp32/src/main.cpp` line 186-188: WebSocket begins immediately after WiFi connect

---

### 4. **Null Pointer Dereference Crashes**

**Location:** `esp32/src/main.cpp`

**Root Causes:**
- **Issue 4.1:** Line 314: `if (strcmp(activeBehavior->name, "surprised") != 0)` - Will crash if `activeBehavior` is `nullptr`
- **Issue 4.2:** Line 324: `if (strcmp(activeBehavior->name, "listening") != 0)` - Will crash if `activeBehavior` is `nullptr`
- **Issue 4.3:** Line 366: `if (strcmp(activeBehavior->name, "sleepy_idle") != 0)` - Will crash if `activeBehavior` is `nullptr`
- **Issue 4.4:** Line 360: `if (activeBehavior && strcmp(activeBehavior->name, "calm_idle") != 0)` - This one is safe, but inconsistent with others

**Code References:**
- Line 314: Missing null check before accessing `activeBehavior->name`
- Line 324: Missing null check before accessing `activeBehavior->name`
- Line 366: Missing null check before accessing `activeBehavior->name`

---

### 5. **Conflicting Idle Timeout Mechanisms**

**Location:** `esp32/src/main.cpp`

**Root Causes:**
- **Issue 5.1:** Two separate idle timeout systems:
  - Lines 206-210: 20 second timeout using `lastInteractionTime`
  - Lines 365-369: 30 second timeout using `lastActivity`
- **Issue 5.2:** `lastActivity` is initialized to `millis()` at line 359, meaning it starts at boot time, not when activity happens
- **Issue 5.3:** Both systems can trigger simultaneously, causing behavior switching conflicts
- **Issue 5.4:** `lastInteractionTime` starts at 0, so first check will always trigger

**Code References:**
- Line 194: `const unsigned long IDLE_TIMEOUT = 20000;`
- Line 197: `unsigned long lastInteractionTime = 0;` - Starts at 0!
- Line 206-210: First idle timeout (20s)
- Line 359: `static unsigned long lastActivity = millis();` - Initialized to boot time
- Line 365-369: Second idle timeout (30s)

---

### 6. **Eye Engine Behavior Mapping Issues**

**Location:** `esp32/src/eye_engine.h`

**Root Causes:**
- **Issue 6.1:** When `setTarget()` is called with `calm_idle`, it sets `activeEffect_ = EFFECT_NONE` (line 103), but the eye dimensions might not match the expected idle state
- **Issue 6.2:** The `sleepy_idle` behavior sets `topLid_ = 0.4f` (line 79), but this lid value persists even after switching behaviors if not explicitly reset
- **Issue 6.3:** In `update()` function (line 128), when `EFFECT_ZZZ` is active, `blinkFactor_` is set to `1.0f` which keeps eyes open, but the lids are still applied, making it look sleepy

**Code References:**
- `esp32/src/eye_engine.h` line 76-81: `sleepy_idle` sets `topLid_ = 0.4f`
- `esp32/src/eye_engine.h` line 98-104: `calm_idle` resets lids to 0.0f
- `esp32/src/eye_engine.h` line 128: `blinkFactor_ = 1.0f;` when sleepy - but lids still applied

---

### 7. **Sensor Logic Issues**

**Location:** `esp32/src/main.cpp`

**Root Causes:**
- **Issue 7.1:** Line 285: Darkness check uses `if (d.light > 3000)` - This is backwards! High light values mean bright, not dark. Should be `< 3000` or similar low value
- **Issue 7.2:** `sensors.isTriggered()` at line 213 might not properly detect all interaction types
- **Issue 7.3:** Multiple sensor triggers can cause rapid behavior switching

**Code References:**
- Line 285: `if (d.light > 3000)` - Logic is inverted (high light = bright, not dark)

---

### 8. **WebSocket Connection State Issues**

**Location:** `esp32/src/websocket_client.h` and `esp32/src/main.cpp`

**Root Causes:**
- **Issue 8.1:** `begin()` is called in `setup()` (line 188) but connection might fail silently
- **Issue 8.2:** No retry logic with backoff - just constant 5-second reconnects
- **Issue 8.3:** Heartbeat might be too aggressive for unstable connections
- **Issue 8.4:** No connection timeout - will keep trying forever

**Code References:**
- `esp32/src/main.cpp` line 188: `robotWs.begin();` - No error handling
- `esp32/src/websocket_client.h` line 66: `ws.setReconnectInterval(5000);` - No exponential backoff

---

## SUMMARY BY FILE

### `esp32/src/main.cpp`
1. **Line 106:** `activeBehavior` never initialized to `calm_idle`
2. **Line 191:** Missing `startBehavior("calm_idle")` after boot
3. **Line 197:** `lastInteractionTime = 0` causes immediate sleepy trigger
4. **Line 207:** Idle check triggers immediately due to uninitialized time
5. **Line 314:** Null pointer dereference risk
6. **Line 324:** Null pointer dereference risk
7. **Line 285:** Inverted light sensor logic (darkness check)
8. **Line 359:** `lastActivity` initialized to boot time, not activity time
9. **Line 365:** Null pointer dereference risk
10. **Line 206-210 & 365-369:** Conflicting idle timeout mechanisms

### `esp32/src/eye_engine.h`
1. **Line 14-39:** `startBootSequence()` doesn't set behavior
2. **Line 128:** `blinkFactor_ = 1.0f` when sleepy but lids still applied
3. **Line 76-81:** `sleepy_idle` lid values might persist

### `esp32/src/websocket_client.h`
1. **Line 66:** Aggressive 5-second reconnect interval
2. **Line 67:** Heartbeat might be too strict
3. **Line 56-68:** No connection validation or timeout

---

## PRIORITY FIXES

### CRITICAL (Causes crashes or immediate failures):
1. Fix null pointer checks (lines 314, 324, 366)
2. Initialize `activeBehavior` to `calm_idle` on boot
3. Fix `lastInteractionTime` initialization
4. Fix inverted light sensor logic

### HIGH (Causes incorrect behavior):
5. Add `startBehavior("calm_idle")` after boot
6. Consolidate idle timeout mechanisms
7. Fix `lastActivity` initialization

### MEDIUM (Causes connection issues):
8. Improve WebSocket reconnection logic
9. Add connection validation
10. Adjust heartbeat settings

### LOW (Polish):
11. Fix eye engine lid persistence
12. Improve sensor interaction detection

# EMO Deskbot: Tuning Emotions & Adding Cloud (Beginner Friendly)

## Quick recap of the latest fixes (read this first)
- WebSocket message contract aligned end-to-end:
   - Controller/UI -> Server: `behavior` or `trigger_behavior`, `led`/`set_led`, `servo`/`set_servo`, `audio`, `ping`.
   - Server -> Robot: `set_behavior`, `set_led`, `set_servo`, `play_audio`, `ping`.
   - Robot -> Server: `hello`, `robot_status`, `sensor_data`, `pong`.
   - Server -> UI: `behavior_change`, `led_state` (hex or `off`), `sensor_update`, `event` (with `details`).
- Sensor payload normalized for UI:
   - light → number
   - motion → boolean
   - sound → number
   - touch → "touched" | "none"
   - distance → centimeters (numeric, null/"—" when unknown)
- Behavior names mapped to firmware set (e.g., sad→soft_sad, sleepy→sleepy_idle, confused→listening_confused, excited→playful_mischief, surprised/angry→startled).
- Ping/pong works: server pings; robot replies with `pong`.
- LED broadcasts send hex color or `off`; UI highlights buttons correctly.
- Audio: server sends `play_audio` (URL); robot streams via HTTP.

## Build & run without mistakes
1) Flash robot firmware (from esp32/):
    - `pio run -t upload`
    - Monitor (optional): `pio device monitor --baud 115200`
2) Start server (from server/):
    - `npm install` (first time)
    - `node server.js`
    - Robot should connect to `ws://<server-ip>:3000/ws?type=robot&token=emo_secret_2024`
3) Open Web UI:
    - Serve `web ui/` (the server already serves it at http://localhost:3000)
    - Controller WS URL: `ws://<server-ip>:3000/ws?type=controller&token=emo_secret_2024`
4) WiFi/Portal (robot): touch head pad at boot to enter portal; defaults in config.h (hotspot SSID/pass).

## Message shapes (for debugging)
- To robot:
   - `{"type":"set_behavior","name":"happy"}`
   - `{ "type":"set_led", "r":255, "g":0, "b":0 }` (or controller can send hex to server)
   - `{ "type":"set_servo", "angle":90 }`
   - `{ "type":"play_audio", "url":"http://<server>/audio/tts.wav" }`
   - `{ "type":"ping" }`
- From robot:
   - `{ "type":"robot_status", "state":"idle", "behavior":"calm_idle" }`
   - `{ "type":"sensor_data", "light":1234, "motion":false, "distance_mm":450, "touch_head":false, "touch_side":false, "sound":12 }`
   - `{ "type":"pong" }`
- From server to UI:
   - `{ "type":"behavior_change", "behavior":"happy" }`
   - `{ "type":"led_state", "color":"#ff0000" }` or `"off"`
   - `{ "type":"sensor_update", "sensors": { "light":1234, "motion":false, "sound":12, "touch":"none", "distance":45.0 } }`
   - `{ "type":"event", "event":"motion", "details":{...} }`


## Part 1: How to Change Emotions/Timings (Firmware)

Files to edit:
- src/behaviors.h — expression values (openness, scaleX, bottomLid, offsetX/Y, effect)
- src/eye_engine.h — rendering, smoothing factor
- src/main.cpp — loop timing (FRAME_INTERVAL, SENSOR_INTERVAL, PING_INTERVAL)

Steps:
1) Change an expression shape
   - Open src/behaviors.h
   - Each behavior has: openness, scaleX, bottomLid, offsetX (±48), offsetY (±24)
   - Example: make happy wider
     - Find "happy" entry and raise openness (e.g., 1.1) or bottomLid (e.g., 0.85)
2) Change eye smoothing speed
   - Open src/eye_engine.h
   - In update(), smoothing = 12. Higher = faster response, lower = smoother/laggier.
3) Change frame rate
   - Open src/main.cpp
   - FRAME_INTERVAL default 33 ms (~30 fps). Lower for smoother (uses more CPU), higher for lower power.
4) Change sensor/report rates
   - In src/main.cpp: SENSOR_INTERVAL (default 500 ms), PING_INTERVAL (default 5000 ms).
5) Default behavior on boot
   - In setup(), eye.setBehavior("calm_idle"); change to any name from behaviors.h.
6) Add a new behavior
   - Add an entry in src/behaviors.h (name must be unique) with offsets and lids.
   - Send it via WebSocket command: {"type":"set_behavior","name":"your_name"}.
7) Tune LED/servo/audio response
   - LED: src/led_controller.h (brightness, colors)
   - Servo: src/servo_controller.h (default angle)
   - Audio: src/audio_manager.h (tone sequences)


## Part 2: Set Up a Future WebSocket Server (free/low-cost)

Goal: control from phone and robot over the internet using a cheap VPS (DigitalOcean student credit).

Simple plan (Node.js):
1) Create a small VPS (1–2 GB RAM) and point a domain/subdomain to its IP (A record).
2) Install Node 20 and Caddy (for HTTPS/WSS).
3) Make a folder `server/` on the VPS with two files:
   - server.js (WebSocket + REST)
   - package.json
4) Example server.js (minimal):
```js
import { createServer } from 'http';
import express from 'express';
import { WebSocketServer } from 'ws';

const app = express();
app.use(express.json());

// Simple auth token
const TOKEN = process.env.ROBOT_TOKEN || 'changeme';

app.post('/api/cmd', (req, res) => {
  const { token, cmd } = req.body;
  if (token !== TOKEN) return res.status(401).send('bad token');
  // broadcast to robot clients
  wss.clients.forEach(c => c.readyState === 1 && c.send(JSON.stringify(cmd)));
  res.send('ok');
});

const server = createServer(app);
const wss = new WebSocketServer({ server, path: '/ws' });

wss.on('connection', ws => {
  ws.on('message', msg => {
    console.log('rx', msg.toString());
  });
});

server.listen(8080, () => console.log('HTTP+WS on :8080'));
```
5) package.json:
```json
{
  "type": "module",
  "dependencies": {
    "express": "^4.19.2",
    "ws": "^8.16.0"
  }
}
```
6) Run: `npm install`, then `node server.js`.
7) Set up Caddy for TLS (free Let’s Encrypt):
```
YOURDOMAIN.com {
  reverse_proxy localhost:8080
}
```
8) In firmware config.h, set WS_HOST to your domain, WS_PORT 443, WS_PATH "/ws". Use WSS automatically via Caddy.
9) Mobile control: build a tiny web UI hosted from the same server that opens WSS to `wss://YOURDOMAIN.com/ws` and sends commands.

### What is Caddy? (plain words)
- It is a tiny web server that auto-gets free HTTPS certificates (Let’s Encrypt) and reverse-proxies to your Node app. No manual TLS.
- One short config file; no "tough language"—just point it at your Node port.

Steps to install and use Caddy on Ubuntu:
1) `sudo apt update && sudo apt install -y debian-keyring debian-archive-keyring apt-transport-https`
2) `curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/gpg.key' | sudo gpg --dearmor -o /usr/share/keyrings/caddy-stable-archive-keyring.gpg`
3) `curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt' | sudo tee /etc/apt/sources.list.d/caddy-stable.list`
4) `sudo apt update && sudo apt install -y caddy`
5) Edit /etc/caddy/Caddyfile to:
```
YOURDOMAIN.com {
   reverse_proxy localhost:8080
}
```
6) `sudo systemctl restart caddy`
- Now HTTPS and WSS are on port 443; your Node app stays on 8080. If you change the Node port, update reverse_proxy.


## Part 3: Add TTS and LLM APIs

Architecture choice:
- Keep robot thin: send sensor/events to server; server does LLM + TTS; server returns small commands/audio URLs.
- This avoids heavy compute on ESP32.

Server flow:
1) Mobile/robot sends text or audio to server.
2) Server runs STT (if audio) -> gets text.
3) Server calls LLM -> gets reply text.
4) Server calls TTS -> returns an audio URL or PCM to robot.
5) Server also sends a `set_behavior` to match the emotion.

Minimal server additions:
- Add `/api/tts` route that calls chosen TTS, stores audio (or streams), responds with URL.
- Add `/api/llm` route that calls your LLM provider and returns text.
- Push behavior via WebSocket after LLM response.


## Part 4: TTS Options and Tanglish Considerations

### Cloud TTS
- Google (Gemini TTS / Text-to-Speech): very good multilingual, good for code-switched (Tanglish), mid cost.
- Azure Speech TTS: high quality, many voices, strong multilingual; good for Tanglish with Indian English voices; free tier available.
- OpenAI TTS: very natural English, limited Indian-accent voices; Tanglish is okay but sometimes anglicizes Tamil words.

### Offline / Self-hosted
- Piper (lightweight, CPU-friendly): small footprint, but accents limited; Tanglish support weaker.
- Coqui TTS: better quality, heavier than Piper; can be okay if you find an Indian English/Tamil-ish model.
- VITS custom models: need training; not beginner-friendly.

### Recommendation for Tanglish
- Best balance: **Azure Speech TTS** with Indian English voices (e.g., en-IN). Good at handling code-switched English/Tamil pronunciations.
- Runner-up: **Google TTS** if you prefer Google’s stack; also good with code-switching.
- Offline: only if you must avoid cloud; expect lower quality for Tanglish.


## Part 5: LLM Options
- Google Gemini Pro: strong reasoning, good for English; for code-switching it may anglicize Tamil words, but generally fine.
- OpenAI GPT-4/4.1: excellent quality, cost per token.
- DeepSeek-R1 / other cheaper models: lower cost, may struggle with mixed-language nuance.
- Self-host small models (e.g., Llama 3.1 8B) if you need offline, but quality will drop for Tanglish.


## Part 6: How to Wire the Robot to the Server

Firmware changes (later):
1) Edit src/config.h with your domain and token approach (you can hardcode a token and add it in headers inside websocket_client.h if desired).
2) Reflash.
3) Server: deploy Node app with HTTPS/WSS and token checks.
4) Mobile UI: host on same server; it calls REST `/api/cmd` with token, server broadcasts via WS.


## Part 7: If You Want a Quick Start Today (Local Only)
1) Run a local WS server on your laptop (ws://<laptop-ip>:8080/ws) using the sample server.js.
2) In config.h, set WS_HOST to your laptop IP, WS_PORT 8080, WS_PATH "/ws".
3) Flash ESP32, open Serial, and send commands from a simple web page using the same IP.


## FAQ
- “How do I slow down an expression?” Increase smoothing in eye_engine.h or increase FRAME_INTERVAL.
- “How do I make shy look higher?” Increase offsetY negative (e.g., -24) and maybe decrease openness.
- “Will USB phone charger work?” Yes, use a 2A supply if LEDs + servo together.

You can ask for a ready-made Node server scaffold next; I can add tokens, TTS proxy, and LLM call stubs for you.


## Part 8: Hardware Wiring (Breadboard First)

### A) How to read this text diagram
- Each line is FROM → TO. Keep all GNDs common. 5V/VCC is USB power; 3V3 is ESP32 3.3V pin.
- Signal pins go to the exact GPIO numbers in src/pins.h.

### B) Wiring map (ESP32 DevKit v1)
- OLED (I2C): VCC→3V3, GND→GND, SDA→GPIO21, SCL→GPIO22.
- MAX98357A (speaker): Vin→5V, GND→GND, BCLK→GPIO26, LRC→GPIO25, DIN→GPIO27; speaker to L+/L-.
- INMP441 mic (optional): VDD→3V3, GND→GND, SCK→GPIO14, WS→GPIO15, SD→GPIO32.
- WS2812 ring: 5V→5V, GND→GND, DIN→GPIO13. (If you have one, put a 330–470 Ω resistor inline on DIN and a 470–1000 µF cap across 5V/GND near the ring.)
- Servo: V+→5V, GND→GND, Signal→GPIO18. (If jitter, add ~100 µF cap across 5V/GND.) Use a 5V/2A supply.
- Ultrasonic HC-SR04: VCC→5V, GND→GND, TRIG→GPIO12, ECHO→GPIO35 (input-only, OK).
- PIR: VCC→5V, GND→GND, OUT→GPIO33.
- LDR divider: 3.3V→LDR→ADC34→10k→GND. Node (between LDR and 10k) to GPIO34.
- Touch pads: Pad→GPIO4 (T0), Pad→GPIO2 (T2). Keep leads short.
- Buzzer (optional): V+→5V, GND→GND, Signal→GPIO19 (short tones only).

### C) Build order on breadboard
1) Power rails: bring 5V and GND from ESP32 to rails; add 3.3V rail for OLED/mic.
2) OLED first (easy to verify): SDA/SCL to 21/22, power to 3V3/GND.
3) Ultrasonic + PIR: wire power, then TRIG/ECHO and PIR OUT.
4) LDR divider: add the 10k to GND, LDR to 3.3V, node to GPIO34.
5) Touch pads: short wires to GPIO4/2.
6) LED ring: GND, then 5V, then DIN to GPIO13 (add series resistor if you have one).
7) Servo: GND, then 5V, then signal to GPIO18. Check supply is 2A if servo + LEDs.
8) Audio out (MAX98357A): GND, 5V, then BCLK/LRC/DIN to 26/25/27; connect speaker.
9) (Optional) Mic: GND, 3V3, then SCK/WS/SD to 14/15/32.

### D) Why these choices
- ESP32 GPIOs are 3.3V; OLED/mic are 3.3V. WS2812 and servo need 5V power but accept 3.3V data.
- GPIO35 is input-only, perfect for HC-SR04 echo.
- Touch uses native capacitive pins T0 (GPIO4) and T2 (GPIO2).
- Pin map matches pins.h so firmware works without edits.

### E) Flashing firmware (PlatformIO)
1) Install VS Code + PlatformIO extension.
2) Open folder esp32/ (with platformio.ini inside).
3) Edit src/config.h: set WIFI_SSID/PASSWORD (or leave blank for offline), WS_HOST/PORT if using WS.
4) Plug ESP32 via USB. In VS Code, pick the correct serial port if needed.
5) Click PlatformIO: Build (hammer icon), then Upload (arrow), then Monitor (plug icon) at 115200 baud.

### F) First power-on checks
- Use a 5V/2A USB supply. ESP32 LED should light, OLED should show eyes after boot.
- If OLED blank: recheck SDA/SCL and 3V3/GND; try SH1106 driver in main.cpp if the model differs.
- If LEDs flicker when servo moves: bigger 5V supply and a cap near servo/LED ring.

### G) Moving to vero board
- Keep identical connections. Solder GND bus first, then 5V/3.3V, then signals.
- Keep touch wires short; keep mic away from speaker; keep servo power traces thicker.

### H) Reading diagrams (general tips)
- Find rails first (GND, 5V, 3.3V) and ensure every module shares ground.
- Trace each part: power pin to the right voltage, ground to common, signal to the exact GPIO.
- For sensors: their output goes into ESP32 GPIO; for actuators: ESP32 GPIO drives them.


## Part 9: Visual Wiring Diagram (Text Format)

### ESP32 DevKit v1 Pinout (Top View, USB at bottom)
```
         ┌───────────────────────┐
     3V3 │ ●                   ● │ VIN (5V input)
      EN │ ●                   ● │ GND
GPIO36/VP│ ●                   ● │ GPIO23
GPIO39/VN│ ●                   ● │ GPIO22 ← SCL (OLED)
GPIO34   │ ● ← LDR (ADC)       ● │ GPIO21 ← SDA (OLED)
GPIO35   │ ● ← Ultrasonic ECHO ● │ GND
GPIO32   │ ● ← Mic SD          ● │ GPIO19 ← Buzzer
GPIO33   │ ● ← PIR OUT         ● │ GPIO18 ← Servo Signal
GPIO25   │ ● ← Speaker LRC     ● │ GPIO5
GPIO26   │ ● ← Speaker BCLK    ● │ GPIO17
GPIO27   │ ● ← Speaker DIN     ● │ GPIO16
GPIO14   │ ● ← Mic SCK         ● │ GPIO4  ← Touch Head
GPIO12   │ ● ← Ultrasonic TRIG ● │ GPIO2  ← Touch Side
     GND │ ●                   ● │ GPIO15 ← Mic WS
GPIO13   │ ● ← LED Ring DIN    ● │ GND
         │      [USB PORT]       │
         └───────────────────────┘
```

### Breadboard Layout (Bird's Eye View)
```
┌─────────────────────────────────────────────────────────────────────────┐
│  POWER RAILS (top of breadboard)                                        │
│  ─────────────────────────────────────────────────────────────────────  │
│  [+5V]  ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●  [+5V]  │
│  [GND]  ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●  [GND]  │
│  [3.3V] ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●  [3.3V] │
│                                                                         │
│  MAIN AREA                                                              │
│  ─────────────────────────────────────────────────────────────────────  │
│                                                                         │
│     ┌─────────┐                                                         │
│     │  ESP32  │  ← Plug ESP32 here (straddles center gap)               │
│     │ DevKit  │                                                         │
│     │   v1    │                                                         │
│     └─────────┘                                                         │
│                                                                         │
│   ┌──────┐  ┌─────────┐  ┌───────────┐  ┌───────┐  ┌──────┐            │
│   │ OLED │  │MAX98357A│  │ HC-SR04   │  │  PIR  │  │Servo │            │
│   │      │  │(Speaker)│  │(Ultrasonic│  │       │  │      │            │
│   └──────┘  └─────────┘  └───────────┘  └───────┘  └──────┘            │
│                                                                         │
│   ┌─────────────────┐    ┌──────┐    ┌───────────────┐                 │
│   │  WS2812 LED     │    │ LDR  │    │ Touch Pads    │                 │
│   │  Ring (16 LEDs) │    │+10kΩ │    │ (foil/copper) │                 │
│   └─────────────────┘    └──────┘    └───────────────┘                 │
│                                                                         │
│  ─────────────────────────────────────────────────────────────────────  │
│  [GND]  ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●  [GND]  │
│  POWER RAILS (bottom of breadboard)                                     │
└─────────────────────────────────────────────────────────────────────────┘
```


## Part 10: Detailed Wiring for Each Component

### 1. OLED Display (1.3" I2C SSD1306/SH1106)
```
OLED Module        ESP32 Pin       Wire Color (suggestion)
───────────────────────────────────────────────────────────
GND      ────────→ GND             Black
VCC      ────────→ 3V3             Red
SDA      ────────→ GPIO21          Blue
SCL      ────────→ GPIO22          Yellow
```
**What is I2C?** A way for devices to talk using only 2 wires (SDA=data, SCL=clock).
**Why 3.3V?** OLED is a 3.3V device. Using 5V may damage it.

### 2. MAX98357A Speaker Amplifier + Speaker
```
MAX98357A          ESP32 Pin       Wire Color
───────────────────────────────────────────────────────────
GND      ────────→ GND             Black
Vin      ────────→ 5V              Red
BCLK     ────────→ GPIO26          Orange
LRC      ────────→ GPIO25          Yellow
DIN      ────────→ GPIO27          Green
SD       ────────→ (leave unconnected or 5V for always-on)

Speaker wires:
L+       ────────→ Speaker (+)     Red
L-       ────────→ Speaker (-)     Black
```
**What is I2S?** Audio protocol; uses 3 wires (BCLK=bit clock, LRC=left/right, DIN=data).
**Speaker:** Use any 4Ω–8Ω, 1W–3W speaker. Bigger = louder.

### 3. WS2812 RGB LED Ring (16 LEDs)
```
LED Ring           ESP32 Pin       Wire Color
───────────────────────────────────────────────────────────
GND      ────────→ GND             Black
5V       ────────→ 5V              Red
DIN      ────────→ GPIO13          Green (through 330Ω resistor)

Optional but recommended:
- Put 330Ω resistor between GPIO13 and DIN (prevents signal damage)
- Put 470µF capacitor across 5V and GND near the ring (smooths power)
```
**What is WS2812?** "Smart" LEDs; each pixel has its own chip. One data wire controls all.
**Why resistor?** Protects first LED from voltage spikes when ESP32 sends data.

### 4. SG90 Servo Motor
```
Servo              ESP32 Pin       Wire Color (standard)
───────────────────────────────────────────────────────────
GND (brown)  ────→ GND             Brown
5V (red)     ────→ 5V              Red
Signal (orange)──→ GPIO18          Orange

Optional:
- Put 100µF capacitor across 5V and GND near servo (stops jitter)
```
**What is PWM?** Pulses that tell the servo what angle to move to. ESP32 generates these.
**Power:** Servo draws ~500mA when moving. Use 2A USB supply.

### 5. HC-SR04 Ultrasonic Distance Sensor
```
HC-SR04            ESP32 Pin       Wire Color
───────────────────────────────────────────────────────────
GND      ────────→ GND             Black
VCC      ────────→ 5V              Red
TRIG     ────────→ GPIO12          Yellow
ECHO     ────────→ GPIO35          Green
```
**How it works:** ESP32 sends a pulse on TRIG; sensor sends ultrasound; ECHO goes HIGH for the time sound takes to bounce back.
**Why GPIO35?** It's input-only on ESP32, perfect for receiving the ECHO signal.

### 6. PIR Motion Sensor (HC-SR501)
```
PIR                ESP32 Pin       Wire Color
───────────────────────────────────────────────────────────
GND      ────────→ GND             Black
VCC      ────────→ 5V              Red
OUT      ────────→ GPIO33          Yellow
```
**How it works:** Detects infrared (body heat). OUT goes HIGH when motion detected.
**Warmup:** PIR needs 30–60 seconds after power-on to stabilize. Ignore false triggers during this time.

### 7. LDR (Light Sensor) with Voltage Divider
```
Circuit on breadboard:

    3.3V ───────┬─────────────────
                │
              [LDR]  (light-dependent resistor)
                │
                ├──────────────────→ GPIO34 (ADC input)
                │
            [10kΩ resistor]
                │
    GND ────────┴─────────────────

Physical wiring:
- LDR leg 1      → 3.3V rail
- LDR leg 2      → junction point
- 10kΩ leg 1     → same junction point
- 10kΩ leg 2     → GND rail
- Junction point → GPIO34
```
**What is a voltage divider?** Two resistors split voltage. As LDR resistance changes with light, GPIO34 sees different voltages.
**ADC:** Analog-to-Digital Converter reads voltage (0–3.3V) as a number (0–4095). Bright = higher number.

### 8. Capacitive Touch Pads
```
Touch Pad          ESP32 Pin       Notes
───────────────────────────────────────────────────────────
Head pad  ───────→ GPIO4 (T0)      Keep wire SHORT (<10cm)
Side pad  ───────→ GPIO2 (T2)      Keep wire SHORT (<10cm)
```
**How to make a touch pad:**
1. Cut a small piece of copper tape or aluminum foil (2cm × 2cm)
2. Solder a short wire to the foil (or use conductive glue)
3. Connect wire to GPIO4 or GPIO2
4. Glue foil inside robot shell where you want touch sensitivity

**How it works:** ESP32 has built-in capacitive sensing. When you touch the pad, capacitance changes, and ESP32 detects it.


## Part 11: Common Mistakes & How to Fix Them

| Problem | What You See | Cause | Fix |
|---------|-------------|-------|-----|
| OLED blank | No display at all | Wrong pins or voltage | Check SDA→21, SCL→22, use 3.3V not 5V |
| OLED shows garbage | Random pixels | Wrong driver | Change SSD1306 to SH1106 in main.cpp |
| Eyes flicker | Display stutters | Not enough power | Use 2A USB supply; add capacitor |
| LEDs don't light | Ring stays dark | No power or bad DIN | Check 5V/GND; add resistor on DIN |
| LEDs show wrong color | Colors are off | Wrong LED type setting | Check NEO_GRB in code matches your ring |
| Servo jitters | Twitches randomly | Power instability | Add 100µF cap near servo; use 2A supply |
| Servo doesn't move | Stays still | Wrong pin or no signal | Check GPIO18; ensure servo is 5V powered |
| Ultrasonic reads 0 | Always shows 0mm | TRIG/ECHO swapped | TRIG→12, ECHO→35; check connections |
| PIR always HIGH | Motion always detected | Still warming up | Wait 60 seconds after power-on |
| PIR never triggers | No motion detected | Sensitivity too low | Adjust potentiometer on PIR module |
| Touch doesn't work | No response to touch | Wire too long | Keep wire under 10cm; check GPIO4/2 |
| No sound | Speaker silent | Wrong I2S pins | Check BCLK→26, LRC→25, DIN→27 |
| Sound is distorted | Crackling audio | Bad power or speaker | Use 2A supply; try different speaker |
| WiFi won't connect | "WiFi failed" in Serial | Wrong credentials | Check SSID/PASSWORD in config.h |
| Upload fails | "Failed to connect" | Wrong COM port or mode | Hold BOOT button while clicking Upload |


## Part 12: Step-by-Step Build Checklist

Print this and check off as you go!

### Phase 1: Prepare (30 minutes)
- [ ] Gather all components (see Parts List below)
- [ ] Install VS Code from https://code.visualstudio.com
- [ ] Install PlatformIO extension in VS Code
- [ ] Download/clone the esp32/ folder to your computer
- [ ] Get a 5V/2A USB power supply (phone charger works)
- [ ] Get a breadboard and jumper wires

### Phase 2: Basic Test - OLED Only (15 minutes)
- [ ] Place ESP32 in center of breadboard (straddles the gap)
- [ ] Connect USB cable to computer
- [ ] Wire OLED: VCC→3V3, GND→GND, SDA→GPIO21, SCL→GPIO22
- [ ] Open esp32/ folder in VS Code
- [ ] Edit src/config.h (set WiFi or leave blank for offline test)
- [ ] Click PlatformIO → Upload
- [ ] **SUCCESS CHECK:** OLED shows animated eyes!

### Phase 3: Add LED Ring (10 minutes)
- [ ] Wire LED ring: 5V→5V, GND→GND, DIN→GPIO13
- [ ] (Optional) Add 330Ω resistor between GPIO13 and DIN
- [ ] Reset ESP32 (press EN button or replug USB)
- [ ] **SUCCESS CHECK:** LEDs light up cyan on boot

### Phase 4: Add Sensors (20 minutes)
- [ ] Wire HC-SR04: VCC→5V, GND→GND, TRIG→GPIO12, ECHO→GPIO35
- [ ] Wire PIR: VCC→5V, GND→GND, OUT→GPIO33
- [ ] Wire LDR divider: 3.3V→LDR→GPIO34→10kΩ→GND
- [ ] Wire touch pads: short wires to GPIO4 and GPIO2
- [ ] Click PlatformIO → Monitor (opens Serial at 115200)
- [ ] **SUCCESS CHECK:** Serial shows sensor values every 0.5s

### Phase 5: Add Actuators (15 minutes)
- [ ] Wire servo: GND→GND, 5V→5V, Signal→GPIO18
- [ ] (Optional) Add 100µF capacitor near servo across 5V/GND
- [ ] Reset ESP32
- [ ] **SUCCESS CHECK:** Servo moves to center position (90°)

### Phase 6: Add Audio (15 minutes)
- [ ] Wire MAX98357A: GND→GND, Vin→5V, BCLK→GPIO26, LRC→GPIO25, DIN→GPIO27
- [ ] Connect speaker to L+ and L-
- [ ] Reset ESP32
- [ ] **SUCCESS CHECK:** Hear chirp sound on boot!

### Phase 7: Full System Test (15 minutes)
- [ ] All components connected
- [ ] Using 5V/2A power supply
- [ ] Open Serial Monitor
- [ ] Move hand near ultrasonic → distance changes
- [ ] Walk past PIR → motion detected
- [ ] Cover/uncover LDR → light value changes
- [ ] Touch the touch pads → touch detected
- [ ] **SUCCESS CHECK:** All sensors working!

### Phase 8: WiFi & WebSocket (Optional, 15 minutes)
- [ ] Edit src/config.h with your WiFi SSID/PASSWORD
- [ ] Set WS_HOST to your laptop IP (or leave for offline)
- [ ] Upload again
- [ ] **SUCCESS CHECK:** Serial shows "WiFi OK, IP: xxx.xxx.xxx.xxx"


## Part 13: Parts List with Links (India)

| Component | Quantity | Approx Price (INR) | Where to Buy |
|-----------|----------|-------------------|--------------|
| ESP32 DevKit v1 | 1 | ₹350-500 | Amazon, Robocraze, Robu.in |
| 1.3" OLED I2C (SH1106/SSD1306) | 1 | ₹250-350 | Amazon, Robocraze |
| MAX98357A I2S Amplifier | 1 | ₹150-250 | Amazon, Robocraze |
| Small Speaker (3W 4Ω) | 1 | ₹50-100 | Local electronics shop |
| WS2812 LED Ring (16 LEDs) | 1 | ₹200-350 | Amazon, Robocraze |
| SG90 Servo Motor | 1 | ₹100-150 | Amazon, Robocraze |
| HC-SR04 Ultrasonic Sensor | 1 | ₹50-80 | Amazon, Robocraze |
| HC-SR501 PIR Sensor | 1 | ₹50-80 | Amazon, Robocraze |
| LDR (pack of 5-10) | 1 pack | ₹20-50 | Local shop, Amazon |
| 10kΩ Resistors (pack) | 1 pack | ₹20-30 | Local shop |
| 330Ω Resistors (pack) | 1 pack | ₹20-30 | Local shop |
| Capacitors (100µF, 470µF) | 2-3 each | ₹10-20 | Local shop |
| Breadboard (830 points) | 1 | ₹100-150 | Amazon, Robocraze |
| Jumper Wires (M-M, M-F) | 1 set | ₹100-150 | Amazon, Robocraze |
| USB Cable (Type-C or Micro) | 1 | ₹50-100 | Anywhere |
| 5V 2A USB Adapter | 1 | ₹150-250 | Phone charger works! |
| Copper Tape (for touch pads) | 1 roll | ₹100-200 | Amazon, stationery shop |

**Total Estimate:** ₹1500-2500 depending on where you buy


## Part 14: Glossary (Plain English)

| Term | What It Really Means | Analogy |
|------|---------------------|---------|
| **GPIO** | General Purpose Input/Output - pins on ESP32 you connect stuff to | Like electrical outlets on a wall |
| **GND** | Ground - the "return path" for electricity; all devices share it | Like the drain in plumbing |
| **3V3** | 3.3 Volts - low voltage for sensitive chips (OLED, mic) | Like gentle water pressure |
| **5V** | 5 Volts - higher voltage for motors and LEDs | Like stronger water pressure |
| **I2C** | A protocol using 2 wires (SDA, SCL) for devices to communicate | Like a 2-lane road for data |
| **I2S** | A protocol for audio using 3 wires | Like a 3-lane highway for sound |
| **ADC** | Converts analog voltage (0-3.3V) to digital number (0-4095) | Like a thermometer turning temperature into a number |
| **PWM** | Pulses that control speed/brightness/position | Like dimming a light by flicking it on/off very fast |
| **Baud Rate** | Speed of serial communication (115200 = fast) | Like internet speed for the USB cable |
| **Capacitor** | Stores small amounts of electricity to smooth out power | Like a water tank that prevents pressure drops |
| **Resistor** | Limits current flow | Like a narrow pipe that slows water |
| **Voltage Divider** | Two resistors that split voltage | Like two pipes splitting water flow |
| **Breadboard** | Board with holes for prototyping; no soldering needed | Like LEGO for electronics |
| **Vero Board** | Board with copper strips for permanent soldering | Like gluing LEGO pieces together forever |


## Part 15: Quick Test Commands (Serial Monitor)

Open Serial Monitor in VS Code (Ctrl+Shift+P → "PlatformIO: Serial Monitor") at 115200 baud.

Type these commands to test (press Enter after each):

```
BEHAVIOR TESTS:
set_behavior:calm_idle        → Normal calm eyes
set_behavior:happy            → Happy smiling eyes
set_behavior:sleepy_idle      → Droopy sleepy eyes
set_behavior:curious_idle     → Wide curious eyes
set_behavior:shy_happy        → Shy looking away + hearts
set_behavior:startled         → Super wide shocked eyes
set_behavior:thinking         → Looking up-left, thinking
set_behavior:sad              → Sad droopy eyes

LED TESTS:
set_led:255,0,0               → Red LEDs
set_led:0,255,0               → Green LEDs
set_led:0,0,255               → Blue LEDs
set_led:255,255,0             → Yellow LEDs
set_led:0,255,255             → Cyan LEDs
set_led:255,0,255             → Magenta LEDs
set_led:255,255,255           → White LEDs
set_led:0,0,0                 → LEDs off

SERVO TESTS:
set_servo:0                   → Servo to 0°
set_servo:45                  → Servo to 45°
set_servo:90                  → Servo to 90° (center)
set_servo:135                 → Servo to 135°
set_servo:180                 → Servo to 180°

SOUND TESTS:
play_sound:chirp              → Happy chirp sound
play_sound:alert              → Alert beeps

SENSOR CHECK:
print_sensors                 → Show all sensor values
```

---

🎉 **Congratulations!** If you followed all these steps, you now have a working EMO-style deskbot!

**Next steps after breadboard works:**
1. Transfer to vero board (solder same connections)
2. Design/3D-print a robot shell
3. Set up WebSocket server for remote control
4. Add AI/TTS for conversations

**Need help?** Re-read the relevant section or check the troubleshooting table in Part 11.

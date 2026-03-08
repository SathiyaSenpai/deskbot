# JEEVA Tech Stack

## Overview

This document defines all software stack decisions for JEEVA, from beta (2025)
through production (2027), with rationale for each choice.

---

## Microcontroller Firmware

### ESP32 (Voice + Motor Control)

| Layer | Beta (2025) | Production (2027) | Rationale |
|-------|-------------|-------------------|-----------|
| **Framework** | PlatformIO + Arduino | **ESP-IDF C++** | Full FreeRTOS control, better timing |
| **Build system** | PlatformIO | ESP-IDF CMake | Standard for production |
| **OTA** | ArduinoOTA (simple) | ESP-IDF OTA + Ed25519 signing | Security requirement |
| **WiFi credentials** | ~~config.h (INSECURE)~~ → NVS | NVS encrypted partition | Never hardcode credentials |
| **Wake word** | ESP-SR WakeNet | ESP-SR WakeNet | Proven, runs on ESP32-S3 |

#### Why Migrate to ESP-IDF for Production?
- Full control over FreeRTOS task priorities (critical for audio + motor simultaneously)
- Secure boot and flash encryption are easier to configure
- Better memory management (eliminate Arduino overhead)
- Access to ESP-ADF (Audio Development Framework) for audio pipeline
- ESP-IDF is the official ESPRESSIF framework; Arduino is a wrapper around it

---

## Jetson Inference Stack

### LLM Inference

| Layer | Beta (2025) | Production (2027) | Speedup |
|-------|-------------|-------------------|---------|
| **Engine** | llama.cpp (CPU+GPU) | **TensorRT-LLM** | 2–3× |
| **Model format** | GGUF (Q4_K_M) | TensorRT engine | Quantized |
| **API** | llama.cpp HTTP server | FastAPI wrapper | Standardized |

#### llama.cpp → TensorRT-LLM Migration (Production)
```python
# Beta: llama.cpp server (simple, works now)
# curl http://localhost:8080/v1/chat/completions

# Production: TensorRT-LLM (2-3x faster on Jetson)
# Use NVIDIA trtllm-build to convert GGUF → TensorRT engine
# Command: trtllm-build --model sarvam-2b --precision fp16 --output-dir /opt/engines/
```

**When to migrate**: When beta is complete and response latency > 3 seconds (user expectation)

---

### Computer Vision

| Layer | Beta (2025) | Production (2027) | Notes |
|-------|-------------|-------------------|-------|
| **Framework** | OpenCV 4.x + CUDA | OpenCV 4.x + CUDA | Stable, no change needed |
| **Pipeline** | Python (PyCamera2) | Evaluate **DeepStream** | DeepStream: 3× throughput |
| **Face recognition** | InsightFace/ArcFace | InsightFace (TensorRT) | Same model, faster |
| **Object detection** | YOLOv8n (tiny) | YOLOv8n (TensorRT) | Fall detection, obstacle |

#### DeepStream Evaluation Criteria
Adopt DeepStream in production only if:
1. Multi-camera support needed (> 1 camera per robot)
2. Real-time object tracking required (fall detection upgrade)
3. Face recognition latency > 200ms on OpenCV pipeline

---

## Web API Server

### Replace Flask with FastAPI

| Aspect | Flask (current) | FastAPI (target) | Reason to change |
|--------|----------------|------------------|-----------------|
| **Async** | Sync (blocking) | **Async (native)** | Non-blocking I2C, audio |
| **Validation** | Manual / marshmallow | **Pydantic (automatic)** | Fewer bugs |
| **Auth** | Flask-JWT (addon) | **Built-in OAuth2** | Standard |
| **Docs** | None | **Auto Swagger UI** | Developer experience |
| **Performance** | WSGI | ASGI (Uvicorn) | Higher concurrency |
| **Type hints** | Optional | **Enforced** | Better maintainability |

#### FastAPI Migration Example
```python
# Old Flask
@app.route("/api/chat", methods=["POST"])
def chat():
    data = request.get_json()
    # No validation, manual error handling
    response = process_chat(data["message"])
    return jsonify({"response": response})

# New FastAPI
class ChatRequest(BaseModel):
    message: str
    language: str = "auto"
    resident_id: Optional[str] = None

@app.post("/api/chat")
async def chat(request: ChatRequest, user: dict = Depends(get_current_user)):
    response = await process_chat_async(request.message, request.language)
    return {"response": response}
# Automatic validation, auth, async, OpenAPI docs
```

---

## Database

### SQLite (Conversations + Profiles)

```sql
-- Conversations table
CREATE TABLE conversations (
    id INTEGER PRIMARY KEY,
    resident_id TEXT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    user_message TEXT NOT NULL,
    bot_response TEXT NOT NULL,
    language TEXT NOT NULL,
    emotion_detected TEXT
);

-- Health records table
CREATE TABLE health_records (
    id INTEGER PRIMARY KEY,
    resident_id TEXT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    spo2 REAL,
    heart_rate INTEGER,
    temperature REAL,
    humidity REAL
);

-- Residents table
CREATE TABLE residents (
    id TEXT PRIMARY KEY,  -- UUID
    name TEXT NOT NULL,
    preferred_language TEXT DEFAULT 'ta',
    room_number TEXT,
    caregiver_phone TEXT,
    face_embedding BLOB
);
```

### DuckDB for Health Analytics (Consider for Production)
```python
# DuckDB enables SQL analytics over health time-series
import duckdb
conn = duckdb.connect()
result = conn.execute("""
    SELECT 
        DATE_TRUNC('day', timestamp) as day,
        AVG(spo2) as avg_spo2,
        MIN(spo2) as min_spo2
    FROM health_records
    WHERE resident_id = ?
    GROUP BY day
    ORDER BY day DESC
    LIMIT 30
""", [resident_id]).fetchdf()
```

---

## Eye Rendering (Display)

| Layer | Beta (2025) | Production Evaluation | Notes |
|-------|-------------|----------------------|-------|
| **Engine** | **PyGame CE** (current) | Evaluate LVGL or Raylib | See below |
| **Target display** | SSD1306 OLED × 2 | Larger round display | |
| **Frame rate** | 30 FPS | 60 FPS target | |

#### PyGame CE vs Alternatives

| Engine | Language | GPU Accel | Embedded | License |
|--------|----------|-----------|----------|---------|
| **PyGame CE** | Python | Partial | Yes | LGPL |
| **LVGL** | C | Yes | **Yes (primary use)** | MIT |
| **Raylib** | C | Yes | Possible | zlib |

**Decision for production**: Evaluate LVGL if display is upgraded to color round LCD.
PyGame CE is sufficient for OLED-based eye animations in beta.

---

## Version Control Policy

> ⚠️ **JEEVA repository MUST remain PRIVATE.**

| Policy | Rule |
|--------|------|
| **Repository visibility** | **PRIVATE** — GitHub Private Repository only |
| **No public forks** | Disable forking in repository settings |
| **Contributor access** | Invite-only, minimum necessary permissions |
| **Secret scanning** | GitHub secret scanning enabled |
| **Branch protection** | Main branch: require PR review before merge |
| **Credentials** | NEVER commit `.env`, `*.pem`, `*.key` files |

All references to "open source" in previous documentation are corrected to:
**"Proprietary software with free OTA updates for registered units."**

---

## Development Environment Setup

### Jetson Development
```bash
# Install dependencies
sudo apt-get install -y python3-pip python3-venv libopencv-dev
pip3 install fastapi uvicorn pydantic python-dotenv sqlalchemy

# Install llama.cpp (beta LLM backend)
git clone https://github.com/ggerganov/llama.cpp
cd llama.cpp && cmake -B build -DLLAMA_CUDA=ON && cmake --build build -j4

# Install AI4Bharat IndicWhisper
pip3 install ai4bharat-indicwhisper
```

### ESP32 Development
```bash
# Install PlatformIO
pip3 install platformio

# Build and upload
cd esp32
pio run --target upload --environment esp32s3

# Monitor serial
pio device monitor --baud 115200
```

### Node.js Server (Legacy → FastAPI transition)
```bash
# Current: Node.js server
cd server
npm install
npm start

# Target: FastAPI
cd jeeva_api
pip3 install -r requirements.txt
uvicorn main:app --host 0.0.0.0 --port 3000 --reload
```

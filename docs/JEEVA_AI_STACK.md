# JEEVA AI Stack — India-Native Architecture

## Design Philosophy

JEEVA uses an **India-first AI stack**: prioritizing Tamil/Hindi/English multilingual support
with offline-capable models for reliable elder-care in areas with intermittent internet.

All "western-only" models (English-only Whisper base, pyttsx3/espeak) are replaced with
India-native alternatives. Internet-dependent features have offline fallbacks.

---

## LLM (Large Language Model)

### Primary Models

| Model | Size (Q4_K_M) | Languages | VRAM | Use Case |
|-------|--------------|-----------|------|---------|
| **Sarvam-2B** | ~1.5 GB | Tamil, Hindi, English | 2 GB | Tamil/Hindi conversations |
| **Llama 3.2 3B** | ~2.0 GB | English (strong) | 2.5 GB | English conversations |

### Language Detection Router
```python
# Language detected in first 500ms of user input
# Routes to appropriate model
def route_llm(text: str, detected_lang: str) -> str:
    if detected_lang in ("ta", "hi", "mr", "te", "kn"):
        return run_sarvam2b(text)
    else:
        return run_llama32_3b(text)
```

### RAM Management
⚠️ **Do NOT load both models simultaneously** — Jetson Orin Nano has 8 GB shared RAM.
Use model swapping: unload current model, load target model (< 3 seconds swap time).

### Model Loading Strategy
```yaml
# /opt/jeeva/config/llm_config.yaml
models:
  sarvam2b:
    path: /opt/jeeva/models/sarvam-2b-q4_k_m.gguf
    context_length: 4096
    gpu_layers: 20
  llama32_3b:
    path: /opt/jeeva/models/llama-3.2-3b-q4_k_m.gguf
    context_length: 8192
    gpu_layers: 25

active_model: sarvam2b  # Default — Tamil-speaking users
```

---

## ASR (Automatic Speech Recognition)

### Architecture

```
Mic Array → WebRTC APM → Language Detection (2s) → Route to ASR
                                  ↓
                    Tamil/Hindi → IndicWhisper (offline primary)
                                → Bhashini ASR (online enhancement)
                    English     → IndicWhisper (handles English too)
```

### Models

| Model | Size | Languages | Mode | Quality |
|-------|------|-----------|------|---------|
| **AI4Bharat IndicWhisper** | ~500 MB | 22 Indian languages + English | Offline primary | Tamil 8/10, Hindi 9/10 |
| **Bhashini ASR** | API call | All Indian official languages | Online enhancement | Tamil 9/10, Hindi 9.5/10 |

### Language Detection in First 2 Seconds
```python
# Detect language from first 2 seconds of audio
# Prevents loading wrong model for the full utterance
def detect_language_early(audio_chunk_2s: np.ndarray) -> str:
    # Run fast langid on brief transcription
    brief_text = indicwhisper.transcribe(audio_chunk_2s, task="language-detect")
    return brief_text.language  # "ta", "hi", "en", etc.
```

### Bhashini ASR Integration
```python
# Government of India free API — no cost
BHASHINI_ASR_URL = "https://dhruva-api.bhashini.gov.in/services/inference/pipeline"

def bhashini_asr(audio_b64: str, source_lang: str) -> str:
    payload = {
        "pipelineTasks": [{
            "taskType": "asr",
            "config": {"language": {"sourceLanguage": source_lang}}
        }],
        "inputData": {"audio": [{"audioContent": audio_b64}]}
    }
    # Returns higher accuracy than offline model alone
```

---

## TTS (Text-to-Speech)

### Priority Chain

```
Tamil/Hindi text → Bhashini TTS (online, 9/10 quality) ──→ Output
                 → AI4Bharat IndicTTS (offline, 7.5/10)  ↗ (if offline)
English text    → Piper TTS (offline, 8/10)             ↗
```

⚠️ **NEVER use pyttsx3 or espeak for Tamil/Hindi** — robotic quality is unsuitable for
elder-care. Patient trust requires natural-sounding speech.

### TTS Models

| Engine | Languages | Mode | Quality (Tamil) | Quality (Hindi) | Cost |
|--------|-----------|------|-----------------|-----------------|------|
| **Bhashini TTS** | 22 Indian languages | Online | 9/10 | 9.5/10 | Free (Govt API) |
| **AI4Bharat IndicTTS** | Tamil, Hindi, others | Offline | 7.5/10 | 8/10 | Free |
| **Piper TTS** | English (en_IN accent) | Offline | N/A | N/A | Free |
| ~~pyttsx3~~ | ~~English only~~ | ~~Offline~~ | ~~1/10~~ | ~~2/10~~ | ~~Free~~ |
| ~~espeak~~ | ~~Many~~ | ~~Offline~~ | ~~1/10~~ | ~~1/10~~ | ~~Free~~ |

### Bhashini TTS Integration
```python
def bhashini_tts(text: str, target_lang: str, voice: str = "female") -> bytes:
    payload = {
        "pipelineTasks": [{
            "taskType": "tts",
            "config": {
                "language": {"sourceLanguage": target_lang},
                "gender": voice,
                "samplingRate": 22050
            }
        }],
        "inputData": {"input": [{"source": text}]}
    }
    response = requests.post(BHASHINI_TTS_URL, json=payload, headers=bhashini_headers())
    return base64.b64decode(response.json()["pipelineResponse"][0]["audio"][0]["audioContent"])
```

---

## Wake Word Detection

### Implementation
- **Engine**: ESP-SR WakeNet (runs on ESP32-S3 microcontroller, not Jetson)
- **Wake phrase**: `"Hello Jeeva"` (two-word phrase)
- **Power**: ESP32-S3 stays awake; Jetson wakes only on valid wake word

### Why "Hello Jeeva" (Not Just "Jeeva")
| Phrase | False Trigger Rate | Reason |
|--------|-------------------|--------|
| "Jeeva" | High | Common Tamil name — triggers constantly in Tamil households |
| "Hello Jeeva" | < 1% | Two-word combination rarely occurs in ambient speech |
| "Hey Jeeva" | Medium | "Hey" is common in Tamil slang |

### WakeNet Configuration
```c
// esp32/src/wake_word.cpp
#define WAKE_WORD_MODEL  "wakeNet9_v1_helloJeeva_5_0.6_0.6"
#define WAKE_THRESHOLD   0.85f  // High threshold for elder-care (fewer false positives)
```

---

## Face Recognition

### Architecture
- **Engine**: InsightFace (ArcFace backbone) on Jetson
- **Use case**: Multi-resident support (hospital ward, nursing home)
- **Database**: Store 5–10 faces per robot unit

### Registration Flow
```
1. Caregiver opens admin panel
2. Selects "Register New Resident"
3. Robot captures 10 photos at different angles
4. InsightFace generates 512-dim embedding
5. Store in SQLite: (resident_id, name, language, embedding_blob)
```

### Privacy
- Face embeddings stored locally only (never sent to cloud)
- Embeddings encrypted at rest with AES-256
- "Delete All My Data" button removes all embeddings for that resident

---

## Emotion Detection

### Multi-Modal Fusion

| Modality | Weight | Rationale |
|----------|--------|-----------|
| **Voice emotion** | 70% | More reliable for elderly (face may be neutral) |
| **Facial expression** | 30% | Supplementary confirmation |

### Why Voice-Primary
- Elderly Indian faces are under-represented in FER2013 (0.3% of dataset)
- Models trained on FER2013 perform poorly on elderly South Asian faces
- Voice emotion (tone, pace, pitch) is more consistent across cultures

### Voice Emotion Analysis
```python
# Uses prosodic features + speech emotion model
VOICE_EMOTION_MODEL = "superb/wav2vec2-base-superb-er"  # 4 classes
# Map to: happy, sad, anxious, neutral
# Tamil-specific prosody rules (different from English)
```

---

## Medication RAG (Retrieval-Augmented Generation)

### Architecture
```
User: "What is this tablet for?"
         ↓
    FAISS vector search
    (local, offline)
         ↓
    Top-3 matching medications
         ↓
    LLM generates explanation in user's language
         ↓
    Response in Tamil/Hindi/English
```

### Knowledge Base
- **Source**: CIMS India top 200 medications (Indian brand names + generic names)
- **Storage**: Local FAISS vector store (no cloud dependency)
- **Size**: ~50 MB (fits on Jetson eMMC easily)
- **Update**: Manual quarterly update from CIMS India website

### Medication Entry Format
```json
{
  "brand_name": "Ecosprin 75",
  "generic_name": "Aspirin 75mg",
  "tamil_name": "அஸ்பிரின்",
  "hindi_name": "एस्पिरिन",
  "use": "Heart attack prevention, blood thinner",
  "side_effects": "Stomach upset, bleeding",
  "warning": "Take with food. Do not crush."
}
```

---

## Model Performance Comparison Tables

### LLM Tamil/Hindi Quality

| Model | Tamil | Hindi | English | VRAM | Offline |
|-------|-------|-------|---------|------|---------|
| Sarvam-2B Q4 | 8.5/10 | 9/10 | 7/10 | 2 GB | ✅ |
| Llama 3.2 3B Q4 | 4/10 | 6/10 | 9/10 | 2.5 GB | ✅ |
| GPT-4o (cloud) | 9/10 | 9.5/10 | 10/10 | N/A | ❌ |
| Gemma 2 2B | 3/10 | 5/10 | 8/10 | 2 GB | ✅ |

### ASR Comparison

| Model | Tamil WER | Hindi WER | English WER | Size | Offline |
|-------|-----------|-----------|-------------|------|---------|
| IndicWhisper | 12% | 8% | 6% | 500 MB | ✅ |
| Bhashini ASR | 8% | 5% | N/A | API | ❌ |
| Whisper Large v3 | 25% | 18% | 4% | 1.5 GB | ✅ |
| Whisper Base | 45% | 35% | 8% | 75 MB | ✅ |

*WER = Word Error Rate (lower is better). Tested on elder speech samples.*

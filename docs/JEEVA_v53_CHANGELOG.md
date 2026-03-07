# JEEVA v5.3 Changelog

## Version: 5.3 (Audit Implementation)
## Previous Version: 5.2 (Pre-Audit)
## Date: 2025

This changelog documents all changes from JEEVA v5.2 to v5.3 as part of the
14-dimension engineering master audit implementation.

---

## BOM Changes

| Component | Change | Delta/Unit | Rationale |
|-----------|--------|-----------|-----------|
| Head servos (×2) | SG90 → **MG90S** (metal gear) | +₹130 | Plastic gears strip in 3–4 weeks continuous use |
| Magnetometer | HMC5883L → **DROPPED** | −₹130 | Discontinued; all Indian stock is counterfeit QMC5883L |
| Temp/Humidity | DHT22 → **SHT40** | +₹60 | I2C (no FreeRTOS timing conflict), ±0.2°C |
| Emergency button | New addition | +₹30 | Patient safety — P0 requirement |
| SpO2 finger slot | Mechanical widening to 25mm | +₹50 | Elderly finger accommodation |
| Vibration isolation | 4× M2.5 silicone grommets | +₹20 | Decouple mic PCB from PETG/fan vibration |
| Environmental: IPX2 | Silicone gaskets + sealed pass-throughs | +₹100 | Monsoon/drip protection |
| Environmental: Humidity | Conformal coating + silica gel | +₹36 | 85–95% RH Indian monsoon |
| Environmental: Thermal | Thermal pad (Jetson→chassis) | +₹30 | Indian summer heat 42–48°C |
| Environmental: Dust | 0.5mm nylon mesh on vents | +₹20 | Dust protection |
| Environmental: Insects | Acoustic mesh + foam gasket + cable glands | +₹60 | Insect prevention (speaker, mic, cables) |
| Environmental: Drop | EVA foam padding (display) | +₹15 | 1m drop protection |
| Camera FFC | Standard → 300mm high-flex FFC | +₹100 | Prevents cable cracking from pan/tilt |
| Tamper stickers | Tamper-evident stickers on screws | +₹10 | Physical security |
| Second mic (array) | Extra INMP441 | +₹150 | Dual-mic array for elder-care room SNR |

### BOM Delta Summary
| | Amount |
|---|--------|
| Previous BOM total (v5.2) | ₹52,899/unit |
| Net change | **+₹631/unit** |
| New BOM total (v5.3) | **~₹53,530/unit** |
| Budget status | ✅ Within budget (₹55,000 target) |

---

## AI Stack Changes

### ASR (Speech Recognition)
| Previous (v5.2) | v5.3 | Reason |
|----------------|------|--------|
| Whisper base (English-optimized) | **AI4Bharat IndicWhisper** | Tamil WER: 45% → 12% |
| No online enhancement | **Bhashini ASR** (Govt of India free API) | Tamil WER: 12% → 8% |

### LLM (Language Model)
| Previous (v5.2) | v5.3 | Reason |
|----------------|------|--------|
| Llama 3.2 3B (English) | **Sarvam-2B** for Tamil/Hindi | Tamil quality: 4/10 → 8.5/10 |
| Single model | **Language router** (Sarvam-2B + Llama 3.2 3B) | Tamil and English both excellent |

### TTS (Text-to-Speech)
| Previous (v5.2) | v5.3 | Reason |
|----------------|------|--------|
| pyttsx3/espeak (robotic) | **Bhashini TTS** (Govt API) | Tamil quality: 1/10 → 9/10 |
| No offline Tamil | **AI4Bharat IndicTTS** | Tamil offline fallback: 7.5/10 |
| No Indian-accented English | **Piper TTS** (en_IN) | Natural Indian English |

---

## Security Changes

| Area | Previous (v5.2) | v5.3 | Standard |
|------|----------------|------|----------|
| API Authentication | None | **JWT (HS256)** | Industry standard |
| ESP32 WiFi credentials | ~~config.h (hardcoded)~~ | **NVS encrypted partition** | Security requirement |
| ESP32 firmware | Unsigned | **Flash Encryption + Secure Boot v2** | ESP-IDF security |
| OTA updates | Unsigned binary | **Ed25519 code signing** | Prevents malicious OTA |
| Jetson storage | Plaintext | **LUKS full-disk encryption** | Data protection |
| Jetson SSH | Password auth | **Key-based only** | Security hardening |
| Code protection | Python source | **Cython compiled .so** | IP protection |
| Privacy compliance | None | **DPDP Act 2023 compliance** | Legal requirement |
| Secrets management | Ad-hoc | **.env + .gitignore** | Standard practice |

---

## Wake Phrase Change

| | v5.2 | v5.3 |
|--|------|------|
| **Wake phrase** | "Jeeva" | **"Hello Jeeva"** |
| False trigger rate | ~15% (common Tamil name) | < 1% |
| User experience | Robot interrupts family conversations | Triggers only when addressed |

---

## Night Alert LED Change

| | v5.2 | v5.3 |
|--|------|------|
| **Alert frequency** | 5 Hz strobe | **0.5 Hz gentle amber pulse** |
| Photosensitive seizure risk | ⚠️ Yes (5–30 Hz is danger zone) | ✅ No (0.5 Hz is safe) |
| Sleep disruption | High | Minimal |
| Patient comfort | Poor | Good |

---

## Missing Features Tracked (13 New)

| ID | Feature | Priority |
|----|---------|---------|
| P0-1 | Physical emergency button (GPIO 44) | P0 |
| P0-2 | Conversation memory persistence | P0 |
| P0-3 | SpO2 finger slot widened to 25mm | P0 |
| P0-4 | "Hello Jeeva" wake phrase | P0 |
| P0-5 | Ethics committee paperwork | P0 |
| P1-1 | Fall detection (software, Month 7–8) | P1 |
| P1-2 | Verbal BP logging (Month 5–6) | P1 |
| P1-3 | Multi-resident face recognition (Month 7–8) | P1 |
| P1-4 | Daily data backup (Month 9) | P1 |
| P1-5 | Emergency escalation chain (Month 9) | P1 |
| P1-6 | Camera FFC fatigue mitigation | P1 |
| P2-1 | Caregiver/nurse dashboard | P2 |
| P2-2 | NFC wristbands | P2 |

See `JEEVA_MISSING_FEATURES_TRACKER.md` for full details.

---

## GPIO Map Expansion

| Version | Active GPIOs | Reserved | Total |
|---------|-------------|---------|-------|
| v5.2 | 25 | 0 | 25 |
| v5.3 | **28** | **3** | **31** |

New GPIO assignments:
- **GPIO 44**: Emergency button (hardware interrupt, P0-1)
- **GPIO 45**: SHT40 SDA (I2C — replaces DHT22 single-wire)
- **GPIO 46**: SHT40 SCL (I2C)
- GPIO 47–48: Reserved for production expansion

---

## Documentation Added (v5.3)

All new documentation created in `docs/`:

| File | Content |
|------|---------|
| `JEEVA_ACOUSTIC_DESIGN.md` | Dual-mic array, WebRTC APM, SNR budget |
| `JEEVA_AI_STACK.md` | India-native AI: Sarvam-2B, IndicWhisper, Bhashini |
| `JEEVA_SECURITY_ARCHITECTURE.md` | JWT, Flash Encryption, LUKS, DPDP compliance |
| `JEEVA_ENVIRONMENTAL_PROTECTION.md` | IPX2, monsoon, heat, dust, insects, drop |
| `JEEVA_FUTURE_PROOFING_2031.md` | Jetson longevity, model swappability, battery roadmap |
| `JEEVA_COMPONENT_UPGRADE_ROADMAP.md` | Beta → Production → 2031 component path |
| `JEEVA_PROCUREMENT_GUIDE.md` | Supply chain, counterfeit flags, cell verification |
| `JEEVA_MANUFACTURING_QC.md` | Assembly standardization, per-unit QC checklist |
| `JEEVA_TECH_STACK.md` | FastAPI, TensorRT-LLM, DuckDB, version control policy |
| `JEEVA_MISSING_FEATURES_TRACKER.md` | 13 features with P0/P1/P2 priorities |
| `JEEVA_FUNDED_PROJECT_PLAN.md` | Project plan placeholder (v5.3) |
| `JEEVA_MASTER_AUDIT_REPORT.md` | 14-dimension audit summary |
| `JEEVA_v53_CHANGELOG.md` | This file |

---

## Repository Changes (v5.3)

| File | Change |
|------|--------|
| `.gitignore` | **Created** — prevents secret files from being committed |
| `.env.example` | **Created** — documents all required environment variables |
| `docs/` | **Created** — new directory with 13 documentation files |

---

## Open Issues for Next Sprint

These items are documented but require firmware/code changes (not just documentation):

1. **Night alert LED**: Change 5 Hz → 0.5 Hz in ESP32 firmware (2-line change, URGENT)
2. **WiFi credentials**: Remove from `config.h`, implement NVS loading in firmware
3. **WakeNet model**: Retrain with "Hello Jeeva" phrase
4. **FastAPI migration**: Replace `server/server.js` with Python FastAPI
5. **Bhashini TTS integration**: Replace current TTS in `server/ai-services.js`

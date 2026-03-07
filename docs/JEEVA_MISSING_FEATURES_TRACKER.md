# JEEVA Missing Features Tracker

## Overview

This document tracks all features identified as missing from the current JEEVA design.
Features are prioritized by impact on patient safety and clinical usability.

**Priorities:**
- **P0** — Must have before ANY beta unit goes to a patient
- **P1** — Add during beta build phase (Month 5–9)
- **P2** — Add during beta deployment (Month 10+)

---

## P0: Must Have Before Beta Deployment

### P0-1: Physical Emergency Button
| Field | Detail |
|-------|--------|
| **Description** | Dedicated hardware emergency button for patient to call caregiver |
| **Why P0** | Patient safety — elderly may have fall/medical event when voice fails |
| **GPIO** | GPIO 44 (ESP32-S3, reserved for this purpose) |
| **Hardware** | Large dome button, 40mm diameter, red color (₹30) |
| **Implementation** | Interrupt-driven GPIO, triggers even if main loop hung |
| **Alert behavior** | 0.5 Hz amber pulse LED + audio alert + WhatsApp/SMS to caregiver |
| **Cost** | ₹30/unit |
| **Complexity** | Low (hardware interrupt + notification API) |
| **Timeline** | Week 3 of hardware build |
| **Dependency** | Emergency escalation chain (P1-5) for notification delivery |

### P0-2: Conversation Memory Persistence
| Field | Detail |
|-------|--------|
| **Description** | Remember previous conversations across sessions |
| **Why P0** | Elderly users expect continuity — "You don't remember what we talked about yesterday" is deeply unsettling |
| **Implementation** | SQLite: store last 50 conversations per resident, summarize with LLM into "memory context" |
| **Context injection** | First 500 tokens of every LLM prompt = compressed memory summary |
| **Cost** | ₹0 (software only) |
| **Complexity** | Medium (LLM summarization pipeline) |
| **Timeline** | Month 4 (software) |
| **Dependency** | Resident identification (face recognition P1-3) |

### P0-3: SpO2 Finger Slot Width
| Field | Detail |
|-------|--------|
| **Description** | Widen SpO2 finger slot from current design to 25mm |
| **Why P0** | Current slot is too narrow for elderly fingers (arthritis, swelling) |
| **Implementation** | PETG chassis redesign: slot width 25mm × 22mm height |
| **Mechanical** | Ensure MAX30102 PCB still contacts fingertip with wider slot (add spring-loaded guide) |
| **Cost** | ₹50/unit (spring mechanism) |
| **Complexity** | Low (mechanical design) |
| **Timeline** | Before first print run |
| **Dependency** | None |

### P0-4: "Hello Jeeva" Wake Phrase
| Field | Detail |
|-------|--------|
| **Description** | Replace single-word "Jeeva" wake word with two-word "Hello Jeeva" |
| **Why P0** | "Jeeva" is a common Tamil name — robot triggers constantly in Tamil households |
| **Implementation** | Retrain WakeNet9 model with "Hello Jeeva" phrase (ESP-IDF/ESP-SR) |
| **False trigger rate** | "Jeeva" alone: ~15% false trigger rate → "Hello Jeeva": < 1% |
| **Cost** | ₹0 (software, model retraining) |
| **Complexity** | Medium (WakeNet model training with ESP-SR toolkit) |
| **Timeline** | Month 3 (firmware) |
| **Dependency** | None |

### P0-5: Ethics Committee Paperwork
| Field | Detail |
|-------|--------|
| **Description** | Institutional Ethics Committee (IEC) approval for elder-care AI deployment |
| **Why P0** | Legally required before any robot interacts with patients in clinical settings |
| **Process** | Submit to hospital/university IEC: protocol, consent forms, risk analysis |
| **Cost** | ₹15,000–₹30,000 (filing fees + legal consultation) |
| **Complexity** | High (bureaucratic process, 2–4 month timeline) |
| **Timeline** | Start immediately (Month 1–4, runs in parallel) |
| **Dependency** | Must be approved before unit deployment |

---

## P1: Add During Beta Build Phase

### P1-1: Fall Detection (Software)
| Field | Detail |
|-------|--------|
| **Description** | Detect if elderly resident has fallen using camera + MPU6050 |
| **Implementation** | YOLOv8-pose (skeleton detection) on Jetson: if person horizontal + still → fall alert |
| **Backup** | IMU-based vibration spike detection (if camera view blocked) |
| **Cost** | ₹0 (software only) |
| **Complexity** | High (model tuning for elderly body shapes) |
| **Timeline** | Month 7–8 |
| **Dependency** | Camera mounted correctly (P0 hardware), caregiver alert system |

### P1-2: Verbal BP Logging
| Field | Detail |
|-------|--------|
| **Description** | Resident verbally reports blood pressure reading; robot logs it |
| **Example** | "Jeeva, my BP is 130/85 today" → logs to health record |
| **Why not automated** | Accurate wrist BP sensor > ₹2,000/unit — out of budget |
| **Implementation** | Named entity extraction from ASR output (regex + LLM) |
| **Cost** | ₹0 (software only) |
| **Complexity** | Low (NLP extraction) |
| **Timeline** | Month 5–6 |
| **Dependency** | ASR working reliably (P0) |

### P1-3: Multi-Resident Face Recognition
| Field | Detail |
|-------|--------|
| **Description** | Identify which resident is speaking to enable personalized responses |
| **Use case** | Hospital ward: 4 beds, 4 residents, one JEEVA unit |
| **Implementation** | InsightFace/ArcFace on Jetson, register 5–10 faces per unit |
| **Storage** | Face embeddings in SQLite (local, encrypted) |
| **Cost** | ₹0 (software only) |
| **Complexity** | Medium (registration UI + embedding management) |
| **Timeline** | Month 7–8 |
| **Dependency** | Admin web interface |

### P1-4: Daily Data Backup
| Field | Detail |
|-------|--------|
| **Description** | Automatic daily backup of health data to facility server or cloud |
| **Implementation** | Encrypted backup via rsync or S3-compatible API |
| **Encryption** | AES-256 before transmission (patient data cannot be in plain text) |
| **Cost** | ₹0 (software) + ₹200/month cloud storage (optional) |
| **Complexity** | Low (cron job + encrypted rsync) |
| **Timeline** | Month 9 |
| **Dependency** | Facility IT approval, DPDP compliance review |

### P1-5: Emergency Escalation Chain
| Field | Detail |
|-------|--------|
| **Description** | When emergency button pressed: notify caregiver phone → nurse station → family |
| **Channels** | WhatsApp Business API (preferred in India) + SMS fallback |
| **Escalation** | Primary caregiver → 5 min no response → nurse station → 5 min → family |
| **Cost** | ₹0 (software) + WhatsApp Business API cost (~₹0.10/message) |
| **Complexity** | Medium (API integration + escalation state machine) |
| **Timeline** | Month 9 |
| **Dependency** | P0-1 emergency button hardware |

### P1-6: Camera FFC Fatigue Mitigation
| Field | Detail |
|-------|--------|
| **Description** | Prevent camera flex cable failure from pan/tilt motion |
| **Implementation** | (a) Use 300mm high-flex FFC (hardware, P0-level hardware fix) |
| | (b) Software: Park head at center during 8-hour dock/sleep periods |
| | (c) Monitoring: detect camera disconnection, log for preventive maintenance |
| **Cost** | ₹100 (high-flex cable) + ₹0 (software) |
| **Complexity** | Low |
| **Timeline** | Month 5 (hardware) + Month 6 (software) |
| **Dependency** | None |

---

## P2: Beta Deployment Phase

### P2-1: Caregiver / Nurse Dashboard
| Field | Detail |
|-------|--------|
| **Description** | Web dashboard for caregivers to view resident health trends |
| **Features** | SpO2 chart, conversation log, medication reminders, alert history |
| **Tech** | React frontend + FastAPI backend (or simple Chart.js + Express) |
| **Cost** | ₹0 (software) |
| **Complexity** | High (full web application) |
| **Timeline** | Month 10–12 |
| **Dependency** | Health database (P1-4), auth system |

### P2-2: NFC Wristbands for Residents
| Field | Detail |
|-------|--------|
| **Description** | NFC wristband for quick resident identification (supplement to face recognition) |
| **Use case** | Resident with back to camera, or dim lighting — NFC is instant |
| **Hardware** | RC522 RFID/NFC reader (₹80) + NFC wristbands (₹40 each) |
| **Cost** | ₹80 + ₹40/resident |
| **Complexity** | Low (NFC library + resident DB lookup) |
| **Timeline** | Month 11 |
| **Dependency** | Resident database |

### P2-3: Night Alert LED Fix
| Field | Detail |
|-------|--------|
| **Description** | Replace 5 Hz emergency LED strobe with 0.5 Hz gentle amber pulse |
| **Why P2** | Current 5 Hz strobe could trigger photosensitive epilepsy — serious safety issue |
| **Correct spec** | 0.5 Hz amber pulse (1 second on, 1 second off) |
| **Implementation** | Change PWM parameters in ESP32 LED control code |
| **Cost** | ₹0 (software only) |
| **Complexity** | Trivial (2-line code change) |
| **Timeline** | **Fix in next sprint (before beta)** — actually P0 priority |
| **Dependency** | None |

### P2-4: Speaker Upgrade for Elderly Hearing
| Field | Detail |
|-------|--------|
| **Description** | Upgrade speaker for elderly users with presbycusis (high-frequency hearing loss) |
| **Hardware** | 5W 4Ω speaker (₹150) + NS4168 I2S amplifier (₹80) |
| **Benefit** | 10 dB louder, better speech clarity at 1–4 kHz |
| **Cost** | +₹170/unit vs basic speaker |
| **Complexity** | Low (hardware swap + I2S config) |
| **Timeline** | Month 10 (hardware rev) |
| **Dependency** | Chassis redesign to fit larger speaker |

---

## Feature Summary Table

| ID | Feature | Priority | Cost | Timeline | Status |
|----|---------|---------|------|----------|--------|
| P0-1 | Emergency button | P0 | ₹30 | Week 3 | ⬜ Not started |
| P0-2 | Conversation memory | P0 | ₹0 | Month 4 | ⬜ Not started |
| P0-3 | SpO2 slot widening | P0 | ₹50 | Pre-print | ⬜ Not started |
| P0-4 | "Hello Jeeva" wake | P0 | ₹0 | Month 3 | ⬜ Not started |
| P0-5 | Ethics committee | P0 | ₹15-30K | Month 1–4 | ⬜ Not started |
| P1-1 | Fall detection | P1 | ₹0 | Month 7–8 | ⬜ Not started |
| P1-2 | Verbal BP logging | P1 | ₹0 | Month 5–6 | ⬜ Not started |
| P1-3 | Face recognition | P1 | ₹0 | Month 7–8 | ⬜ Not started |
| P1-4 | Daily data backup | P1 | ₹0 | Month 9 | ⬜ Not started |
| P1-5 | Emergency escalation | P1 | ~₹0 | Month 9 | ⬜ Not started |
| P1-6 | Camera FFC fix | P1 | ₹100 | Month 5–6 | ⬜ Not started |
| P2-1 | Caregiver dashboard | P2 | ₹0 | Month 10–12 | ⬜ Not started |
| P2-2 | NFC wristbands | P2 | ₹120 | Month 11 | ⬜ Not started |
| P2-3 | Night LED fix | **P0** | ₹0 | **Immediate** | ⬜ **Urgent** |
| P2-4 | Speaker upgrade | P2 | ₹170 | Month 10 | ⬜ Not started |

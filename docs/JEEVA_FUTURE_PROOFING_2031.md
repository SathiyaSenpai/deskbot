# JEEVA Future-Proofing 2031

## Design Longevity Commitment

JEEVA robots deployed in 2025–2026 must remain functional and serviceable through 2031
(5-year minimum support window). This document outlines decisions made to ensure longevity.

---

## Jetson Orin Nano Super Platform

### NVIDIA Support Timeline
- **Jetson Orin Nano Super** officially supported through **~2032**
- NVIDIA JetPack SDK: Long-term support releases (3-year LTS cadence)
- TensorRT-LLM: Active development, backwards compatible API design

### Future-Proof API Choices
| Avoid (fragile) | Use (stable) | Reason |
|-----------------|--------------|--------|
| Direct CUDA kernel calls | TensorRT-LLM | Abstracts hardware changes |
| OpenCV CUDA-specific APIs | OpenCV standard | Portable across versions |
| Custom NVIDIA-only calls | DeepStream GStreamer | Standard pipeline interface |

### Software Architecture
```python
# Good: Abstract inference backend
class LLMInference:
    def __init__(self, backend: str = "llamacpp"):  # Swappable: llamacpp, tensorrt, onnx
        self.backend = load_backend(backend)
    
    def generate(self, prompt: str) -> str:
        return self.backend.generate(prompt)  # Same API regardless of backend
```

---

## AI Model Swappability

### Config-Driven Model Loading
```yaml
# /opt/jeeva/config/models.yaml — change models without code changes
llm:
  tamil: sarvam-2b-q4_k_m.gguf    # Swap to sarvam-3b when available
  english: llama-3.2-3b-q4_k_m.gguf

asr:
  primary: indicwhisper-v2         # Swap to indicwhisper-v3 when released
  online_enhancement: bhashini

tts:
  online: bhashini
  offline_tamil: indicTTS-v2
  offline_english: piper-en_IN-low
```

### Quarterly Model Evaluation Process
1. Check AI4Bharat releases (monthly newsletter)
2. Check Bhashini API changelog
3. Run WER benchmark on standard Tamil/Hindi/English test set
4. If new model improves WER > 5%, schedule upgrade
5. Test on 3 JEEVA units in staging before production rollout
6. OTA update via signed firmware mechanism

---

## Battery Technology Roadmap

### Current: LiFePO4 (2025–2031)
- LiFePO4 chemistry: stable, >2000 cycles, safe at 45°C Indian heat
- Expected battery life: 3–5 years before capacity degrades to 80%
- **Modular BMS connector**: Robot designed for battery replacement without full disassembly

### Battery Replacement Design
```
User-replaceable battery pack:
- 4× 32700 LiFePO4 cells (3.2V, 6Ah each) in 2S2P = 6.4V, 12Ah
- JST-XH locking connector (not bare terminals)
- Battery tray slides out from robot bottom (4 screws)
- BMS included in battery pack (not in robot mainboard)
```

### 2029+ Sodium-Ion Consideration
- Sodium-ion batteries: 30% cheaper than LiFePO4 by 2029 (projected)
- Same voltage profile — drop-in replacement for LiFePO4
- Better cold performance (relevant for North India winter deployments)
- Monitor: CATL, Sodium Energy quarterly pricing

---

## Connectivity Roadmap

### WiFi (2025–2031)
- WiFi 6 (802.11ax): Sufficient for all JEEVA data needs
- Hospital WiFi upgrading to WiFi 6 in 2025–2027 (Indian hospital modernization)
- No hardware change needed

### 4G/5G Modem Slot (Production Design)
```
Hardware provision in production PCB:
- M.2 2242 slot for USB 4G/5G modem
- SIM card slot (nano-SIM)
- Enables deployment in rural areas without WiFi
- Bhashini API works over 4G (LLM inference can stay offline)
```

### Expected Bandwidth Needs
| Feature | Bandwidth | Notes |
|---------|-----------|-------|
| Bhashini TTS (1 response) | ~50 KB | Very low |
| Bhashini ASR (10s audio) | ~160 KB | Low |
| OTA firmware update | ~50 MB | Monthly, can use WiFi only |
| Health data backup | ~1 MB/day | Very low |
| **Total daily data** | **< 100 MB** | Fits on 1 GB/month plan |

---

## CDSCO Regulatory Trajectory

JEEVA's SpO2 measurement and health monitoring may eventually require CDSCO
(Central Drugs Standard Control Organisation) medical device certification.

### Start Good Habits Now (2025)
Even before certification is required, adopt these practices:
1. **IEC 62304 documentation**: Document software safety classification, risk analysis
2. **Version control**: All software versions tagged, release notes maintained
3. **Traceability**: Requirements → design → code → test linkage
4. **Defect tracking**: GitHub Issues with severity classification

### Regulatory Timeline
| Year | Milestone |
|------|-----------|
| 2025–2026 | Research use (no CDSCO approval needed) |
| 2027 | Apply for Class B medical device if commercializing SpO2 |
| 2028 | CDSCO approval expected (1–2 year process) |
| 2029+ | Full regulatory compliance, CE marking (if export) |

---

## Competition Analysis

### Current Indian Market (2025)

| Competitor | Price | Offline | India Languages | Elder-care Focus |
|------------|-------|---------|-----------------|------------------|
| **Miko 3** | ₹8,000 | No | Hindi only | Children only |
| **Invento Miko** | ₹15,000 | No | Hindi, English | General |
| **Chinese imports** | ₹5,000–20,000 | No | No | No |
| **JEEVA (target)** | **₹45,000** | **Yes** | **Tamil+Hindi+English** | **Elder-care ✅** |

### JEEVA's Defensible Moat

1. **Clinical trial data**: Partnering with elder-care facilities for evidence
2. **India-native AI**: Only robot with Sarvam-2B + IndicWhisper + Bhashini TTS
3. **Price point**: ₹45K (affordable for nursing homes, not luxury)
4. **Offline-first**: Works during internet outages (common in India)
5. **DPDP compliance**: Only robot designed from ground-up for Indian privacy law
6. **Patents**: File provisional patents on: acoustic array design, elder-care conversation memory, multilingual health RAG

---

## Component End-of-Life Planning

### Camera Module
| Timeline | Module | Interface | Notes |
|----------|--------|-----------|-------|
| Beta (2025) | IMX219 (CSI) | MIPI CSI-2 | Widely available |
| Production (2027) | IMX708 | MIPI CSI-2 | Autofocus, HDR, better night |
| 2031 | TBD (Sony IMX500?) | MIPI CSI-2 | AI-on-chip (reduces Jetson load) |

IMX708 is CSI-2 compatible — software change only (update GStreamer pipeline params).

### Eliminate Breakout Board Dependencies
```
Beta: Uses 5+ breakout boards (I2C adapter, power, etc.)
Production target: Custom single-board PCB
Benefits:
  - Eliminates connectors (common failure point)
  - Reduces footprint by 40%
  - Reduces cost by ₹800/unit
  - Professional look for clinical settings
  - 3-year minimum PCB supply guarantee from manufacturer
```

### Sensor EOL Watch List
| Sensor | Status | Risk | Mitigation |
|--------|--------|------|------------|
| MPU6050 | Active but old | Medium | Switch to BMI270 in production |
| HMC5883L | **DISCONTINUED** | **Critical** | **Drop from BOM** |
| TCRT5000 | Active | Low | VL53L0X upgrade planned |
| DHT22 | Active | Low | SHT40 upgrade planned |
| INA219 | Active | Low | MAX17048 upgrade for production |

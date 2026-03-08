# JEEVA Acoustic Design Architecture

## Problem Statement

A single INMP441 microphone is non-viable in typical Indian elder-care rooms due to:

- **Ceiling fan noise**: 50 dBA (constant broadband noise)
- **Room AC/blower noise**: 30 dBA (structure-borne + airborne vibration)
- **Reflective cement walls**: RT60 > 0.5 s (reverberation time)
- **Net result**: SNR < 10 dB at 1.5 m distance

Whisper ASR requires ≥ 15 dB SNR for reliable transcription. The single-mic configuration
fails this threshold in the majority of Indian elder-care settings.

---

## Solution: 2× INMP441 End-Fire Microphone Array

### Configuration
- **Array type**: End-fire (delay-and-sum beamforming)
- **Mic spacing**: 6 cm (optimal for 300 Hz–3 kHz voice band at room temperature)
- **Interface**: Jetson I2S (2× I2S channels, left/right channel multiplexing)
- **Beamforming**: WebRTC APM (software, runs on Jetson CPU)
- **Expected SNR improvement**: +8–12 dB over single mic at 1.5 m

### Beamforming Pipeline
```
INMP441 L  ──┐
             ├── I2S → Jetson → WebRTC APM → IndicWhisper ASR
INMP441 R  ──┘           (noise suppression + echo cancellation + AGC)
```

### WebRTC APM Modules Enabled
| Module | Purpose |
|--------|---------|
| Noise Suppression (NS) | Removes fan/blower noise |
| Acoustic Echo Cancellation (AEC) | Removes speaker echo |
| Automatic Gain Control (AGC) | Normalizes voice level |
| Voice Activity Detection (VAD) | Triggers wake-word detection |

---

## Vibration Isolation

### Problem
The PETG robot chassis transmits blower fan structure-borne vibration directly to mic PCBs
mounted with rigid M2.5 screws. This adds 15–25 dB of low-frequency noise to recordings.

### Solution
**4× M2.5 silicone grommets** (shore hardness 40A) at each mic PCB mounting point.

| Parameter | Rigid Mount | Grommet Mount |
|-----------|-------------|---------------|
| Blower vibration (80 Hz) | −5 dB | −28 dB |
| Fan vibration (120 Hz) | −8 dB | −32 dB |
| Structural resonance | Present | Eliminated |
| Cost | ₹0 | +₹10/unit |

### Grommet Specification
- Material: Silicone (not rubber — degrades in Indian heat)
- Shore hardness: 40A (softer = better isolation at low frequencies)
- Diameter: M2.5 bore, 6 mm OD
- Supplier: Amazon India (₹120 for 50-pack)

---

## SNR Budget: Indian Elder-Care Room Scenarios

| Scenario | Fan | AC | Reflections | Single Mic SNR | Array SNR | Whisper Pass? |
|----------|-----|----|-------------|----------------|-----------|---------------|
| Quiet night | Off | Off | Low | 28 dB | 36 dB | ✅ Yes |
| Day (fan only) | On | Off | Medium | 9 dB | 19 dB | ✅ Yes |
| Day (fan + AC) | On | On | Medium | 4 dB | 14 dB | ⚠️ Marginal |
| Hot day (all) | On | Full | High | −2 dB | 10 dB | ❌ Fail |
| Array + NS | On | Full | High | −2 dB | **18 dB** | ✅ Yes |

*NS = WebRTC Noise Suppression enabled*

---

## Microphone Model Comparison

| Model | SNR | Sensitivity | Interface | India Price | Recommendation |
|-------|-----|-------------|-----------|-------------|----------------|
| **INMP441** | 58 dB | −26 dBFS | I2S | ₹150 | ✅ Beta (use 2×) |
| SPH0645LM4H | 65 dB | −26 dBFS | I2S | ₹250–350 | 🔄 Production upgrade |
| ICS-43434 | 70 dB | −26 dBFS | I2S | ₹300–500 | 🔮 2031 target |

### Why INMP441 for Beta
- Widely available in India (Robocraze, Robu.in)
- Well-documented I2S driver for Jetson
- 2× array compensates for lower SNR vs single high-end mic
- Cost-effective: 2× INMP441 (₹300) vs 1× ICS-43434 (₹400) with better real-world performance

---

## Speaker–Mic Geometry

### Acoustic Feedback Prevention
- **Minimum separation**: 8 cm (center-to-center, speaker cone to nearest mic)
- **PETG acoustic baffle**: 3 mm wall between speaker cavity and mic cavity
- **Software AEC**: WebRTC AEC3 (removes speaker echo before VAD)
- **Half-duplex fallback**: Disable mic input while TTS is playing (backup if AEC insufficient)

### Geometry Diagram
```
[Mic L] ←── 8 cm+ ───→ [Speaker]
[Mic R] ←── 6 cm ──→ [Mic L]
         ↑
    PETG baffle
```

---

## Speaker Upgrade for Elderly Hearing

Indian elderly users often have presbycusis (high-frequency hearing loss). Standard 1W
8Ω speakers are insufficient for clear speech intelligibility.

| Component | Beta | Production | 2031 |
|-----------|------|------------|------|
| Speaker | 1W 8Ω (₹60) | 5W 4Ω (₹150) | Directional parametric |
| Amplifier | PAM8403 (₹30) | NS4168 I2S amp (₹80) | Custom PCB |
| SPL @ 1m | 78 dB | 88 dB | 92 dB |

### NS4168 Amplifier Benefits
- I2S digital input (eliminates DAC noise)
- 5W output (10 dB louder than 0.5W)
- Built-in EQ for speech clarity boost at 1–4 kHz
- India supplier: LCSC/Mouser (₹80/unit)

---

## Testing Protocol

### Acoustic Acceptance Test (Per Unit)

**Setup**
1. Mount both INMP441 mics on silicone grommets in assembled chassis
2. Place robot in simulated elder-care room (or corner of lab)
3. Run blower fan at full speed

**Test Procedure**
1. Play reference audio (3 Tamil sentences + 3 Hindi sentences + 3 English sentences) from 1.5 m
2. Record with WebRTC APM pipeline active
3. Run IndicWhisper on recording
4. Calculate WER (Word Error Rate) vs. reference transcription

**Acceptance Criteria**
- WER degradation vs. quiet baseline: **< 15%**
- If WER degradation ≥ 15%: inspect grommet mounting, re-run AEC calibration

**Automated Test Script**
```bash
# Run on Jetson
python3 /opt/jeeva/scripts/acoustic_test.py \
  --reference /opt/jeeva/test_audio/reference_sentences.json \
  --output /tmp/acoustic_test_$(date +%Y%m%d_%H%M%S).json
```

---

## Cost Impact Summary

| Item | Qty | Unit Cost | Total |
|------|-----|-----------|-------|
| Extra INMP441 mic | 1 | ₹150 | ₹150 |
| M2.5 silicone grommets | 4 | ₹3 | ₹12 |
| 5W 4Ω speaker upgrade | 1 | +₹90 | ₹90 |
| **Total acoustic upgrade** | | | **+₹252/unit** |

*Rounded to +₹200/unit in BOM summary (speaker upgrade tracked separately)*

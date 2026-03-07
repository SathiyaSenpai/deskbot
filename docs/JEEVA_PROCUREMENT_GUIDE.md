# JEEVA Procurement Guide

## Overview

This guide covers sourcing every BOM component for 16 beta units.
India has unique supply chain challenges: counterfeit components, import delays,
customs classification issues, and limited local stock.

---

## Component Sourcing Matrix

### Compute & Connectivity

| Component | Qty (16 units) | Primary Supplier | Price | Backup Supplier | Stock Status | Lead Time |
|-----------|---------------|-----------------|-------|-----------------|-------------|-----------|
| Jetson Orin Nano Super 8GB | 16 + 2 spare | NVIDIA India (e-Infochips) | ₹22,000 | Arrow Electronics India | In stock | 2–4 weeks |
| ESP32-S3-WROOM-1 | 16 + 4 spare | Robu.in | ₹350 | Robocraze | In stock | 3–5 days |
| WiFi antenna (if external) | 16 | Amazon India | ₹50 | AliExpress | In stock | 3–5 days |

---

### Sensors

| Component | Qty (16 units) | Primary | Price | Backup | Stock | Lead Time | Counterfeit Risk |
|-----------|---------------|---------|-------|--------|-------|-----------|-----------------|
| INMP441 mic | 32 + 8 spare | Amazon India | ₹150 | AliExpress (reputable) | In stock | 3–7 days | Low |
| SHT40 (temp/humidity) | 16 + 4 spare | Robu.in | ₹140 | Mouser India | In stock | 3–5 days | Low |
| VL53L0X ToF (cliff) | 32 + 8 spare | Mouser India | ₹300 | Robu.in | In stock | 5–7 days | Medium |
| HC-SR04 ultrasonic | 16 + 4 spare | Robu.in | ₹50 | Amazon India | In stock | 3–5 days | Low |
| MAX30102 SpO2 | 16 + 4 spare | Amazon India | ₹250 | AliExpress | In stock | 3–7 days | Medium |
| MPU6050 IMU | 16 + 4 spare | Robu.in | ₹120 | Amazon India | In stock | 3–5 days | **High** |
| INA219 power monitor | 16 + 4 spare | Robu.in | ₹80 | Robocraze | In stock | 3–5 days | Low |

---

### Motors & Servos

| Component | Qty (16 units) | Primary | Price | Backup | Stock | Lead Time | Counterfeit Risk |
|-----------|---------------|---------|-------|--------|-------|-----------|-----------------|
| N20 gear motor 6V 100RPM | 32 + 8 spare | Robocraze | ₹150 | Robu.in | In stock | 3–5 days | Low |
| TB6612FNG motor driver | 16 + 4 spare | **Robu.in only** | ₹120 | Mouser India | In stock | 3–5 days | **VERY HIGH** |
| MG90S servo (metal gear) | 32 + 8 spare | Robocraze | ₹230 | Amazon India | In stock | 3–5 days | Medium |

---

### Power Components

| Component | Qty (16 units) | Primary | Price | Backup | Stock | Lead Time | Counterfeit Risk |
|-----------|---------------|---------|-------|--------|-------|-----------|-----------------|
| 32700 LiFePO4 cells | 64 + 16 spare | **Verified seller only** | ₹200 | See cell verification below | Variable | **2–4 weeks** | **CRITICAL** |
| BMS 2S 20A | 16 + 4 spare | Amazon India | ₹150 | AliExpress | In stock | 3–7 days | Low |
| Buck converter 5V 5A | 16 + 4 spare | Robu.in | ₹150 | Amazon India | In stock | 3–5 days | Low |
| TP4056 charging module | 16 + 4 spare | Robu.in | ₹30 | Amazon India | In stock | 3–5 days | Low |

---

### Display & Audio

| Component | Qty (16 units) | Primary | Price | Backup | Stock | Lead Time |
|-----------|---------------|---------|-------|--------|-------|-----------|
| SSD1306 OLED 128×64 | 32 + 8 spare | Amazon India | ₹120 | Robu.in | In stock | 3–5 days |
| 5W 4Ω speaker | 16 + 4 spare | Amazon India | ₹150 | Robocraze | In stock | 3–5 days |
| NS4168 amplifier | 16 + 4 spare | LCSC India / Mouser | ₹80 | LCSC | In stock | 5–10 days |
| IMX219 camera (CSI) | 16 + 4 spare | Amazon India | ₹800 | Waveshare India | In stock | 3–5 days |
| High-flex FFC 300mm | 16 + 4 spare | AliExpress | ₹150 | Amazon India | Variable | 7–14 days |

---

## AliExpress Import Cost Calculator

For components ordered from AliExpress, total landed cost includes:

```
Landed Cost = Product Price + Shipping + 18% IGST + Customs Duty

Example: 10× INMP441 mics
  AliExpress price: ₹80 each = ₹800
  Shipping: ₹500 (China Post registered)
  Subtotal: ₹1,300
  IGST 18%: ₹234
  Customs duty (HS 8543.70): 10% = ₹130
  Total landed: ₹1,664
  Per unit: ₹166 (vs ₹150 Amazon India — prefer domestic)
```

### Key HS Codes for JEEVA Components

| Component | HS Code | Customs Duty | Notes |
|-----------|---------|-------------|-------|
| Microphones (INMP441) | 8518.10 | 15% | Basic customs duty |
| Microcontrollers | 8542.31 | 0% | ITA-1 exemption |
| Sensors (temperature etc.) | 9025.19 | 10% | |
| Servo motors | 8501.10 | 7.5% | |
| LiFePO4 cells | 8507.60 | 10% + BCD | **Check annual budget notification** |
| SpO2 module | 9018.19 | **15% + may be medical device** | See warning below |
| Cameras | 8525.80 | 10% | |

---

## Counterfeit Risk Flags

### ⚠️ CRITICAL: LiFePO4 32700 Cells

> **Verified capacity is essential.** Many sellers claim 6000mAh; tested capacity is
> 2500–3500mAh. Substandard cells = 40% of rated battery life = poor patient experience.

**LiFePO4 Cell Verification Protocol:**
```
For EVERY BATCH of cells received:
1. Fully charge to 3.65V at 1A (3.2V nominal)
2. Rest 1 hour at room temperature
3. Discharge at 3A constant current to 2.5V cutoff
4. Measure actual mAh delivered
5. ACCEPTANCE CRITERIA: Must deliver ≥ 5,400 mAh (90% of rated 6,000 mAh)
6. REJECT batch if any cell < 5,400 mAh
7. Document: batch number, supplier, date, tested capacity
```

**Verified Sellers (India):**
- Loom Solar (Amazon India) — tested, consistent
- Powertech Systems (direct) — industrial supplier, genuine cells
- AVOID: Random AliExpress sellers, WhatsApp groups claiming "genuine"

---

### ⚠️ HIGH: TB6612FNG Motor Driver

> Genuine Toshiba part is becoming scarce. Indian market is flooded with
> unmarked counterfeits that overheat and fail at normal load.

**Verification:**
- Buy exclusively from Robu.in or Mouser India (verified supply chain)
- Genuine chip: "TOSHIBA TB6612FNG" with proper logo, sharp marking
- Counterfeit: Blurry text, missing logo, sometimes just "TB6612FNG" with no brand
- Test: Run at 800mA for 5 minutes — genuine stays cool, counterfeit gets hot

---

### ⚠️ HIGH: MPU6050 IMU

> Market has many clones with degraded gyroscope performance.

**Verification:**
- Buy from Robu.in or Robocraze (reputable Indian suppliers)
- Test: Run calibration routine, gyro drift must be < 1°/minute at room temp
- If drift > 3°/min: likely clone — affects head tracking accuracy

---

### ⚠️ CRITICAL: HMC5883L Magnetometer

> **Do not buy.** Officially discontinued. All Indian stock (100%) is QMC5883L
> clones mis-labeled as HMC5883L. Different register map causes driver failures.
> **Remove from BOM. Not used in JEEVA.**

---

## SpO2 Module Customs Risk

> ⚠️ **MAX30102 SpO2 modules may be classified as medical devices by Indian customs.**

Classification depends on presentation:
- "SpO2 sensor for hobbyist/development" → Electronic component (HS 8542)
- "Blood oxygen measurement module" → Medical device (HS 9018) → 15% + licensing

**Recommendation:**
- Order as "proximity and optical sensor module" (accurate description, less risk)
- Buy from domestic Amazon India stock (already cleared customs)
- For bulk orders > 20 units: consult customs broker

---

## Spare Parts Manifest (16-Unit Beta Build)

Maintain 20% extra stock of consumables and high-failure-risk components:

| Component | Production Qty | Spare Qty (+20%) | Total Order |
|-----------|---------------|-----------------|-------------|
| ESP32-S3 module | 16 | 4 | 20 |
| SG90/MG90S servos | 32 | 8 | 40 |
| INMP441 mics | 32 | 8 | 40 |
| N20 motors | 32 | 8 | 40 |
| LiFePO4 cells | 64 | 16 | 80 |
| MAX30102 SpO2 | 16 | 4 | 20 |
| SSD1306 OLED | 32 | 8 | 40 |
| TB6612FNG drivers | 16 | 4 | 20 (buy from Robu.in only) |
| High-flex FFC | 16 | 8 (+50% — high failure) | 24 |
| BMS modules | 16 | 4 | 20 |

---

## Bulk Ordering Strategy for W1 (Wave 1: 16 Units)

### Timeline
| Week | Activity |
|------|----------|
| W1-2 | Order Jetson modules (longest lead time) |
| W1-2 | Order LiFePO4 cells (verify batch on arrival) |
| W3 | Order all remaining electronic components |
| W4 | Order PETG filament + hardware (screws, standoffs, grommets) |
| W5 | Components arrive, run counterfeit checks |
| W6 | Begin assembly |

### Minimum Order Quantities for Bulk Discount
- Robu.in / Robocraze: No MOQ, but 10+ units = 5% discount (ask sales team)
- Mouser India: No MOQ, standard pricing
- AliExpress: 10+ units often triggers free DHL shipping (faster + more reliable)

### Budget Buffer
Allocate 10% of BOM budget as contingency for:
- Counterfeit replacements
- Failed QC components
- Shipping damage
- Price increases (electronics prices volatile)

# JEEVA Component Upgrade Roadmap

## Overview

This document tracks every component from the beta design through production (2027)
to the 2031 long-term target. All costs are India retail prices (inclusive of GST).

---

## Component Upgrade Table

### Microcontroller / Compute

| Component Role | Beta (2025) | Production (2027) | 2031 Target |
|---------------|-------------|-------------------|-------------|
| **Main compute** | Jetson Orin Nano Super (8 GB) | Same | Jetson Orin NX 16 GB |
| **MCU (voice/motors)** | ESP32-S3-WROOM-1 | Same | Custom PCB with ESP32-S3 |
| **MCU framework** | PlatformIO + Arduino | ESP-IDF C++ | ESP-IDF C++ (hardened) |

---

### Environmental Sensors

| Sensor Role | Beta | Production | 2031 | Notes |
|------------|------|------------|------|-------|
| **Temp/Humidity** | DHT22 (₹80) | **SHT40** (₹140) | SHT41 | I2C, ±0.2°C, saves GPIO |
| **IMU** | MPU6050 (₹120) | **BMI270** (₹200) | BMI270 | Lower noise, lower power, built-in step counter |
| **Magnetometer** | HMC5883L (₹150) | **DROPPED** | Removed | Discontinued; Indian stock is 99% fake QMC5883L clones |

#### DHT22 → SHT40 Migration Details
- **Why**: DHT22 requires 1-Wire bit-banging with precise FreeRTOS timing. SHT40 is I2C.
  DHT22 causes FreeRTOS timing conflicts with servo PWM on ESP32.
- **Wiring**: SHT40 uses I2C (SDA/SCL) — no new GPIO needed if I2C bus already present
- **Code**: Replace `dht.read()` with `Wire.requestFrom(SHT40_ADDR, 6)` + CRC check
- **Cost delta**: +₹60/unit
- **India supplier**: Robu.in / Robocraze

#### HMC5883L Discontinuation Note
> ⚠️ **HMC5883L has been officially discontinued by Honeywell.**
> All Indian online stock (AliExpress, Amazon India, local shops) is counterfeit QMC5883L
> with wrong register maps. These fail silently and return garbage heading data.
> **Remove from BOM entirely.** Navigation heading not required for elder-care use case.
> Save ₹130/unit.

---

### Proximity / Range Sensors

| Sensor Role | Beta | Production | 2031 | Notes |
|------------|------|------------|------|-------|
| **Cliff detection** | TCRT5000 IR (₹30 × 2) | **VL53L0X ToF** (₹300 × 2) | VL53L1X | Immune to ambient IR; I2C |
| **Obstacle front** | HC-SR04 ultrasonic (₹50) | VL53L1X ToF | Solid-state LIDAR | Better accuracy |

#### TCRT5000 → VL53L0X Migration Details
- **Why TCRT5000 fails**: High ambient IR in India (sunny rooms, tube lights) causes
  false cliff detection. False cliff triggers stop the robot unexpectedly.
- **VL53L0X advantages**: Time-of-flight, immune to ambient light, I2C, ±5% accuracy
- **Address conflict**: Multiple VL53L0X on same I2C need XSHUT pin control for address assignment
- **Cost delta**: +₹540/unit (6 sensors: 2 cliff + 4 obstacle)

---

### Motor Control

| Component | Beta | Production | 2031 | Notes |
|-----------|------|------------|------|-------|
| **Drive motors** | Generic N20 gear motors | Same (better variant) | BLDC with encoders | Higher torque consistency |
| **Motor driver** | TB6612FNG breakout (₹120) | TB6612FNG on custom PCB | Integrated MCU+driver | See counterfeit warning below |
| **Drive encoders** | None | Magnetic encoders (AS5600) | Built-in | Dead reckoning navigation |

#### TB6612FNG Counterfeit Warning
> ⚠️ **High counterfeit rate on AliExpress and some Amazon India listings.**
> Genuine Toshiba TB6612FNG: max 1.2A continuous per channel.
> Counterfeit: overheats at 0.5A, fails within 2 weeks.
> **Buy from**: Robu.in (verified genuine), or Mouser India.
> **Verify**: Genuine chip is marked "TOSHIBA TB6612FNG". Fakes have blurry or
> differently-spaced markings.

---

### Servo Motors

| Component | Beta | Production | 2031 | Notes |
|-----------|------|------------|------|-------|
| **Head pan servo** | SG90 plastic gear (₹100) | **MG90S metal gear** (₹230) | Feetech SCS0009 | See below |
| **Head tilt servo** | SG90 plastic gear (₹100) | **MG90S metal gear** (₹230) | Feetech SCS0009 | See below |

#### SG90 → MG90S Migration
- **Why**: SG90 plastic gears strip within 3–4 weeks of continuous elder-care use
  (head follows faces = constant small movements all day)
- **MG90S**: Metal gears, same form factor, same PWM control, same pin assignments
- **Code change**: None — direct drop-in replacement
- **Cost delta**: +₹130/unit (2 × ₹65 more per servo)

#### MG90S → Feetech SCS0009 (Production)
- **Why**: Serial bus servos (TTL half-duplex UART) allow:
  - Position feedback (know actual servo angle)
  - Current monitoring (detect stalls = head blocked)
  - Multiple servos on single wire (reduces wiring)
- **Cost**: ₹450/unit (vs ₹460 for 2× MG90S) — comparable cost, much better features
- **Interface**: Requires serial bus adapter (ST3215 board, ₹200)

---

### Power Management

| Component | Beta | Production | 2031 | Notes |
|-----------|------|------------|------|-------|
| **Battery current monitor** | INA219 (₹80) | **MAX17048** (₹120) | MAX17049 | Dedicated fuel gauge |
| **5V regulator** | Generic buck converter (₹50) | **Pololu filtered regulator** (₹350) | Custom LDO on PCB | Low noise for mic/audio |
| **Battery cells** | 32700 LiFePO4 4× (₹800) | Same (verified) | Sodium-ion (if available) | See procurement guide |

#### INA219 → MAX17048 Migration
- **Why**: INA219 measures voltage + current (indirect capacity calculation).
  MAX17048 uses fuel-gauging algorithm (direct State-of-Charge, far more accurate)
- **Patient benefit**: Accurate battery % display for caregiver ("Robot needs charging soon")
- **I2C compatible**: Same bus, different address (0x36)

#### Buck Converter → Pololu Regulator
- **Why**: Generic buck converters have high-frequency switching noise that
  couples into audio/mic power rails → audible hum in recordings
- **Pololu D24V50F5** (5V, 5A): Filtered, stable, high-efficiency
- **Cost delta**: +₹300/unit — worth it for audio quality

---

### Display

| Component | Beta | Production | 2031 | Notes |
|-----------|------|------------|------|-------|
| **Eye display** | 2× SSD1306 OLED 128×64 (₹120) | 2× SH1106 1.3" OLED (₹160) | Round GC9A01 LCD | Better contrast for elderly |
| **Status LED** | RGB LED strip (₹50) | Same | Ambient OLED ring | Gentle indicators |

---

### Camera

| Component | Beta | Production | 2031 | Notes |
|-----------|------|------------|------|-------|
| **Main camera** | IMX219 CSI module (₹800) | **IMX708 CSI module** (₹1,200) | IMX500 (AI-in-sensor) | Autofocus + HDR |
| **Camera cable** | Standard FFC (₹50) | **300mm high-flex FFC** (₹150) | Integrated | Prevents cracking from pan/tilt |

#### Camera FFC Cable Note
Standard flat flex cables crack within 2–3 months of continuous pan/tilt motion.
High-flex FFC (fine-stranded copper, extra insulation) rated for 100,000+ flex cycles.

---

## India Pricing and Suppliers Summary

| Component | Beta Price | Production Price | Primary Supplier | Backup Supplier |
|-----------|-----------|-----------------|-----------------|-----------------|
| SHT40 | ₹140 | ₹140 | Robu.in | Robocraze |
| VL53L0X | ₹300 each | ₹280 each | Mouser India | Robu.in |
| MG90S servo | ₹230 each | ₹200 each (bulk) | Robocraze | Amazon India |
| MAX17048 | ₹120 | ₹100 (bulk) | Mouser India | Digi-Key India |
| Pololu D24V50F5 | ₹350 | ₹350 | Pololu direct | Amazon India |
| IMX708 camera | ₹1,200 | ₹1,000 (bulk) | Amazon India | AliExpress (check seller) |
| High-flex FFC | ₹150 | ₹120 (bulk) | AliExpress | Amazon India |

---

## BOM Cost Delta Summary (Beta → Production)

| Change | Delta |
|--------|-------|
| DHT22 → SHT40 | +₹60 |
| MPU6050 → BMI270 | +₹80 |
| HMC5883L → DROPPED | −₹150 |
| TCRT5000 × 2 → VL53L0X × 2 | +₹540 |
| SG90 × 2 → MG90S × 2 | +₹260 |
| INA219 → MAX17048 | +₹40 |
| Buck → Pololu regulator | +₹300 |
| IMX219 → IMX708 | +₹400 |
| FFC upgrade | +₹100 |
| **Total component delta** | **+₹1,630/unit** |

# JEEVA Manufacturing Quality Control

## Overview

This document defines the assembly process and quality control checklist for
**16 JEEVA beta units** (JEEVA-BETA-001 through JEEVA-BETA-016).

Estimated time: **2 working days per unit** (assembly + testing).

---

## Assembly Standardization

### Test Jig Requirements
Before assembling any units, build a **test jig** for rapid PCB verification:

| Jig | Purpose | Checks |
|-----|---------|--------|
| **Power jig** | Test BMS + cell combination | Voltage, charge cutoff, discharge cutoff |
| **Motor jig** | Test N20 motors + TB6612FNG | Both directions, stall current |
| **Servo jig** | Test MG90S servos | Min/max PWM response, zero position |
| **Sensor jig** | Test all I2C sensors | Correct I2C addresses, data output |
| **Audio jig** | Test mic + speaker | Record 5s, play back, confirm clarity |

### Wiring Harness Standardization
- **All wiring**: Pre-made harnesses (NOT random loose wires)
- **Connector standard**: JST-PH 2.0mm locking connectors (NOT Dupont/Dupont jumpers)
- **Color coding**: Follow standard throughout ALL 16 units:

| Color | Signal |
|-------|--------|
| Red | 3.3V power |
| Orange | 5V power |
| Black | GND |
| Yellow | I2C SDA |
| White | I2C SCL |
| Blue | UART TX |
| Green | UART RX |
| Purple | GPIO (general) |
| Brown | PWM (servo) |
| Gray | Analog input |

### Dupont Connector Ban
> ⚠️ **JST-PH locking connectors ONLY.** Dupont connectors vibrate loose during
> robot motion and fall detection events. A loose power connector = field failure =
> patient safety incident.

---

## 3D Printing Consistency

### Printer Configuration
| Parameter | Value | Reason |
|-----------|-------|--------|
| Printer | Same printer for ALL 16 units | Eliminates dimensional variation |
| Filament | Same PETG batch (single 3kg spool if possible) | Color + strength consistency |
| Layer height | 0.2 mm | Structural integrity |
| Infill | 40% gyroid | Impact resistance |
| Wall count | 4 perimeters | Structural strength for drop test |
| Temperature | 240°C / 80°C bed | Per PETG manufacturer spec |

### Dimensional QC
After printing each chassis component:
```
1. Check all M2.5 hole diameters: must accept M2.5 screw without binding
2. Check display cutout: must accept OLED module without force
3. Check speaker grille: must accept 40mm speaker without gap
4. Check servo horn clearance: must rotate ±60° without contact
5. Check cable routing channels: must accommodate all harnesses
```

### Sprocket (Drive Wheel) Calibration
```
After printing drive sprockets:
1. Mount on N20 motor
2. Drive at 50% PWM on flat surface for 30 seconds
3. Measure distance traveled: must be 1,200 mm ± 30 mm
4. If out of range: re-calibrate encoder count or reprint sprocket
```

---

## Cable Reliability

### Connector Reinforcement
After all connectors are crimped and connected:
1. **Hot glue**: Apply small bead over JST-PH connector base (prevents pullout)
2. **Conformal coat**: Spray all PCB connectors after final assembly

### Camera FFC Cable
- Use **300mm high-flex FFC** (fine-stranded copper, rated 100,000+ flex cycles)
- Route with **minimum 20mm bend radius** at both ends
- Secure with cable clips every 40mm along chassis
- Standard FFC cables crack within 2–3 months of pan/tilt use

### Cable Strain Relief
- All external cables (power, any sensors): cable gland or strain relief clip within 30mm of entry point
- All servo cables: leave 20mm slack at servo end (allows full servo rotation without tension)

---

## Unit Identification System

### Serial Number Format
`JEEVA-BETA-XXX` where XXX = 001 through 016

### Labeling Requirements
Each unit must have:
1. **External label** (bottom of robot):
   - Unit serial number (JEEVA-BETA-XXX)
   - Assembly date (YYYY-MM-DD)
   - QR code linking to unit record in tracking database
2. **Internal label** (inside chassis, readable during service):
   - Same serial number
   - Batch numbers of critical components (LiFePO4 cells, ESP32)
3. **Laser engraving** (PETG chassis):
   - Serial number engraved on chassis mold (permanent, cannot be removed)

### Facility Assignment Labels
After deployment:
- Add facility name + room number label to robot
- Update central tracking database with deployment location
- Photograph robot in deployed location (for insurance/warranty records)

---

## Per-Unit QC Checklist

Complete ALL items before shipping any unit. Sign and date each item.

**Unit Serial Number**: JEEVA-BETA-_____
**Assembly Technician**: _________________________
**QC Verifier**: _________________________
**Date**: _________________________

---

### Phase 1: Power On (10 minutes)
- [ ] Battery cells verified capacity ≥ 5,400 mAh (per procurement protocol)
- [ ] Boot test: Jetson powers on, no error LEDs
- [ ] Boot test: ESP32 initializes, WiFi connects
- [ ] Boot test: All I2C sensors detected (SHT40, VL53L0X × 2, MAX17048, MPU6050)
- [ ] Status LED: Correct boot sequence colors

### Phase 2: Voice Interaction (15 minutes)
- [ ] Wake word "Hello Jeeva" detected reliably (5/5 attempts at 1.5 m)
- [ ] Tamil: 3 test phrases transcribed correctly (acceptable: 2/3)
- [ ] Hindi: 3 test phrases transcribed correctly (acceptable: 2/3)
- [ ] English: 3 test phrases transcribed correctly (acceptable: 3/3)
- [ ] TTS response: Tamil audio sounds natural (not robotic)
- [ ] TTS response: Hindi audio sounds natural
- [ ] TTS response: English audio sounds natural

### Phase 3: Health Sensors (10 minutes)
- [ ] SpO2: Place test finger, reading obtained within 5 seconds
- [ ] SpO2: Compare to certified pulse oximeter — must be within ±3% SpO2
- [ ] SpO2: Heart rate reading within ±5 BPM of certified device
- [ ] Temperature: Reading obtained, within ±1°C of calibrated thermometer
- [ ] Humidity: Reading obtained, within ±5% RH of calibrated sensor

### Phase 4: Mobility (15 minutes)
- [ ] Track drive: Forward movement straight (< 5° drift per meter)
- [ ] Track drive: Backward movement (no binding)
- [ ] Track drive: Left turn 90° accurate (±10°)
- [ ] Track drive: Right turn 90° accurate (±10°)
- [ ] Cliff sensors: Detected table edge, robot stopped (3/3 tests)
- [ ] Obstacle detection: Detected obstacle at 15 cm (3/3 tests)
- [ ] Head pan: Full ±60° range without binding
- [ ] Head tilt: Full range without binding

### Phase 5: Docking (20 minutes)
- [ ] Dock approach: Robot finds dock from 50 cm distance
- [ ] Dock lock: Mechanical docking connector engages
- [ ] Dock lock: Charging begins (INA219/MAX17048 shows current flow)
- [ ] Dock undock: Robot exits dock cleanly on voice command
- [ ] Complete dock cycle success: **3 consecutive successful dock cycles**

### Phase 6: Safety Features (10 minutes)
- [ ] Emergency button: Press button → robot stops immediately + calls emergency sequence
- [ ] Emergency button: LED flashes 0.5 Hz amber (NOT 5 Hz strobe)
- [ ] Emergency button: Audio alert in resident's preferred language
- [ ] LED flashlight: Activates on voice command, deactivates on voice command
- [ ] Obstacle avoidance: Robot avoids direct collision (reactive)

### Phase 7: Connectivity (15 minutes)
- [ ] WiFi: Connects to 2.4 GHz network
- [ ] WiFi AP mode: Robot creates hotspot when no WiFi configured
- [ ] PWA (Progressive Web App): Accessible from phone browser
- [ ] Phone pairing: QR code displayed, scanned, paired successfully
- [ ] OTA check: Robot checks for updates (can be simulated)
- [ ] JWT auth: Unauthorized API requests rejected (test with curl)

### Phase 8: Drop Test (30 minutes)
- [ ] 1 m drop, front face down — robot powers on, all systems functional
- [ ] 1 m drop, back face down — robot powers on, all systems functional
- [ ] 1 m drop, left side down — robot powers on, all systems functional
- [ ] 1 m drop, right side down — robot powers on, all systems functional
- [ ] 1 m drop, top down — robot powers on, all systems functional
- [ ] 1 m drop, bottom down — robot powers on, all systems functional

### Phase 9: Burn-In (2 hours)
- [ ] 2-hour continuous operation: No thermal shutdown
- [ ] 2-hour continuous operation: Battery % decreases at expected rate
- [ ] 2-hour continuous operation: No memory leak (RAM usage stable)
- [ ] 2-hour continuous operation: No WiFi disconnections (> 3 = FAIL)
- [ ] 2-hour continuous operation: Head servos not hot (< 50°C surface temp)

### Phase 10: Visual Inspection (10 minutes)
- [ ] Chassis: No cracked PETG from drop tests
- [ ] Display: No dead pixels, correct brightness
- [ ] Camera lens: Clean, no scratches
- [ ] Speaker grille: Intact, no insect entry points
- [ ] All screws: Tightened, tamper-evident stickers applied
- [ ] Serial number label: Present and readable on robot base
- [ ] Internal label: Present and readable (verified during assembly)

---

**QC VERDICT**: ☐ PASS   ☐ FAIL

**Failed items** (if any): _____________________________________________

**Corrective actions taken**: __________________________________________

**Re-test date** (if required): ________________________________________

**Final approval signature**: _________________________  Date: ___________

---

## Spare Parts Manifest (Build Stock)

Keep the following spares on-hand throughout the build:

| Component | Spare Qty | Reason |
|-----------|-----------|--------|
| ESP32-S3 module | 4 | Occasional flash failures |
| MG90S servos | 8 | Gear strip risk during testing |
| INMP441 mics | 8 | Solder damage risk |
| N20 motors | 8 | Shaft damage in testing |
| LiFePO4 cells | 16 | Failed capacity check |
| MAX30102 SpO2 | 4 | Finger slot damage |
| SSD1306 OLED | 8 | Display fractures in drop test |
| JST-PH connectors (assorted) | 200 | Crimp errors |
| High-flex FFC | 8 | Cable damage during assembly |
| TB6612FNG | 4 | Counterfeit failures |

---

## Assembly Budget

| Activity | Time per Unit | 16 Units Total |
|----------|--------------|----------------|
| PCB prep + sensor soldering | 3 hours | 48 hours |
| Chassis printing (not labor) | 6 hours print time | 96 hours print time |
| Chassis assembly + wiring | 4 hours | 64 hours |
| QC checklist execution | 2 hours | 32 hours |
| Rework (estimated 20% units) | 1 hour avg | 16 hours |
| **Total labor** | **~10 hours/unit** | **~160 hours** |
| **Total calendar time** | **2 working days** | **4–5 weeks** (2 assemblers) |

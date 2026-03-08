# JEEVA Environmental Protection Specifications

## Overview

JEEVA robots operate in Indian elder-care environments: hospital wards, nursing homes,
and home settings. Indian environmental conditions are harsher than typical consumer
electronics are designed for.

**Key challenges**: Monsoon humidity, summer heat, dust, insects, occasional drops.

---

## IPX2 Drip Protection (Water Resistance)

**Standard**: IPX2 — protection against dripping water when tilted up to 15°
**Cost per unit**: ₹100

### Implementation

| Component | Solution | Cost |
|-----------|---------|------|
| Chassis seams | Medical-grade silicone gaskets (3M 734) | ₹40 |
| Ventilation openings | Acoustic mesh + silicone gasket | ₹20 |
| Cable entry points | Sealed cable pass-throughs | ₹30 |
| USB/power ports | Port covers when not in use | ₹10 |

### Silicone Gasket Specification
- **Material**: Medical-grade silicone (food-safe, no off-gassing near patient)
- **Hardness**: Shore A 50 (compresses to seal under chassis screws)
- **Supplier**: Amazon India (₹150 for 1m strip)
- **Application**: Bead around all chassis splits before assembly

### Ventilation Protection
```
Top vent: 0.5mm acoustic mesh + silicone bead around perimeter
Bottom intake: Mesh + 2mm foam gasket (replaces open slot)
Speaker port: See Acoustic Design doc (separate acoustic mesh)
```

---

## Monsoon Humidity Protection (85–95% RH)

Indian monsoon season (June–September) brings 85–95% relative humidity.
Unprotected electronics corrode within 1–2 monsoon seasons.

### Conformal Coating
- **Product**: Electrolube HPA (Humidity Protection Acrylic)
- **Application**: Spray all PCBs with 2 coats after soldering
- **Coverage**: All PCBs — Jetson carrier, ESP32, sensor boards, power board
- **Exclusions**: Connectors, fan heatsink mounting area
- **Cost**: ₹16/unit (400ml can covers ~25 units)
- **Drying**: 30 minutes at room temperature between coats

```bash
# QC step: Check conformal coating under UV light
# Electrolube HPA fluoresces blue-white under 365nm UV
# Verify no bare copper visible
```

### Silica Gel Desiccants
- **Placement**: 2× 5g silica gel pouches inside sealed chassis
- **Location**: Near battery (high heat area) + near PCB stack
- **Cost**: ₹20/unit (₹800 for 40g bag = ~80 pouches at 1g per pouch)
- **Replacement**: Every 6 months (or when indicator turns pink)
- **Type**: Orange indicator silica gel (safe, cobalt-free)

---

## Indian Summer Heat Protection (42–48°C Ambient)

Jetson Orin Nano throttles at 85°C junction temperature. In 45°C ambient with a
poorly-ventilated chassis, this is easily reached.

### Thermal Management

| Component | Solution | Cost |
|-----------|---------|------|
| Jetson heatsink → chassis | Thermal pad (3.0W/mK, 1mm) | ₹30 |
| Battery charging cutoff | Halt charging if ambient > 45°C | Software |
| Fan control | PWM speed based on Jetson temp sensor | Software |

### Thermal Pad Specification
- **Product**: Bergquist GP3000S30 or equivalent
- **Thickness**: 1 mm (compressible to 0.5 mm under pressure)
- **Conductivity**: 3.0 W/mK minimum
- **Size**: 40mm × 40mm (cut to fit Jetson heatsink base)
- **Purpose**: Transfers Jetson heat to PETG chassis body (acts as large heatsink)

### Battery Temperature Protection
```python
# jeeva/core/battery_manager.py
MAX_CHARGE_TEMP_C = 45.0
MAX_DISCHARGE_TEMP_C = 60.0

def battery_temperature_guard(temp_c: float) -> None:
    if temp_c > MAX_CHARGE_TEMP_C:
        disable_charging()
        log_warning(f"Charging halted: temp {temp_c}°C > {MAX_CHARGE_TEMP_C}°C limit")
    if temp_c > MAX_DISCHARGE_TEMP_C:
        initiate_safe_shutdown()
        log_critical(f"Emergency shutdown: battery temp {temp_c}°C")
```

### Fan Control Strategy
```python
# Adaptive fan speed based on Jetson junction temperature
TEMP_TO_FAN_SPEED = {
    (0, 50): 30,    # 30% PWM — silent for patient comfort
    (50, 65): 50,   # 50% PWM — moderate
    (65, 75): 75,   # 75% PWM — active cooling
    (75, 85): 100,  # 100% PWM — maximum
}
```

---

## Dust Protection

Indian homes and hospital wards accumulate fine dust. Dust ingress into fans
causes bearing failure; dust on camera lens causes recognition errors.

### Intake Vent Mesh
- **Mesh size**: 0.5 mm nylon mesh (blocks most construction dust)
- **Mounting**: Heat-staked into PETG chassis (no separate frame needed)
- **Cost**: ₹20/unit (nylon mesh sheet, 0.5mm)

### Maintenance Schedule
| Task | Frequency | Method |
|------|-----------|--------|
| Intake vent cleaning | Monthly | Compressed air (3s burst) |
| Camera lens cleaning | Weekly | Dry microfiber cloth |
| Overall chassis wipe | Weekly | Damp cloth (no solvents) |
| Silica gel check | Every 6 months | Visual indicator check |

### Camera Lens Cover
- Retractable PETG cover over camera during robot sleep/dock mode
- Servo-actuated (uses existing Pan servo channel during idle)
- Prevents dust accumulation during 8–10 hours of dock time overnight

---

## Insect Protection

Indian elder-care facilities have cockroaches and small insects.
Insects entering through speaker ports or mic openings cause damage and
can be detected as acoustic noise by the microphone.

### Speaker Port Protection
- **Material**: Acoustic mesh (0.2 mm mesh, high acoustic transparency)
- **Mounting**: Snaps into speaker grille recess
- **Cost**: ₹20/unit
- **Acoustic impact**: < 0.5 dB attenuation (negligible)

### Microphone Port Protection
- **Material**: Foam gasket around INMP441 mic cavity
- **Purpose**: Blocks insects while allowing sound through foam pores
- **Foam spec**: 10 ppi acoustic foam, 3mm thick
- **Cost**: ₹10/unit

### Cable Entry Points
- **Method**: Nylon cable glands (PG7 size) for all external cable penetrations
- **IP rating**: IP68 when tightened
- **Cost**: ₹30/unit (3 cable glands × ₹10 each)
- **Application**: Power input cable, any wall-mounted sensor cables

---

## Drop Protection

Elder-care robots get bumped into by patients, family members, and cleaning staff.
A 1m drop onto tile floor is a realistic scenario.

### Internal Foam Padding
- **Target**: Display (most fragile component)
- **Material**: 10 mm EVA foam padding around display edges
- **Mounting**: Self-adhesive backing onto PETG inner wall
- **Cost**: ₹15/unit
- **Protection**: Absorbs vibration from drops, prevents display PCB flex

### Drop Test Protocol
**Requirement**: Pass 1 m drop test in 6 orientations.

```
Test sequence (all onto ceramic tile floor):
1. Front face down
2. Back face down
3. Left side down
4. Right side down
5. Top (head) down
6. Bottom down

Pass criteria:
- Robot powers on after each drop
- No cracked display
- No broken structural PETG (cosmetic marks acceptable)
- All sensors functional (SpO2, camera, distance sensors)
- Voice interaction works
```

---

## Environmental Protection Cost Summary

| Protection | Solution | Cost/Unit |
|------------|---------|-----------|
| IPX2 (drip) | Silicone gaskets + sealed pass-throughs | ₹100 |
| Humidity | Conformal coating | ₹16 |
| Humidity | Silica gel desiccants | ₹20 |
| Heat | Thermal pad (Jetson) | ₹30 |
| Dust | 0.5mm nylon mesh vents | ₹20 |
| Insects (speaker) | Acoustic mesh | ₹20 |
| Insects (mic) | Foam gaskets | ₹10 |
| Insects (cables) | Cable glands | ₹30 |
| Drop | EVA foam (display) | ₹15 |
| **Total** | | **₹261/unit** |

*Rounded to ₹236/unit in v5.3 BOM (some items shared with other budgets)*

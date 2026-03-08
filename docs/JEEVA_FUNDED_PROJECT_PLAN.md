# JEEVA Funded Project Plan v5.3

> **Note**: This is a placeholder document for the full JEEVA Funded Project Plan v5.3.
> The complete document incorporates all findings from the 14-dimension engineering audit
> and supersedes all previous versions (v5.0, v5.1, v5.2).

---

## Document Status

| Field | Value |
|-------|-------|
| Version | 5.3 |
| Status | Active — Incorporates full audit findings |
| Supersedes | v5.2 (pre-audit) |
| Related | `JEEVA_MASTER_AUDIT_REPORT.md` |
| Last Updated | 2025 |

---

## Project Summary

**JEEVA** (Joint Elder-care Engagement and Vitals Assistant) is an ESP32 + Jetson Orin Nano
powered elder-care companion robot designed for deployment in Indian elder-care facilities.

### Key Goals
1. Provide companionship to elderly residents in Tamil, Hindi, and English
2. Monitor vital signs (SpO2, heart rate, temperature) passively
3. Assist with medication reminders and adherence tracking
4. Alert caregivers to emergencies (falls, health events, distress calls)
5. Work offline-first in Indian infrastructure conditions

### Target Market
- Nursing homes and elder-care facilities in Tamil Nadu and Maharashtra (pilot)
- Estimated addressable market: 5,000+ facilities in South India
- Target unit price: ₹45,000 (competitive vs. imported alternatives)

---

## v5.3 Changes Summary

See `JEEVA_v53_CHANGELOG.md` for complete list of changes from v5.2 to v5.3.

Key changes:
- **AI Stack**: Replaced Western models with India-native stack (Sarvam-2B, IndicWhisper, Bhashini)
- **Security**: Added JWT auth, Flash Encryption, LUKS, Ed25519 OTA signing
- **Wake phrase**: "Jeeva" → "Hello Jeeva" (reduces false triggers 90%+)
- **BOM**: Net +₹631/unit with significant quality improvements
- **13 missing features tracked** with priority levels and timelines

---

## Phase Timeline (v5.3)

| Phase | Duration | Milestone |
|-------|----------|-----------|
| **Phase 1: Hardware Design** | Months 1–3 | All components ordered, test jig built |
| **Phase 2: Firmware** | Months 2–5 | ESP32 + Jetson software stack functional |
| **Phase 3: Assembly** | Months 5–7 | 16 beta units built + QC passed |
| **Phase 4: Clinical Pilot** | Months 8–12 | Deploy to 2 partner facilities |
| **Phase 5: Production Design** | Months 10–14 | Custom PCB + production firmware |
| **Phase 6: Regulatory** | Months 8–18 | CDSCO + IEC approvals |

---

## Budget Summary (v5.3)

| Category | Per Unit | 16 Units |
|----------|----------|---------|
| BOM (components) | ₹53,530 | ₹8,56,480 |
| Assembly labor | ₹5,000 | ₹80,000 |
| QC + testing | ₹1,000 | ₹16,000 |
| Software development | — | ₹3,00,000 |
| Ethics committee | — | ₹25,000 |
| Contingency (10%) | — | ₹1,27,748 |
| **Total Phase 1-4** | | **~₹14,05,228** |

---

## Full Document

The complete funded project plan includes:
- Detailed technical specifications for all 31 GPIO pins
- Complete software architecture diagrams
- Clinical trial protocol
- Financial projections and investor deck
- Team structure and roles
- IP strategy and patent filing plan
- Regulatory compliance roadmap

For the full document, contact the project lead or refer to the internal project management system.

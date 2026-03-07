# JEEVA Security Architecture

## Overview

JEEVA handles sensitive health data of elderly residents. Security is non-negotiable:
violations could expose private health information and violate DPDP Act 2023.

This document covers all security layers from API to physical hardware.

---

## API Security (Node.js / FastAPI Server)

### Authentication: JWT

```python
# FastAPI with OAuth2 + JWT (replace Flask)
from fastapi.security import OAuth2PasswordBearer
from jose import JWTError, jwt
from datetime import datetime, timedelta

SECRET_KEY = os.environ["JWT_SECRET"]  # 32+ random chars, NEVER hardcoded
ALGORITHM = "HS256"
ACCESS_TOKEN_EXPIRE_MINUTES = 60

def create_access_token(data: dict) -> str:
    expire = datetime.utcnow() + timedelta(minutes=ACCESS_TOKEN_EXPIRE_MINUTES)
    return jwt.encode({**data, "exp": expire}, SECRET_KEY, algorithm=ALGORITHM)
```

### Device Pairing via QR Code
1. Robot displays QR code on screen at first boot
2. Caregiver scans QR with hospital app
3. QR contains one-time pairing token (expires in 10 minutes)
4. Successful pairing stores device certificate locally

### HTTPS (Self-Signed for LAN)
```bash
# Generate self-signed cert for local network (valid 10 years)
openssl req -x509 -newkey rsa:4096 -keyout /etc/jeeva/server.key \
  -out /etc/jeeva/server.crt -days 3650 -nodes \
  -subj "/CN=jeeva-robot-$(hostname)"
```

### Rate Limiting
```python
from slowapi import Limiter
limiter = Limiter(key_func=get_remote_address)

@app.post("/api/chat")
@limiter.limit("30/minute")  # Prevent abuse
async def chat(request: Request, ...):
    ...
```

### CORS (Restricted to Paired Devices Only)
```python
app.add_middleware(
    CORSMiddleware,
    allow_origins=get_paired_device_origins(),  # Dynamic, not "*"
    allow_credentials=True,
    allow_methods=["GET", "POST"],
    allow_headers=["Authorization", "Content-Type"],
)
```

---

## ESP32-S3 Security

### Flash Encryption (Development Mode)
- Encrypts flash contents with AES-256
- Prevents reading firmware from flash chip with external programmer
- Enable: `idf.py --port /dev/ttyUSB0 flash-encryption-generate-key`

### Secure Boot v2 (RSA-3072)
- Verifies bootloader signature before execution
- Prevents replacement of firmware with malicious code
- RSA-3072 key pair generated once, public key burned to eFuse

```bash
# Generate RSA-3072 secure boot key (do once, store private key securely)
espsecure.py generate_signing_key --version 2 secure_boot_signing_key.pem

# Sign firmware
espsecure.py sign_data --version 2 --keyfile secure_boot_signing_key.pem \
  firmware.bin --output firmware_signed.bin
```

### WiFi Credentials in NVS Encrypted Partition
**NEVER hardcode WiFi credentials in `config.h`.**

```c
// esp32/src/wifi_manager.cpp — Read from NVS encrypted partition
#include <nvs_flash.h>
#include <nvs.h>

void load_wifi_credentials(char* ssid, char* password) {
    nvs_handle_t nvs_handle;
    nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    size_t ssid_len = 32, pass_len = 64;
    nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);
    nvs_get_str(nvs_handle, "password", password, &pass_len);
    nvs_close(nvs_handle);
}
```

---

## Jetson Security

### Cython Compilation of Core Modules
```python
# Compile Python to .so (prevents casual reverse engineering)
# setup.py
from Cython.Build import cythonize
ext_modules = cythonize([
    "jeeva/core/health_monitor.py",
    "jeeva/core/medication_rag.py",
    "jeeva/core/face_recognition.py",
])
```

### LUKS Full-Disk Encryption
```bash
# During Jetson setup (one-time)
# Encrypt the data partition (not root — would require password at boot)
cryptsetup luksFormat /dev/nvme0n1p4  # Data partition
cryptsetup luksOpen /dev/nvme0n1p4 jeeva_data
mkfs.ext4 /dev/mapper/jeeva_data
# Auto-unlock with keyfile stored in TPM (if available) or /boot (secured)
```

### SSH Key-Based Auth Only
```bash
# Disable password SSH
sed -i 's/#PasswordAuthentication yes/PasswordAuthentication no/' /etc/ssh/sshd_config
sed -i 's/PermitRootLogin yes/PermitRootLogin no/' /etc/ssh/sshd_config
systemctl restart sshd
```

---

## OTA (Over-the-Air) Update Security

### Ed25519 Code Signing
```c
// ESP32 firmware — verify OTA update signature before applying
#include "mbedtls/eddsa.h"

// Public key hardcoded in firmware (32 bytes, not secret)
static const uint8_t OTA_PUBLIC_KEY[32] = {
    // Generated from: openssl genpkey -algorithm ed25519
    // ed25519 public key bytes here
    0xAB, 0xCD, /* ... */ 0xEF
};

bool verify_ota_signature(const uint8_t* firmware, size_t len, const uint8_t* sig) {
    return mbedtls_eddsa_verify(firmware, len, sig, OTA_PUBLIC_KEY) == 0;
}
```

### OTA Update Flow
```
1. Server signs firmware.bin with Ed25519 private key
2. ESP32 downloads firmware.bin + signature.bin
3. ESP32 verifies signature with hardcoded public key
4. If VALID → apply update
5. If INVALID → reject, log security event, alert admin
```

---

## DPDP Act 2023 Compliance

India's Digital Personal Data Protection Act 2023 requires explicit consent for
health data collection. JEEVA must comply before any clinical deployment.

### First-Boot Consent Screen
```python
CONSENT_TEXT = {
    "ta": "இந்த சாதனம் உங்கள் உடல்நல தரவை சேகரிக்கும். ஒப்புக்கொள்கிறீர்களா?",
    "hi": "यह डिवाइस आपका स्वास्थ्य डेटा एकत्र करेगा। क्या आप सहमत हैं?",
    "en": "This device will collect your health data. Do you consent?"
}
# Consent must be explicit (button press), not assumed
# Consent record stored with timestamp and resident ID
```

### "Delete All My Data" Function
```python
@app.delete("/api/resident/{resident_id}/data")
async def delete_all_data(resident_id: str, current_user: dict = Depends(get_admin_user)):
    db.execute("DELETE FROM health_records WHERE resident_id = ?", (resident_id,))
    db.execute("DELETE FROM conversations WHERE resident_id = ?", (resident_id,))
    db.execute("DELETE FROM face_embeddings WHERE resident_id = ?", (resident_id,))
    db.execute("DELETE FROM consent_records WHERE resident_id = ?", (resident_id,))
    os.remove(f"/opt/jeeva/data/audio_logs/{resident_id}/")
    audit_log(f"All data deleted for resident {resident_id} by {current_user['id']}")
```

### Data Retention Policy
| Data Type | Retention | Reason |
|-----------|-----------|--------|
| Conversation logs | 90 days | Medication compliance tracking |
| Health vitals (SpO2, temp) | 1 year | Trend analysis |
| Face embeddings | Until discharge | Recognition only |
| Audio recordings | 24 hours | QA only, then auto-delete |
| Consent records | Permanent | Legal requirement |

### Audit Logging
```python
def audit_log(event: str, user: str = "system", data: dict = None):
    log_entry = {
        "timestamp": datetime.utcnow().isoformat(),
        "event": event,
        "user": user,
        "data": data or {}
    }
    # Append-only log (cannot delete individual entries)
    with open("/opt/jeeva/logs/audit.jsonl", "a") as f:
        f.write(json.dumps(log_entry) + "\n")
```

---

## Physical Security

### Tamper-Evident Measures
| Measure | Purpose | Cost |
|---------|---------|------|
| Tamper-evident stickers on screws | Detect physical opening | ₹10/unit |
| Epoxy on NVMe screw | Prevent storage removal | ₹2/unit |
| Serial number tracking | Unit accountability | Label cost only |

### Unit Tracking
- Each unit: `JEEVA-BETA-001` through `JEEVA-BETA-016`
- Serial number sticker on base + laser-engraved on chassis
- Facility assignment recorded in central database
- Chain of custody log maintained

---

## Secrets Management

### Development
```bash
# Always use .env file (never commit to git)
cp .env.example .env
nano .env  # Fill in real values
```

### Production (Jetson)
```python
# /opt/jeeva/config.py
from dotenv import load_dotenv
load_dotenv("/etc/jeeva/production.env")  # System-level, not in git repo

GROQ_API_KEY = os.environ["GROQ_API_KEY"]
BHASHINI_API_KEY = os.environ["BHASHINI_API_KEY"]
JWT_SECRET = os.environ["JWT_SECRET"]
```

### ESP32
```c
// WiFi/server credentials: NVS encrypted partition ONLY
// Provisioned via BLE pairing at first boot
// Never in config.h, never in compiled binary (plaintext)
```

### What NEVER Goes in Git
- `.env` files
- `*.pem`, `*.key` (private keys)
- WiFi passwords
- API keys
- JWT secrets
- Patient health data
- Face embedding databases
- Audit logs

See `.gitignore` at repository root for the complete exclusion list.

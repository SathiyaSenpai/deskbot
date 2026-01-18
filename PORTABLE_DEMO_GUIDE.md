# 📱 Running EMO Server on Phone (Termux)

This guide shows how to run the EMO server on your Android phone for **portable demos** without a laptop!

## Prerequisites

1. **Termux** - Install from F-Droid (NOT Play Store - outdated!)
   - https://f-droid.org/packages/com.termux/

2. **Termux:API** (optional) - For notifications
   - https://f-droid.org/packages/com.termux.api/

---

## One-Time Setup (5 minutes)

### 1. Install Node.js in Termux

```bash
# Update packages
pkg update && pkg upgrade

# Install Node.js
pkg install nodejs

# Verify installation
node --version
npm --version
```

### 2. Copy Server Files to Phone

**Option A: Via USB**
```bash
# Connect phone to PC, copy folder to:
/sdcard/Documents/emo-server/
```

**Option B: Via Git (if you upload to GitHub)**
```bash
pkg install git
git clone https://github.com/yourusername/emo-deskbot.git
cd emo-deskbot/server
```

**Option C: Via Termux storage**
```bash
# Grant storage permission
termux-setup-storage

# Navigate to your files
cd ~/storage/shared/Documents/emo-server/
```

### 3. Install Dependencies

```bash
cd ~/storage/shared/Documents/emo-server/
npm install
```

### 4. Install Edge TTS (Voice Reply!) 🎤

Edge TTS uses Microsoft's FREE text-to-speech - great quality, works on phone!

```bash
# Install Python and pip (if needed)
pkg install python

# Install edge-tts
pip install edge-tts

# Test it works
edge-tts --text "Hello! I am EMO!" --write-media test.mp3

# Play the test (optional)
pkg install play-audio
play-audio test.mp3
```

**Available Indian Voices:**
- `en-IN-NeerjaNeural` - Indian English Female (default)
- `en-IN-PrabhatNeural` - Indian English Male
- `ta-IN-PallaviNeural` - Tamil Female
- `ta-IN-ValluvarNeural` - Tamil Male

---

## Running the Server

### Start Server
```bash
cd ~/storage/shared/Documents/emo-server/
node server.js
```

You'll see:
```
🤖 EMO Deskbot Server Started!
================================
   Full UI:  http://localhost:3000
   Mobile:   http://localhost:3000/mobile
```

### Find Phone's IP Address

```bash
# In Termux
ifconfig wlan0 | grep "inet "
```

Or check in **Settings → About Phone → IP Address**

When running hotspot, IP is usually: `192.168.43.1`

---

## Demo Flow (At College/Seminar)

### Setup (2 minutes)

1. **Turn on Phone Hotspot**
   - Settings → Hotspot → Enable
   - Name: `OnePlus Nord 4`
   - Password: `123456789`

2. **Start Server in Termux**
   ```bash
   cd ~/storage/shared/Documents/emo-server && node server.js
   ```

3. **Power on ESP32**
   - Robot connects to your hotspot
   - Robot connects to server (192.168.43.1:3000)

4. **Open Browser**
   - On same phone: `http://localhost:3000`
   - Or use teacher's phone: `http://192.168.43.1:3000`

### That's it! Full control from phone!

---

## Troubleshooting

### "Permission denied"
```bash
termux-setup-storage
```

### "Address already in use"
```bash
pkill node
node server.js
```

### ESP32 won't connect
- Make sure hotspot is ON
- Check ESP32 serial monitor: `pio device monitor`
- Touch sensor for 5 seconds to reset WiFi config

### Server crashes
```bash
# Run with auto-restart
while true; do node server.js; sleep 2; done
```

---

## Pro Tips

### Run in Background
```bash
# Start server in background
nohup node server.js > server.log 2>&1 &

# Check if running
ps aux | grep node

# View logs
tail -f server.log

# Stop server
pkill node
```

### Auto-start on Termux Open
Add to `~/.bashrc`:
```bash
echo 'cd ~/storage/shared/Documents/emo-server && node server.js' >> ~/.bashrc
```

### Battery Saving
- Keep phone plugged in during demos
- Reduce server ping interval in config
- Disable unused sensors on ESP32

---

## What Works on Phone Server

| Feature | Status |
|---------|--------|
| WebSocket | ✅ |
| Web UI | ✅ |
| Expression Control | ✅ |
| LED Control | ✅ |
| Servo Control | ✅ |
| Sensor Display | ✅ |
| Chat (fallback) | ✅ |
| Chat (Gemini) | ✅ (with API key) |
| **Edge TTS (Voice)** | ✅ (install pip edge-tts) |
| Piper TTS | ❌ (x86 only) |

**Voice Reply on Phone:** Edge TTS works great on Termux! Just install with `pip install edge-tts`

#!/bin/bash
# ============================================================
# Nisya Offline AI Stack Setup — Arch Linux
# Hardware: R5 5500 + RX 7600 (8GB VRAM, ROCm) + 16GB RAM
#
# Stack:
#   LLM : Ollama + qwen3:8b (RX 7600 GPU via ROCm)
#   STT : whisper.cpp HIP    (RX 7600 GPU via ROCm)
#   TTS : Kokoro-82M ONNX    (CPU, R5 5500)
# ============================================================

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "\n${GREEN}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  Nisya AI Stack — Arch Linux + RX 7600 (ROCm) Setup  ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════╝${NC}\n"

# ─── Check we're on Arch ──────────────────────────────────────────────────────
if ! command -v pacman &> /dev/null; then
    echo -e "${RED}✗ This script is for Arch Linux (uses pacman). Adapt for your distro.${NC}"
    exit 1
fi

# ─── 0. ROCm Base (needed for GPU) ────────────────────────────────────────────
echo -e "${YELLOW}[0/4] Installing ROCm base packages...${NC}"
sudo pacman -S --needed --noconfirm rocm-hip-sdk rocminfo

echo -e "  → Adding $USER to video and render groups..."
sudo usermod -aG video,render "$USER"

echo -e "  → Verifying RX 7600 detection..."
if rocminfo 2>/dev/null | grep -q "gfx11"; then
    echo -e "  ${GREEN}✓ RX 7600 (gfx11xx) detected by ROCm!${NC}"
else
    echo -e "  ${YELLOW}⚠ ROCm may not detect GPU yet. Try rebooting after setup.${NC}"
fi

# ─── 1. whisper.cpp with HIP/ROCm ─────────────────────────────────────────────
echo -e "\n${YELLOW}[1/4] Installing whisper.cpp with ROCm (HIP) support...${NC}"

if command -v whisper-cpp &> /dev/null; then
    echo -e "  ${GREEN}✓ whisper-cpp already installed!${NC}"
else
    # Install the ROCm-enabled package from Arch repos
    sudo pacman -S --needed --noconfirm whisper-cpp-rocm || {
        echo -e "  ${YELLOW}⚠ whisper-cpp-rocm not in repos, trying AUR...${NC}"
        # Try AUR if pacman fails (some Arch derivatives)
        if command -v yay &> /dev/null; then
            yay -S --needed --noconfirm whisper-cpp-rocm
        elif command -v paru &> /dev/null; then
            paru -S --needed --noconfirm whisper-cpp-rocm
        else
            echo -e "  ${RED}✗ Install manually: https://github.com/ggerganov/whisper.cpp${NC}"
            echo -e "  ${YELLOW}  Build with: WHISPER_HIPBLAS=1 make -j$(nproc)${NC}"
        fi
    }
fi

echo -e "  → Downloading Whisper medium.en model (~150MB)..."
WHISPER_MODEL_DIR="$HOME/.cache/whisper"
mkdir -p "$WHISPER_MODEL_DIR"

if [ -f "$WHISPER_MODEL_DIR/ggml-medium.en.bin" ]; then
    echo -e "  ${GREEN}✓ ggml-medium.en.bin already downloaded!${NC}"
else
    if command -v whisper-cpp-download-ggml-model &> /dev/null; then
        whisper-cpp-download-ggml-model medium.en
    else
        # Direct download fallback
        wget -O "$WHISPER_MODEL_DIR/ggml-medium.en.bin" \
            "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.en.bin" \
            --show-progress
    fi
    echo -e "  ${GREEN}✓ Whisper medium.en model downloaded!${NC}"
fi

# Quick test
echo -e "  → Testing whisper-cpp..."
whisper-cpp --version 2>/dev/null && echo -e "  ${GREEN}✓ whisper-cpp OK${NC}" || echo -e "  ${YELLOW}⚠ whisper-cpp test skipped${NC}"

# ─── 2. Ollama + qwen3:8b ─────────────────────────────────────────────────────
echo -e "\n${YELLOW}[2/4] Installing Ollama...${NC}"

if command -v ollama &> /dev/null; then
    echo -e "  ${GREEN}✓ Ollama already installed: $(ollama --version 2>/dev/null || echo 'unknown version')${NC}"
else
    curl -fsSL https://ollama.com/install.sh | sh
    echo -e "  ${GREEN}✓ Ollama installed!${NC}"
fi

echo -e "  → Pulling qwen3:8b (5.2GB VRAM, ~45 tok/s on RX 7600)..."
echo -e "  ${YELLOW}  Note: qwen3:8b download is ~5GB. This will take a while.${NC}"
echo -e "  ${YELLOW}  ROCm override: HSA_OVERRIDE_GFX_VERSION=11.0.0${NC}"

# Start ollama with ROCm override
export HSA_OVERRIDE_GFX_VERSION=11.0.0
ollama serve &> /tmp/ollama_setup.log &
OLLAMA_PID=$!
sleep 4

if ollama list 2>/dev/null | grep -q "qwen3:8b"; then
    echo -e "  ${GREEN}✓ qwen3:8b already pulled!${NC}"
else
    ollama pull qwen3:8b && echo -e "  ${GREEN}✓ qwen3:8b pulled!${NC}" || {
        echo -e "  ${YELLOW}⚠ Pull failed. Run manually:${NC}"
        echo -e "     HSA_OVERRIDE_GFX_VERSION=11.0.0 ollama serve"
        echo -e "     ollama pull qwen3:8b"
    }
fi

kill $OLLAMA_PID 2>/dev/null || true
wait $OLLAMA_PID 2>/dev/null || true

# ─── 3. Kokoro-82M TTS ────────────────────────────────────────────────────────
echo -e "\n${YELLOW}[3/4] Installing Kokoro-82M TTS...${NC}"

if python3 -c "import kokoro_onnx" 2>/dev/null; then
    echo -e "  ${GREEN}✓ kokoro-onnx already installed!${NC}"
else
    pip install kokoro-onnx soundfile --quiet
    echo -e "  ${GREEN}✓ kokoro-onnx installed!${NC}"
fi

KOKORO_DIR="$HOME/.local/share/kokoro"
mkdir -p "$KOKORO_DIR"

if [ -f "$KOKORO_DIR/kokoro-v1.0.onnx" ] && [ -f "$KOKORO_DIR/voices-v1.0.bin" ]; then
    echo -e "  ${GREEN}✓ Kokoro model files already downloaded!${NC}"
else
    echo -e "  → Downloading Kokoro model files (~300MB)..."

    # Try huggingface-cli if available, else curl
    if python3 -c "from huggingface_hub import hf_hub_download" 2>/dev/null; then
        python3 -c "
from huggingface_hub import hf_hub_download
import os, shutil

files = ['kokoro-v1.0.onnx', 'voices-v1.0.bin']
for f in files:
    print(f'  Downloading {f}...')
    src = hf_hub_download('kokoro-org/kokoro-v1.0', f)
    dst = os.path.join(os.path.expanduser('~/.local/share/kokoro'), f)
    shutil.copy2(src, dst)
    print(f'  → Saved to {dst}')
print('Done!')
"
    else
        pip install huggingface-hub --quiet
        python3 -c "
from huggingface_hub import hf_hub_download
import shutil, os
for f in ['kokoro-v1.0.onnx', 'voices-v1.0.bin']:
    src = hf_hub_download('kokoro-org/kokoro-v1.0', f)
    dst = os.path.join(os.path.expanduser('~/.local/share/kokoro'), f)
    shutil.copy2(src, dst)
    print(f'  ✓ {f}')
"
    fi
    echo -e "  ${GREEN}✓ Kokoro model files downloaded!${NC}"
fi

# Test Kokoro
echo -e "  → Testing Kokoro TTS..."
python3 -c "
import soundfile as sf, tempfile, os
from kokoro_onnx import Kokoro
import os.path
model = os.path.expanduser('~/.local/share/kokoro/kokoro-v1.0.onnx')
voices = os.path.expanduser('~/.local/share/kokoro/voices-v1.0.bin')
k = Kokoro(model, voices)
samples, sr = k.create('Hello! I am Nisya!', voice='af_heart', lang='en-us')
sf.write('/tmp/nisya_test.wav', samples, sr)
print('  ✓ TTS test OK: /tmp/nisya_test.wav')
" && aplay /tmp/nisya_test.wav 2>/dev/null || echo -e "  ${YELLOW}  (no audio output device in terminal, file at /tmp/nisya_test.wav)${NC}"

# ─── 3.5 Piper TTS (Tamil Voice Model) ────────────────────────────────────────
echo -e "\n${YELLOW}[3.5/4] Installing Piper TTS and Tamil Voice Model (Roja)...${NC}"

if command -v piper &> /dev/null || python3 -c "import piper" 2>/dev/null; then
    echo -e "  ${GREEN}✓ piper-tts already installed!${NC}"
else
    pip install piper-tts --quiet
    echo -e "  ${GREEN}✓ piper-tts installed!${NC}"
fi

PIPER_DIR="$HOME/.local/share/piper"
mkdir -p "$PIPER_DIR"

if [ -f "$PIPER_DIR/ta_IN-roja-medium.onnx" ] && [ -f "$PIPER_DIR/ta_IN-roja-medium.onnx.json" ]; then
    echo -e "  ${GREEN}✓ Tamil Piper model (Roja) already downloaded!${NC}"
else
    echo -e "  → Downloading Tamil Piper model (Roja ~64MB)..."
    python3 -c "
from huggingface_hub import hf_hub_download
import shutil, os
for f in ['ta_IN-roja-medium.onnx', 'ta_IN-roja-medium.onnx.json']:
    print(f'  Downloading {f}...')
    src = hf_hub_download('ezhilkumaran/piper-tamil', f)
    dst = os.path.join(os.path.expanduser('~/.local/share/piper'), f)
    shutil.copy2(src, dst)
    print(f'  ✓ {f}')
print('Done!')
"
    echo -e "  ${GREEN}✓ Tamil Piper model downloaded!${NC}"
fi

# Test Piper Tamil
echo -e "  → Testing Piper Tamil TTS..."
echo "வணக்கம்! நான் நிஸ்யா, உங்கள் நண்பன்!" | piper --model "$PIPER_DIR/ta_IN-roja-medium.onnx" --output_file /tmp/nisya_tamil_test.wav 2>/dev/null && \
    echo -e "  ${GREEN}✓ Tamil TTS test OK: /tmp/nisya_tamil_test.wav${NC}" || \
    echo -e "  ${YELLOW}⚠ Piper test skipped (check if piper binary is in PATH)${NC}"

# ─── 4. Final summary ─────────────────────────────────────────────────────────
echo -e "\n${GREEN}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║                ✅ Setup Complete!                    ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════╝${NC}"
echo -e ""
echo -e "${YELLOW}IMPORTANT: Log out and back in for GPU group changes to take effect!${NC}"
echo -e ""
echo -e "To start the full Nisya system:"
echo -e ""
echo -e "  ${GREEN}Terminal 1${NC} — Start Ollama (RX 7600 GPU):"
echo -e "    ${GREEN}HSA_OVERRIDE_GFX_VERSION=11.0.0 ollama serve${NC}"
echo -e ""
echo -e "  ${GREEN}Terminal 2${NC} — Start Nisya Server:"
echo -e "    ${GREEN}cd server && npm start${NC}"
echo -e ""
echo -e "  ${GREEN}Terminal 3${NC} — Flash ESP32:"
echo -e "    ${GREEN}cd esp32 && pio run -t upload && pio device monitor${NC}"
echo -e ""
echo -e "Check VRAM usage with: ${GREEN}radeontop${NC}"
echo -e "Check Ollama GPU:       ${GREEN}ollama ps${NC}"
echo -e ""
echo -e "Model choices in ai-services.js:"
echo -e "  OLLAMA_MODEL=qwen3:4b  — faster (~90 tok/s), less VRAM"
echo -e "  OLLAMA_MODEL=qwen3:8b  — balanced (~45 tok/s)  ← default"
echo -e "  WHISPER_MODEL=small.en — faster, less accurate"
echo -e "  KOKORO_VOICE=af_sarah  — alternative voice"

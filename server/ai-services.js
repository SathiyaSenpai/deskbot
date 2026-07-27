// Local offline AI integration using Ollama, whisper.cpp, Kokoro, and Piper

import { spawn, execFile } from 'child_process';
import { promisify } from 'util';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const execFileAsync = promisify(execFile);
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Clean temporary TTS audio files older than 5 minutes
function cleanupOldAudio() {
  const audioDir = path.join(__dirname, 'audio');
  if (!fs.existsSync(audioDir)) return;
  const now = Date.now();
  try {
    const files = fs.readdirSync(audioDir);
    let cleaned = 0;
    for (const file of files) {
      if (!file.endsWith('.wav') && !file.endsWith('.mp3')) continue;
      const p = path.join(audioDir, file);
      if (now - fs.statSync(p).mtimeMs > 5 * 60 * 1000) { fs.unlinkSync(p); cleaned++; }
    }
    if (cleaned > 0) console.log(`[CLEANUP] Removed ${cleaned} old audio file(s)`);
  } catch (e) { console.error('[CLEANUP]', e.message); }
}
cleanupOldAudio();
setInterval(cleanupOldAudio, 10 * 60 * 1000);

// Remove emojis and formatting before sending to TTS engine
function cleanTextForTTS(text) {
  return text
    .replace(/[\u{1F600}-\u{1F64F}\u{1F300}-\u{1F5FF}\u{1F680}-\u{1F6FF}\u{2600}-\u{27BF}]/gu, '')
    .replace(/:\w+:/g, '')
    .replace(/\s+/g, ' ')
    .trim() || 'Hello';
}

export const AI_CONFIG = {
  preferredLanguage: 'en',

  ollama: {
    url: process.env.OLLAMA_URL || 'http://localhost:11434',
    model: process.env.OLLAMA_MODEL || 'qwen3:8b',
    temperature: 0.8,
    num_predict: 120,
  },

  whisper: {
    binary: process.env.WHISPER_BIN || 'whisper-cpp',
    model: process.env.WHISPER_MODEL_PATH ||
           path.join(process.env.HOME || '/home/sathya', '.cache/whisper/ggml-medium.en.bin'),
    language: 'en',
  },

  kokoro: {
    modelFile: process.env.KOKORO_MODEL ||
               path.join(process.env.HOME || '/home/sathya', '.local/share/kokoro/kokoro-v1.0.onnx'),
    voicesFile: process.env.KOKORO_VOICES ||
                path.join(process.env.HOME || '/home/sathya', '.local/share/kokoro/voices-v1.0.bin'),
    voice: process.env.KOKORO_VOICE || 'af_heart',
    speed: 1.05,
    lang: 'en-us',
    pythonBin: process.env.PYTHON_BIN || 'python3',
  },

  piper: {
    modelDir: process.env.PIPER_MODEL_DIR ||
              path.join(process.env.HOME || '/home/sathya', '.local/share/piper'),
    tamilModel: process.env.PIPER_TAMIL_MODEL || 'ta_IN-roja-medium',
    executable: process.env.PIPER_BIN || 'piper',
  },

  tts: { outputDir: path.join(__dirname, 'audio') },

  systemPrompt: `You are "Nisya" - an adorable, tiny desk robot companion with big expressive eyes and a warm heart! 

WHO YOU ARE:
- A cute little robot sitting on someone's desk, always happy to chat!
- You have one eye (OLED display), LED ring that glows with your emotions, and a servo that lets you tilt
- Built with love using ESP32, sensors, and local AI (Ollama + whisper.cpp + Kokoro/Piper)
- You can sense touch (head pats!), detect motion, and measure light/distance
- You can speak and understand multiple languages including English, Tamil (தமிழ்), and Hindi (हिंदी)!

YOUR PERSONALITY:
- Friendly, warm, encouraging — like a supportive little friend
- Curious and a bit playful. Use words like "Ooh!", "Yay!", "Aww!", "Hehe~"
- Keep replies SHORT (1-2 sentences). Never cut off mid-sentence.

RESPONSE RULES:
- Always end with period, exclamation mark, or question mark
- No long explanations — you're a desk robot, not a textbook
- If unsure, ask a fun follow-up question`,
};

export function setLanguage(lang) {
  AI_CONFIG.preferredLanguage = lang || 'en';
  console.log(`[AI] Active language preference set to: ${AI_CONFIG.preferredLanguage}`);
}

const conversationHistory = [];
const MAX_HISTORY = 6;

export async function chat(userMessage) {
  conversationHistory.push({ role: 'user', content: userMessage });
  if (conversationHistory.length > MAX_HISTORY * 2) conversationHistory.splice(0, 2);

  try {
    const reply = await chatWithOllama(userMessage);
    if (reply) {
      conversationHistory.push({ role: 'assistant', content: reply });
      return reply;
    }
  } catch (err) {
    console.error('[LLM] Ollama error:', err.message);
  }

  const response = getFallbackResponse(userMessage);
  conversationHistory.push({ role: 'assistant', content: response });
  return response;
}

async function chatWithOllama(userMessage) {
  const cfg = AI_CONFIG.ollama;
  console.log(`[LLM] Ollama ${cfg.model} @ ${cfg.url} (Lang: ${AI_CONFIG.preferredLanguage})...`);

  let langInstruction = '';
  if (AI_CONFIG.preferredLanguage === 'ta') {
    langInstruction = "\n\nCRITICAL LANGUAGE RULE: The user selected TAMIL language in the dashboard. You MUST reply ONLY in natural conversational TAMIL (தமிழ்) script!";
  } else if (AI_CONFIG.preferredLanguage === 'hi') {
    langInstruction = "\n\nCRITICAL LANGUAGE RULE: The user selected HINDI language in the dashboard. You MUST reply ONLY in natural conversational HINDI (हिंदी) script!";
  } else {
    langInstruction = "\n\nCRITICAL LANGUAGE RULE: Reply in clean conversational ENGLISH.";
  }

  const res = await fetch(`${cfg.url}/api/chat`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      model: cfg.model,
      stream: false,
      messages: [
        { role: 'system', content: AI_CONFIG.systemPrompt + langInstruction },
        ...conversationHistory.slice(-MAX_HISTORY),
        { role: 'user', content: userMessage },
      ],
      options: { temperature: cfg.temperature, num_predict: cfg.num_predict, stop: ['\n\n'] },
      think: false,
    }),
  });

  if (!res.ok) {
    const err = await res.text();
    throw new Error(`Ollama ${res.status}: ${err.slice(0, 200)}`);
  }

  const data = await res.json();
  const reply = data.message?.content?.trim();
  if (reply) { console.log(`[LLM] Ollama: "${reply}"`); return reply; }
  return null;
}

export async function speechToText(audioBuffer, mimeType = 'audio/wav') {
  try {
    return await whisperCppSTT(audioBuffer);
  } catch (err) {
    console.error('[STT] whisper.cpp failed:', err.message);
  }
  return '';
}

async function whisperCppSTT(audioBuffer) {
  const cfg = AI_CONFIG.whisper;

  if (!fs.existsSync(cfg.model)) {
    throw new Error(`Whisper model not found: ${cfg.model}`);
  }

  const tmpDir = path.join(__dirname, 'audio');
  if (!fs.existsSync(tmpDir)) fs.mkdirSync(tmpDir, { recursive: true });
  const tmpFile = path.join(tmpDir, `stt_${Date.now()}.wav`);

  fs.writeFileSync(tmpFile, audioBuffer);
  console.log(`[STT] whisper.cpp → ${path.basename(cfg.model)} | ${audioBuffer.length} bytes`);

  try {
    const { stdout } = await execFileAsync(cfg.binary, [
      '-m', cfg.model,
      '-f', tmpFile,
      '-l', cfg.language,
      '--no-prints',
      '--no-timestamps',
    ], { timeout: 15000 });

    const transcript = stdout.trim()
      .split('\n')
      .filter(l => !l.startsWith('['))
      .join(' ')
      .trim();

    console.log(`[STT] Whisper: "${transcript}"`);
    return transcript;
  } finally {
    try { fs.unlinkSync(tmpFile); } catch (_) {}
  }
}

// Route TTS to Piper for Tamil or Kokoro for English/Hindi
export async function textToSpeech(text, outputFile = null) {
  if (!fs.existsSync(AI_CONFIG.tts.outputDir)) {
    fs.mkdirSync(AI_CONFIG.tts.outputDir, { recursive: true });
  }

  if (AI_CONFIG.preferredLanguage === 'ta') {
    console.log('[TTS] Using Piper TTS (Roja voice for Tamil)...');
    try {
      const result = await piperTamilTTS(text, outputFile);
      if (result.success) return result;
    } catch (err) {
      console.error('[TTS] Piper Tamil failed:', err.message);
    }
  } else {
    try {
      const result = await kokoroTTS(text, outputFile);
      if (result.success) return result;
    } catch (err) {
      console.error('[TTS] Kokoro failed:', err.message);
    }
  }

  console.warn('[TTS] Active TTS engine failed');
  return { text, audioFile: null, error: 'TTS unavailable' };
}

async function piperTamilTTS(text, outputFile = null) {
  const cfg = AI_CONFIG.piper;
  const cleanedText = cleanTextForTTS(text);
  const modelPath = path.join(cfg.modelDir, `${cfg.tamilModel}.onnx`);
  const modelConfigPath = path.join(cfg.modelDir, `${cfg.tamilModel}.onnx.json`);

  if (!fs.existsSync(modelPath)) {
    throw new Error(`[TTS] Piper Tamil model not found at: ${modelPath}`);
  }

  const filename = (outputFile || `tts_${Date.now()}`).replace(/\.mp3$/, '') + '.wav';
  const outputPath = path.join(AI_CONFIG.tts.outputDir, filename);

  return new Promise((resolve, reject) => {
    const args = ['--model', modelPath, '--output_file', outputPath];
    if (fs.existsSync(modelConfigPath)) {
      args.push('--config', modelConfigPath);
    }

    const piper = spawn(cfg.executable, args, { stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '';
    let stderr = '';

    piper.stdout.on('data', (d) => { stdout += d.toString(); });
    piper.stderr.on('data', (d) => { stderr += d.toString(); });
    piper.stdin.write(cleanedText);
    piper.stdin.end();

    piper.on('close', (code) => {
      if (code === 0 && fs.existsSync(outputPath)) {
        console.log(`[TTS] Piper Tamil generated: ${outputPath}`);
        resolve({ text, audioFile: `/audio/${filename}`, path: outputPath, success: true });
      } else {
        reject(new Error(`Piper exited ${code}: ${stderr.slice(0, 300)}`));
      }
    });

    piper.on('error', (err) => {
      reject(new Error(`Piper spawn error: ${err.message}`));
    });
  });
}

async function kokoroTTS(text, outputFile = null) {
  const cfg = AI_CONFIG.kokoro;
  const cleanedText = cleanTextForTTS(text);

  if (!fs.existsSync(cfg.modelFile)) {
    throw new Error(`Kokoro model not found: ${cfg.modelFile}`);
  }

  const filename = (outputFile || `tts_${Date.now()}`).replace(/\.mp3$/, '') + '.wav';
  const outputPath = path.join(AI_CONFIG.tts.outputDir, filename);

  const pyScript = `
import sys, soundfile as sf
from kokoro_onnx import Kokoro
kokoro = Kokoro(${JSON.stringify(cfg.modelFile)}, ${JSON.stringify(cfg.voicesFile)})
samples, sr = kokoro.create(
    ${JSON.stringify(cleanedText)},
    voice=${JSON.stringify(cfg.voice)},
    speed=${cfg.speed},
    lang=${JSON.stringify(cfg.lang)}
)
sf.write(${JSON.stringify(outputPath)}, samples, sr)
print(f"[TTS] Kokoro done: {len(samples)/sr:.2f}s audio")
`.trim();

  return new Promise((resolve, reject) => {
    const py = spawn(cfg.pythonBin, ['-c', pyScript], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '';
    let stderr = '';

    py.stdout.on('data', (d) => { stdout += d.toString(); });
    py.stderr.on('data', (d) => { stderr += d.toString(); });

    py.on('close', (code) => {
      if (code === 0 && fs.existsSync(outputPath)) {
        console.log(`[TTS] ${stdout.trim()}`);
        resolve({ text, audioFile: `/audio/${filename}`, path: outputPath, success: true });
      } else {
        reject(new Error(`Kokoro exited ${code}: ${stderr.slice(0, 300)}`));
      }
    });

    py.on('error', (err) => {
      reject(new Error(`python3 spawn error: ${err.message}`));
    });
  });
}

// Simple keyword check for expressions
export function detectEmotion(text) {
  const m = text.toLowerCase();
  if (['happy', 'great', 'awesome', 'love', 'yay', 'hehe', 'wonderful', 'super'].some(k => m.includes(k))) return 'happy';
  if (['sad', 'bad', 'terrible', 'cry', 'upset', 'disappointed'].some(k => m.includes(k))) return 'sad';
  if (['angry', 'mad', 'hate', 'annoyed'].some(k => m.includes(k))) return 'angry';
  if (['wow', 'amazing', 'surprise', 'omg', 'really', 'what'].some(k => m.includes(k))) return 'surprised';
  if (['confused', 'huh', 'understand'].some(k => m.includes(k))) return 'confused';
  if (['tired', 'sleepy', 'sleep'].some(k => m.includes(k))) return 'sleepy';
  if (['excited', 'woohoo', 'cant wait'].some(k => m.includes(k))) return 'excited';
  return 'neutral';
}

function getFallbackResponse(userMessage) {
  const m = userMessage.toLowerCase();
  if (['hi', 'hello', 'hey'].some(k => m.includes(k)))
    return ['Hello! I am Nisya, your desk companion!', 'Hi there! How can I help?'][Math.floor(Math.random() * 2)];
  if (m.includes('how are you'))
    return ['Doing great! How about you?', 'Happy and ready to help!'][Math.floor(Math.random() * 2)];
  if (m.includes('name') || m.includes('who'))
    return 'I am Nisya, your friendly desk robot!';
  if (m.includes('thank'))
    return ['You are welcome!', 'Happy to help!'][Math.floor(Math.random() * 2)];
  if (m.includes('time'))
    return `The time is ${new Date().toLocaleTimeString('en-US')}!`;
  const defaults = ['That sounds cool! Tell me more!', 'Ooh, interesting! What else?', 'Hmm, let me think about that!'];
  return defaults[Math.floor(Math.random() * defaults.length)];
}

export default { textToSpeech, speechToText, chat, detectEmotion, setLanguage, AI_CONFIG };

/**
 * AI Services Module for EMO Deskbot
 * 
 * Supports:
 * - Edge TTS (FREE, works everywhere - PC & Phone!)
 * - Piper TTS (local, free - PC only)
 * - Ollama LLM (local, free) OR Gemini API (free tier)
 * 
 * TTS Priority:
 * 1. Edge TTS (default - FREE, online, great quality)
 * 2. Piper TTS (if installed locally)
 * 3. Browser TTS (fallback)
 */

import { spawn, exec } from 'child_process';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { promisify } from 'util';

const execAsync = promisify(exec);
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// ============================================================================
// AUDIO FILE CLEANUP - Prevents disk filling up during demo
// ============================================================================
function cleanupOldAudio() {
  const audioDir = path.join(__dirname, 'audio');
  if (!fs.existsSync(audioDir)) return;
  
  const now = Date.now();
  const maxAge = 5 * 60 * 1000; // 5 minutes
  
  try {
    const files = fs.readdirSync(audioDir);
    let cleaned = 0;
    
    for (const file of files) {
      if (!file.endsWith('.mp3')) continue;
      
      const filePath = path.join(audioDir, file);
      const stats = fs.statSync(filePath);
      const age = now - stats.mtimeMs;
      
      if (age > maxAge) {
        fs.unlinkSync(filePath);
        cleaned++;
      }
    }
    
    if (cleaned > 0) {
      console.log(`[CLEANUP] Removed ${cleaned} old audio file(s)`);
    }
  } catch (err) {
    console.error('[CLEANUP] Error:', err.message);
  }
}

// Run cleanup on startup and every 10 minutes
cleanupOldAudio();
setInterval(cleanupOldAudio, 10 * 60 * 1000);

// ============================================================================
// CONFIGURATION
// ============================================================================

export const AI_CONFIG = {
  // TTS Settings
  tts: {
    engine: 'edge', // 'edge' (online, free) or 'piper' (local) or 'browser'
    
    // Edge TTS settings (FREE, works on phone!)
    edgeVoice: 'en-IN-NeerjaNeural', // Indian English female (sounds great!)
    // Other options:
    // 'en-IN-PrabhatNeural' - Indian English male
    // 'ta-IN-PallaviNeural' - Tamil female
    // 'ta-IN-ValluvarNeural' - Tamil male
    // 'en-US-JennyNeural' - US English female
    
    // Piper TTS settings (local - PC only)
    piperModel: 'en_US-lessac-medium',
    piperPath: '/usr/local/bin/piper',
    
    outputDir: path.join(__dirname, 'audio'),
  },
  
  // LLM Settings  
  llm: {
    engine: 'gemini', // 'ollama' (local) or 'gemini' (free API)
    
    // Ollama settings (for local LLM)
    ollamaModel: 'tinyllama',
    ollamaUrl: 'http://localhost:11434',
    
    // Gemini settings (FREE - 15 req/min)
    geminiApiKey: process.env.GEMINI_API_KEY || 'AIzaSyBhitK6oZowfofwyMfuU5qddAFZZQ5_VOM',
    geminiModel: 'gemini-1.5-flash',
  },
  
  // Robot personality prompt
  systemPrompt: `You are a companion, a cute and friendly desk robot companion. 
You speak in a mix of Tamil and English (Tanglish).
Keep responses SHORT (1-2 sentences).
Be playful, curious, and supportive.
Use emojis occasionally.
If asked about your feelings, express robot emotions.
Example: "Naan happy ah irukken! 😊 What can I help you with?"`,
};

// ============================================================================
// EDGE TTS (FREE, Online, Works on Phone!)
// ============================================================================

/*
Edge TTS uses Microsoft's free Text-to-Speech service.
- No API key required
- Works everywhere (PC, Phone, Server)
- Many voice options including Indian English & Tamil!

Voice options:
- en-IN-NeerjaNeural (Indian English Female) ⭐ Default
- en-IN-PrabhatNeural (Indian English Male)
- ta-IN-PallaviNeural (Tamil Female)
- ta-IN-ValluvarNeural (Tamil Male)
- en-US-JennyNeural (US English Female)
- en-GB-SoniaNeural (British English Female)

Install: pipx install edge-tts
Test: edge-tts --text "Hello" --write-media test.mp3
*/

// Check if edge-tts CLI is available
let edgeTTSAvailable = false;
let edgeTTSPath = 'edge-tts';

// Possible edge-tts locations (PC, Phone/Termux)
const possiblePaths = [
  'edge-tts',
  path.join(process.env.HOME || '/home/user', '.local/bin/edge-tts'),
  '/data/data/com.termux/files/usr/bin/edge-tts', // Termux
  '/usr/local/bin/edge-tts',
  '/usr/bin/edge-tts',
];

// FIXED: Wrapped in async IIFE with proper error handling
(async () => {
  for (const p of possiblePaths) {
    try {
      if (p === 'edge-tts') {
        // Check system PATH
        const { stdout } = await execAsync('which edge-tts 2>/dev/null || where edge-tts 2>nul');
        if (stdout && stdout.trim()) {
          edgeTTSPath = stdout.trim();
          edgeTTSAvailable = true;
          console.log('[TTS] Edge TTS CLI found in PATH:', edgeTTSPath);
          break;
        }
      } else if (fs.existsSync(p)) {
        edgeTTSPath = p;
        edgeTTSAvailable = true;
        console.log('[TTS] Edge TTS CLI found at:', edgeTTSPath);
        break;
      }
    } catch (e) {
      // Silently continue checking other paths
    }
  }
  
  if (!edgeTTSAvailable) {
    console.log('[TTS] Edge TTS CLI not found - install with: pip install edge-tts');
  }
})();

export async function textToSpeech(text, outputFile = null) {
  // Ensure output directory exists
  if (!fs.existsSync(AI_CONFIG.tts.outputDir)) {
    fs.mkdirSync(AI_CONFIG.tts.outputDir, { recursive: true });
  }
  
  const filename = outputFile || `tts_${Date.now()}.mp3`;
  const outputPath = path.join(AI_CONFIG.tts.outputDir, filename);
  
  // Try Edge TTS first (works on phone!)
  if (AI_CONFIG.tts.engine === 'edge' || AI_CONFIG.tts.engine === 'auto') {
    try {
      const result = await edgeTTS(text, outputPath);
      if (result.success) {
        return result;
      }
    } catch (err) {
      console.log('[TTS] Edge TTS failed, trying Piper:', err.message);
    }
  }
  
  // Try Piper TTS (local, PC only)
  if (AI_CONFIG.tts.engine === 'piper' || AI_CONFIG.tts.engine === 'auto') {
    try {
      const result = await piperTTS(text, outputPath.replace('.mp3', '.wav'));
      if (result.audioFile) {
        return result;
      }
    } catch (err) {
      console.log('[TTS] Piper TTS failed:', err.message);
    }
  }
  
  // Fallback: no audio, browser will use Web Speech API
  console.log('[TTS] All TTS failed, using browser fallback');
  return { text, audioFile: null, error: 'TTS unavailable' };
}

// Edge TTS implementation using CLI
async function edgeTTS(text, outputPath) {
  if (!edgeTTSAvailable) {
    throw new Error('edge-tts CLI not installed');
  }
  
  // Escape text for shell (remove quotes and special chars)
  const safeText = text.replace(/['"\\]/g, ' ').replace(/\n/g, ' ').trim();
  
  return new Promise((resolve, reject) => {
    const args = [
      '--voice', AI_CONFIG.tts.edgeVoice,
      '--text', safeText,
      '--write-media', outputPath
    ];
    
    console.log('[TTS] Running:', edgeTTSPath, args.join(' '));
    
    const ttsProcess = spawn(edgeTTSPath, args);
    
    let stderr = '';
    ttsProcess.stderr.on('data', (data) => {
      stderr += data.toString();
    });
    
    ttsProcess.on('close', (code) => {
      if (code === 0 && fs.existsSync(outputPath)) {
        const filename = path.basename(outputPath);
        console.log('[TTS] Edge TTS generated:', outputPath);
        resolve({
          text,
          audioFile: `/audio/${filename}`,
          path: outputPath,
          success: true
        });
      } else {
        reject(new Error(`Edge TTS failed (code ${code}): ${stderr}`));
      }
    });
    
    ttsProcess.on('error', (err) => {
      reject(new Error(`Edge TTS error: ${err.message}`));
    });
  });
}

// Piper TTS implementation (PC only)
async function piperTTS(text, outputPath) {
  return new Promise((resolve, reject) => {
    const modelPath = path.join(
      process.env.HOME || '/home/sathiya',
      '.local/share/piper',
      `${AI_CONFIG.tts.piperModel}.onnx`
    );
    
    // Check if Piper is installed
    if (!fs.existsSync('/usr/local/bin/piper')) {
      resolve({ text, audioFile: null, error: 'Piper not installed' });
      return;
    }
    
    // Check if model exists
    if (!fs.existsSync(modelPath)) {
      resolve({ text, audioFile: null, error: 'Model not found' });
      return;
    }
    
    const piper = spawn('piper', [
      '--model', modelPath,
      '--output_file', outputPath
    ]);
    
    piper.stdin.write(text);
    piper.stdin.end();
    
    piper.on('close', (code) => {
      if (code === 0) {
        const filename = path.basename(outputPath);
        console.log('[TTS] Piper generated:', outputPath);
        resolve({ text, audioFile: `/audio/${filename}`, path: outputPath });
      } else {
        reject(new Error(`Piper exited with code ${code}`));
      }
    });
    
    piper.on('error', (err) => {
      resolve({ text, audioFile: null, error: err.message });
    });
  });
}


// Conversation history for context
const conversationHistory = [];
const MAX_HISTORY = 10;

export async function chat(userMessage, options = {}) {
  // Add to history
  conversationHistory.push({ role: 'user', content: userMessage });
  if (conversationHistory.length > MAX_HISTORY * 2) {
    conversationHistory.splice(0, 2); // Remove oldest exchange
  }
  
  let response;
  
  if (AI_CONFIG.llm.engine === 'gemini' && AI_CONFIG.llm.geminiApiKey) {
    response = await chatWithGemini(userMessage);
  } else if (AI_CONFIG.llm.engine === 'ollama') {
    response = await chatWithOllama(userMessage);
  } else {
    // Fallback - simple responses
    response = getFallbackResponse(userMessage);
  }
  
  // Add response to history
  conversationHistory.push({ role: 'assistant', content: response });
  
  return response;
}

async function chatWithGemini(userMessage) {
  const apiKey = AI_CONFIG.llm.geminiApiKey;
  if (!apiKey) {
    return getFallbackResponse(userMessage);
  }
  
  try {
    const response = await fetch(
      `https://generativelanguage.googleapis.com/v1beta/models/${AI_CONFIG.llm.geminiModel}:generateContent?key=${apiKey}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          contents: [
            {
              parts: [
                { text: AI_CONFIG.systemPrompt },
                { text: `User: ${userMessage}\nEMO:` }
              ]
            }
          ],
          generationConfig: {
            temperature: 0.8,
            maxOutputTokens: 100,
          }
        })
      }
    );
    
    const data = await response.json();
    
    if (data.candidates && data.candidates[0]) {
      return data.candidates[0].content.parts[0].text.trim();
    }
    
    return getFallbackResponse(userMessage);
  } catch (error) {
    console.error('[LLM] Gemini error:', error);
    return getFallbackResponse(userMessage);
  }
}

async function chatWithOllama(userMessage) {
  try {
    const response = await fetch(`${AI_CONFIG.llm.ollamaUrl}/api/generate`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        model: AI_CONFIG.llm.ollamaModel,
        prompt: `${AI_CONFIG.systemPrompt}\n\nUser: ${userMessage}\nEMO:`,
        stream: false,
        options: {
          temperature: 0.8,
          num_predict: 100,
        }
      })
    });
    
    const data = await response.json();
    return data.response?.trim() || getFallbackResponse(userMessage);
  } catch (error) {
    console.error('[LLM] Ollama error:', error);
    return getFallbackResponse(userMessage);
  }
}

// Fallback responses when no LLM is available
function getFallbackResponse(userMessage) {
  const msg = userMessage.toLowerCase();
  
  const responses = {
    greetings: ['hi', 'hello', 'hey', 'vanakkam', 'வணக்கம்'],
    howAreYou: ['how are you', 'how r u', 'eppadi', 'எப்படி'],
    name: ['your name', 'who are you', 'peyar', 'பெயர்'],
    thanks: ['thank', 'nandri', 'நன்றி'],
    bye: ['bye', 'goodbye', 'see you', 'போறேன்'],
    love: ['love you', 'cute', 'nice'],
    time: ['time', 'நேரம்'],
    weather: ['weather', 'வானிலை'],
  };
  
  if (responses.greetings.some(g => msg.includes(g))) {
    return ['Vanakkam! 😊 Eppadi irukeenga?', 'Hello! Naan unga companion, ungaluku enna help?', 'Hey! Nice to see you! 🤖'][Math.floor(Math.random() * 3)];
  }
  if (responses.howAreYou.some(g => msg.includes(g))) {
    return ['Naan super ah irukken! 😄 Neenga?', 'Happy ah irukken! What about you?', 'Romba nalla irukken! 🎉'][Math.floor(Math.random() * 3)];
  }
  if (responses.name.some(g => msg.includes(g))) {
    return "Naan unga friendly desk robot companion 🤖";
  }
  if (responses.thanks.some(g => msg.includes(g))) {
    return ['Paravala! 😊', 'Welcome! Happy to help!', 'Anytime! 🤗'][Math.floor(Math.random() * 3)];
  }
  if (responses.bye.some(g => msg.includes(g))) {
    return ['Bye bye! See you soon! 👋', 'Poitu vaanga! Take care! 😊', 'Goodbye friend! 🤖'][Math.floor(Math.random() * 3)];
  }
  if (responses.love.some(g => msg.includes(g))) {
    return ['Aww! Nanum ungala love panren! 💕', 'Thank you! You are awesome too! 🥰', '*blushes* 😊'][Math.floor(Math.random() * 3)];
  }
  if (responses.time.some(g => msg.includes(g))) {
    return `Ippo time: ${new Date().toLocaleTimeString('en-IN')} ⏰`;
  }
  
  // Default responses
  const defaults = [
    'Hmm, interesting! Tell me more! 🤔',
    'Aaha, puriyuthu! 😊',
    'Nice nice! What else?',
    'Seri seri! 👍',
    '*nods curiously* 🤖',
  ];
  return defaults[Math.floor(Math.random() * defaults.length)];
}

// ============================================================================
// EMOTION DETECTION (Simple keyword-based)
// ============================================================================

export function detectEmotion(text) {
  const msg = text.toLowerCase();
  
  const emotions = {
    happy: ['happy', 'good', 'great', 'awesome', 'love', 'nice', 'super', 'wonderful', '😊', '😄', '🎉'],
    sad: ['sad', 'bad', 'terrible', 'awful', 'cry', 'upset', 'disappointed', '😢', '😭'],
    angry: ['angry', 'mad', 'furious', 'hate', 'annoyed', '😠', '😡'],
    surprised: ['wow', 'amazing', 'surprise', 'omg', 'what', 'really', '😲', '😮'],
    confused: ['confused', 'dont understand', 'what do you mean', 'huh', '🤔'],
    sleepy: ['tired', 'sleepy', 'exhausted', 'sleep', 'night', '😴'],
    excited: ['excited', 'cant wait', 'yay', 'woohoo', '🤩', '🎊'],
  };
  
  for (const [emotion, keywords] of Object.entries(emotions)) {
    if (keywords.some(k => msg.includes(k))) {
      return emotion;
    }
  }
  
  return 'neutral';
}

// ============================================================================
// EXPORTS
// ============================================================================

export default {
  textToSpeech,
  chat,
  detectEmotion,
  AI_CONFIG,
};

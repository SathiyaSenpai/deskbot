import express from 'express';
import { WebSocketServer } from 'ws';
import http from 'http';
import path from 'path';
import os from 'os';
import multer from 'multer';
import dgram from 'dgram';
import { fileURLToPath } from 'url';
import { textToSpeech, speechToText, chat, detectEmotion, setLanguage } from './ai-services.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
const PORT = 3000; 

// ENABLE CORS
app.use((req, res, next) => {
    res.header("Access-Control-Allow-Origin", "*");
    next();
});

const server = http.createServer(app);
const wss = new WebSocketServer({ server }); 

// Helper to find local physical network IP (prioritizes Wi-Fi wlan/wlp over USB tethering)
function getServerIP() {
    const interfaces = os.networkInterfaces();
    let ethernetFallback = null;

    for (const name of Object.keys(interfaces)) {
        if (name.toLowerCase().includes('tailscale') || 
            name.toLowerCase().includes('docker') || 
            name.toLowerCase().includes('vbox') || 
            name.toLowerCase().includes('tun') || 
            name.toLowerCase().includes('tap')) {
            continue;
        }
        for (const iface of interfaces[name]) {
            if (iface.family === 'IPv4' && !iface.internal) {
                if (name.toLowerCase().includes('wlan') || 
                    name.toLowerCase().includes('wlp') || 
                    name.toLowerCase().includes('wifi')) {
                    return iface.address; // Returns Wi-Fi IP (10.250.209.236)
                }
                if (!ethernetFallback) ethernetFallback = iface.address;
            }
        }
    }
    return ethernetFallback || 'localhost';
}

const SERVER_IP = getServerIP();
console.log(`📡 Server IP: ${SERVER_IP}:${PORT}`);

// UDP Auto-Discovery Service for ESP32 (Works on Android Hotspots & Dynamic Subnets)
const UDP_PORT = 3001;
const udpSocket = dgram.createSocket('udp4');

udpSocket.on('message', (msg, rinfo) => {
    const str = msg.toString();
    if (str.includes('DISCOVER_DESKBOT')) {
        const response = `DESKBOT_SERVER:${SERVER_IP}:${PORT}`;
        udpSocket.send(Buffer.from(response), rinfo.port, rinfo.address, (err) => {
            if (!err) {
                console.log(`[UDP Discovery] ESP32 auto-discovered server at ${rinfo.address}:${rinfo.port} -> Replied: ${response}`);
            }
        });
    }
});

udpSocket.bind(UDP_PORT, () => {
    try { udpSocket.setBroadcast(true); } catch(e) {}
    console.log(`📡 UDP Discovery Service running on port ${UDP_PORT}`);
});

app.use(express.static(path.join(__dirname, 'public')));
app.use('/audio', express.static(path.join(__dirname, 'audio'))); // Serve TTS audio files
app.use(express.json());

// Audio upload endpoint for whisper.cpp transcription
const upload = multer({ storage: multer.memoryStorage(), limits: { fileSize: 10 * 1024 * 1024 } });

app.post('/stt', upload.single('audio'), async (req, res) => {
  try {
    if (!req.file) {
      return res.status(400).json({ error: 'No audio file received' });
    }
    const mimeType = req.file.mimetype || 'audio/wav';
    console.log(`[STT] Received audio: ${req.file.size} bytes, type: ${mimeType}`);
    const transcript = await speechToText(req.file.buffer, mimeType);
    res.json({ transcript });
  } catch (err) {
    console.error('[STT] Error:', err.message);
    res.status(500).json({ error: err.message });
  }
});

let robotWs = null;
let controllers = new Set(); 

// Randomized cooldown for proximity greetings
let lastGreetingTime = 0;
let greetingCooldown = 25000; // Initial cooldown 25s

function getRandomCooldown() {
  // Random cooldown between 20-45 seconds for natural feel
  return 20000 + Math.random() * 25000;
}

const GREETING_PROMPTS = [
  "Greet your owner who just appeared nearby. Be brief and happy.",
  "Someone is close! Say a quick, friendly hello.",
  "Your owner is here! Express excitement briefly.",
  "Wave hello to the person who just came near you.",
  "Someone approached! Give a warm, short greeting.",
];

console.log(`\n🤖 SERVER READY: Connect PC to Hotspot`);
console.log(`📡 IP MUST BE IN CONFIG.H: Check 'ipconfig' or use: ${SERVER_IP}\n`);

// Handle incoming PCM audio stream from ESP32 mic
let micAudioBuffer = [];
let micAudioTimeout = null;
let isProcessingMicAudio = false;

function createWavHeader(dataLength, sampleRate = 16000, numChannels = 1, bitsPerSample = 16) {
    const buffer = Buffer.alloc(44);
    const byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    const blockAlign = numChannels * (bitsPerSample / 8);

    buffer.write('RIFF', 0);
    buffer.writeUInt32LE(36 + dataLength, 4);
    buffer.write('WAVE', 8);
    buffer.write('fmt ', 12);
    buffer.writeUInt32LE(16, 16);
    buffer.writeUInt16LE(1, 20); // PCM
    buffer.writeUInt16LE(numChannels, 22);
    buffer.writeUInt32LE(sampleRate, 24);
    buffer.writeUInt32LE(byteRate, 28);
    buffer.writeUInt16LE(blockAlign, 32);
    buffer.writeUInt16LE(bitsPerSample, 34);
    buffer.write('data', 36);
    buffer.writeUInt32LE(dataLength, 40);

    return buffer;
}

async function processMicAudioStream() {
    if (micAudioBuffer.length === 0 || isProcessingMicAudio) return;
    isProcessingMicAudio = true;

    try {
        const rawPcm = Buffer.concat(micAudioBuffer);
        micAudioBuffer = []; // Reset buffer

        if (rawPcm.length < 3200) { // Ignore chunks shorter than 100ms
            isProcessingMicAudio = false;
            return;
        }

        console.log(`🎙️ Processing mic audio: ${rawPcm.length} bytes PCM...`);
        const header = createWavHeader(rawPcm.length, 16000, 1, 16);
        const wavBuffer = Buffer.concat([header, rawPcm]);

        if (robotWs && robotWs.readyState === 1) {
            robotWs.send(JSON.stringify({ type: 'set_behavior', name: 'thinking' }));
        }

        const transcript = await speechToText(wavBuffer, 'audio/wav');
        console.log(`🗣️ Transcribed Voice: "${transcript}"`);

        if (transcript && transcript.trim().length > 0) {
            const reply = await chat(transcript);
            const audio = await textToSpeech(reply);
            const emotion = detectEmotion(reply);

            broadcast({ type: 'chat_response', text: reply });

            if (audio.audioFile && robotWs && robotWs.readyState === 1) {
                let expressionBehavior = emotion || 'happy';
                robotWs.send(JSON.stringify({ type: 'set_behavior', name: expressionBehavior }));
                robotWs.send(JSON.stringify({
                    type: 'play_audio',
                    text: reply,
                    url: `http://${SERVER_IP}:${PORT}${audio.audioFile}`
                }));
            }
        }
    } catch (err) {
        console.error('❌ Mic audio processing error:', err.message);
    } finally {
        isProcessingMicAudio = false;
    }
}

function handleMicAudioChunk(chunk) {
    micAudioBuffer.push(chunk);

    if (micAudioTimeout) clearTimeout(micAudioTimeout);
    micAudioTimeout = setTimeout(processMicAudioStream, 800);

    const totalBytes = micAudioBuffer.reduce((sum, buf) => sum + buf.length, 0);
    if (totalBytes >= 16000 * 2 * 5) { // Max 5 seconds
        if (micAudioTimeout) clearTimeout(micAudioTimeout);
        processMicAudioStream();
    }
}

wss.on('connection', (ws, req) => {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const type = url.searchParams.get('type'); 

    // 1. REGISTER ROBOT
    if (type === 'robot') {
        robotWs = ws;
        console.log(`✅ ROBOT CONNECTED!`);
        broadcast({ type: 'robot_status', state: 'ONLINE' });
        
        // Auto-sync accurate server time to DS3231 hardware RTC
        const now = new Date();
        const timestamp = `${now.getFullYear()}-${now.getMonth()+1}-${now.getDate()} ${now.getHours()}:${now.getMinutes()}:${now.getSeconds()}`;
        console.log(`⏱️ Auto-syncing DS3231 RTC time: ${timestamp}`);
        ws.send(JSON.stringify({ type: 'sync_time', timestamp: timestamp }));
    } 
    // 2. REGISTER WEB APP
    else {
        controllers.add(ws);
        console.log(`💻 WEB APP CONNECTED`);
        ws.send(JSON.stringify({ 
            type: 'robot_status', 
            state: robotWs ? 'ONLINE' : 'OFFLINE' 
        }));
    }

    ws.on('message', async (message) => {
        try {
            // Handle binary PCM audio data from ESP32 mic
            if (Buffer.isBuffer(message)) {
                if (ws === robotWs) {
                    handleMicAudioChunk(message);
                }
                return;
            }

            const msg = JSON.parse(message);

            // A. FROM ROBOT -> WEB (Sync & Sensors)
            if (ws === robotWs) {
                broadcast(msg); // Forward to Web App

                // PROXIMITY GREETING - With natural randomized cooldown
                if (msg.event === 'proximity') {
                    const now = Date.now();
                    if (now - lastGreetingTime > greetingCooldown) {
                        lastGreetingTime = now;
                        greetingCooldown = getRandomCooldown(); // Randomize next cooldown
                        
                        console.log(`👀 Proximity! Generating greeting (next in ${(greetingCooldown/1000).toFixed(1)}s)...`);
                        
                        // First trigger surprised -> then happy while speaking
                        robotWs.send(JSON.stringify({ type: 'set_behavior', name: 'surprised' }));
                        
                        // After 500ms, switch to happy and start speaking
                        setTimeout(async () => {
                            try {
                                const prompt = GREETING_PROMPTS[Math.floor(Math.random() * GREETING_PROMPTS.length)];
                                const text = await chat(prompt);
                                const audio = await textToSpeech(text);
                                
                                if (audio.audioFile && robotWs && robotWs.readyState === 1) {
                                    robotWs.send(JSON.stringify({ type: 'set_behavior', name: 'happy' }));
                                    robotWs.send(JSON.stringify({ 
                                         type: 'play_audio', 
                                         text: text,  // Send text for local TTS
                                         url: `http://${SERVER_IP}:${PORT}${audio.audioFile}`
                                    }));
                                    broadcast({ type: 'chat_response', text: text });
                                }
                            } catch (err) {
                                console.error('[GREETING] Error:', err);
                            }
                        }, 500);
                    } else {
                        console.log(`👀 Proximity ignored (cooldown: ${((greetingCooldown - (now - lastGreetingTime))/1000).toFixed(1)}s remaining)`);
                    }
                }
            } 
            
            // B. FROM WEB -> ROBOT (Control)
            else {
                console.log(`ðŸ“± Web Command: ${msg.type}`);
                
                // Handle state request
                if (msg.type === 'request_state') {
                    // Request current state from robot
                    if (robotWs && robotWs.readyState === 1) {
                        robotWs.send(JSON.stringify({ type: 'request_state' }));
                    } else {
                        ws.send(JSON.stringify({ 
                            type: 'robot_status', 
                            state: 'OFFLINE' 
                        }));
                    }
                }
                // Handle Chat
                else if (msg.type === 'chat_message') {
                    console.log(`💬 User: ${msg.text}`);
                    
                    // Wake up robot from sleep if needed
                    if (robotWs && robotWs.readyState === 1) {
                        robotWs.send(JSON.stringify({ type: 'wake_up' }));
                        robotWs.send(JSON.stringify({ type: 'set_behavior', name: 'thinking' }));
                    }
                    
                    const reply = await chat(msg.text);
                    const audio = await textToSpeech(reply);
                    
                    // Detect emotion from reply and set appropriate expression
                    const emotion = detectEmotion(reply);
                    console.log(`🎭 Detected emotion: ${emotion}`);
                    
                    // Reply to Web
                    ws.send(JSON.stringify({ type: 'chat_response', text: reply }));
                    
                    // Command Robot to speak with emotion
                    if (audio.audioFile) {
                        if (robotWs && robotWs.readyState === 1) {
                            // Set emotional expression based on response
                            let expressionBehavior = 'happy'; // default
                            switch(emotion) {
                                case 'happy': expressionBehavior = 'happy'; break;
                                case 'sad': expressionBehavior = 'sad'; break;
                                case 'excited': expressionBehavior = 'excited'; break;
                                case 'surprised': expressionBehavior = 'surprised'; break;
                                case 'confused': expressionBehavior = 'confused'; break;
                                default: expressionBehavior = 'happy';
                            }
                            
                            robotWs.send(JSON.stringify({ type: 'set_behavior', name: expressionBehavior }));
                            robotWs.send(JSON.stringify({ 
                                type: 'play_audio', 
                                text: reply,  // Send text for local TTS
                                url: `http://${SERVER_IP}:${PORT}${audio.audioFile}`
                            }));
                            
                            // Keep robot awake for 25 seconds with random movements
                            robotWs.send(JSON.stringify({ 
                                type: 'stay_awake', 
                                duration: 25000 // 25 seconds in milliseconds
                            }));
                        }
                    }
                }
                // Handle LED action - forward to robot
                else if (msg.type === 'led_action') {
                    console.log(`💡 LED Command: ${msg.color}`);
                    if (robotWs && robotWs.readyState === 1) {
                        robotWs.send(JSON.stringify(msg));
                    }
                } 
                // Handle language preference selection from web dashboard
                else if (msg.type === 'set_language') {
                    console.log(`🌐 Language preference updated: ${msg.lang}`);
                    setLanguage(msg.lang);
                }
                // Handle stopwatch commands
                else if (msg.type === 'stopwatch_start' || msg.type === 'stopwatch_stop' || msg.type === 'stopwatch_reset') {
                    console.log(`⏱️ Stopwatch: ${msg.type}`);
                    if (robotWs && robotWs.readyState === 1) {
                        robotWs.send(JSON.stringify(msg));
                    }
                }
                // Handle DS3231 RTC Alarm & Time Sync commands
                else if (msg.type === 'set_alarm' || msg.type === 'dismiss_alarm' || msg.type === 'sync_time') {
                    console.log(`⏰ RTC Command: ${msg.type}`, msg);
                    if (robotWs && robotWs.readyState === 1) {
                        robotWs.send(JSON.stringify(msg));
                    }
                }
                // Forward Buttons (set_behavior, etc.) - also broadcast to other web clients
                else if (robotWs && robotWs.readyState === 1) {
                    robotWs.send(JSON.stringify(msg));
                    // Broadcast behavior changes to other web clients for multi-client sync
                    if (msg.type === 'set_behavior') {
                        broadcast({ type: 'set_behavior', name: msg.name });
                    }
                }
            }

        } catch (e) { console.error(e); }
    });

    ws.on('close', () => {
        if (ws === robotWs) {
            console.log(`âŒ ROBOT DISCONNECTED`);
            robotWs = null;
            broadcast({ type: 'robot_status', state: 'OFFLINE' });
        } else {
            controllers.delete(ws);
        }
    });
});

function broadcast(data) {
    const msg = JSON.stringify(data);
    for (let client of controllers) {
        if (client.readyState === 1) client.send(msg);
    }
}

server.listen(PORT, '0.0.0.0', () => console.log(`ðŸš€ Listening on Port ${PORT}`));
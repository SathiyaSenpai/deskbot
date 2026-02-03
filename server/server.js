import express from 'express';
import { WebSocketServer } from 'ws';
import http from 'http';
import path from 'path';
import os from 'os'; // <--- FIXED: Import 'os' at the top level
import { fileURLToPath } from 'url';
import { textToSpeech, chat } from './ai-services.js'; 

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

// Helper to find local IP
function getServerIP() {
    const interfaces = os.networkInterfaces(); // Uses the imported 'os' module
    for (const name of Object.keys(interfaces)) {
        for (const iface of interfaces[name]) {
            if (iface.family === 'IPv4' && !iface.internal) {
                return iface.address;
            }
        }
    }
    return 'localhost';
}

const SERVER_IP = getServerIP();
console.log(`📡 Server IP: ${SERVER_IP}:${PORT}`);

app.use(express.static(path.join(__dirname, 'public')));
app.use(express.json());

let robotWs = null;
let controllers = new Set(); 

// ============================================================================
// PROXIMITY GREETING - Natural, randomized cooldown for realistic interaction
// ============================================================================
let lastGreetingTime = 0;
let greetingCooldown = 25000; // Initial cooldown 25s

function getRandomCooldown() {
  // Random cooldown between 20-45 seconds for natural feel
  return 20000 + Math.random() * 25000;
}

const GREETING_PROMPTS = [
  "Greet your owner who just appeared nearby. Be brief and happy.",
  "Someone is close! Say a quick, friendly hello in Tanglish.",
  "Your owner is here! Express excitement briefly.",
  "Wave hello to the person who just came near you.",
  "Someone approached! Give a warm, short greeting.",
];

console.log(`\n🤖 SERVER READY: Connect PC to Hotspot`);
console.log(`📡 IP MUST BE IN CONFIG.H: Check 'ipconfig' or use: ${SERVER_IP}\n`);

wss.on('connection', (ws, req) => {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const type = url.searchParams.get('type'); 

    // 1. REGISTER ROBOT
    if (type === 'robot') {
        robotWs = ws;
        console.log(`âœ… ROBOT CONNECTED!`);
        broadcast({ type: 'robot_status', state: 'ONLINE' });
    } 
    // 2. REGISTER WEB APP
    else {
        controllers.add(ws);
        console.log(`ðŸ’» WEB APP CONNECTED`);
        ws.send(JSON.stringify({ 
            type: 'robot_status', 
            state: robotWs ? 'ONLINE' : 'OFFLINE' 
        }));
    }

    ws.on('message', async (message) => {
        try {
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
                    
                    // Set thinking behavior while processing
                    if (robotWs && robotWs.readyState === 1) {
                        robotWs.send(JSON.stringify({ type: 'set_behavior', name: 'thinking' }));
                    }
                    
                    const reply = await chat(msg.text);
                    const audio = await textToSpeech(reply);
                    
                    // Reply to Web
                    ws.send(JSON.stringify({ type: 'chat_response', text: reply }));
                    
                    // Command Robot to speak
                    if (audio.audioFile) {
                        if (robotWs && robotWs.readyState === 1) {
                            robotWs.send(JSON.stringify({ type: 'set_behavior', name: 'speaking' }));
                            robotWs.send(JSON.stringify({ 
                                type: 'play_audio', 
                                url: `http://${SERVER_IP}:${PORT}${audio.audioFile}`
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
                // Handle stopwatch commands
                else if (msg.type === 'stopwatch_start' || msg.type === 'stopwatch_stop' || msg.type === 'stopwatch_reset') {
                    console.log(`⏱️ Stopwatch: ${msg.type}`);
                    if (robotWs && robotWs.readyState === 1) {
                        robotWs.send(JSON.stringify(msg));
                    }
                }
                // Handle show time voice command
                else if (msg.type === 'show_time') {
                    console.log(`🕐 Show Time: voice command`);
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
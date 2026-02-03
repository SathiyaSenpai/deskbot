const CONFIG = {
  WS_URL: `ws://${window.location.host}/ws?type=controller`,
};

const state = {
  ws: null,
  isConnected: false,
  pendingBehavior: null  // Track pending behavior commands to avoid double updates
};

const elements = {
  status: document.getElementById('connectionStatus'),
  robotState: document.getElementById('robotState'),
  eyeLeft: document.getElementById('eyeLeft'),
  eyeRight: document.getElementById('eyeRight'),
  sensorDist: document.getElementById('sensorDistance'),
  sensorTouch: document.getElementById('sensorTouch'),
  chat: document.getElementById('chatMessages'),
  input: document.getElementById('messageInput'),
  sendBtn: document.getElementById('sendButton')
};

// COMPLETE BEHAVIOR DEFINITIONS (Synced with firmware behaviors.h)
const BEHAVIORS = {
  // IDLE STATES
  'calm_idle':   { 
    openness: 1.0, scaleX: 1.0, top: 0.0, bot: 0.0, x: 0, y: 0,
    width: 32, height: 44, radius: 8, upperCurve: 0, lowerCurve: 0
  },
  'sleepy_idle': { 
    openness: 0.4, scaleX: 1.1, top: 0.4, bot: 0.0, x: 0, y: 5,
    width: 34, height: 32, radius: 10, upperCurve: 0.3, lowerCurve: 0
  },

  // EMOTIONS (PHASE 13: Geometry-based)
  'happy': { 
    openness: 1.0, scaleX: 1.1, top: 0.0, bot: 0.6, x: 0, y: -2,
    width: 35, height: 42, radius: 12, upperCurve: -0.1, lowerCurve: 0.2
  },
  'sad': { 
    openness: 0.7, scaleX: 0.9, top: 0.4, bot: 0.1, x: 0, y: 5,
    width: 28, height: 38, radius: 6, upperCurve: 0.2, lowerCurve: -0.1
  },
  'angry': { 
    openness: 0.9, scaleX: 0.9, top: 0.5, bot: -0.1, x: 0, y: 0,
    width: 30, height: 36, radius: 4, upperCurve: 0.3, lowerCurve: -0.2
  },
  'surprised': { 
    openness: 1.2, scaleX: 0.85, top: 0.0, bot: 0.0, x: 0, y: -2,
    width: 28, height: 52, radius: 14, upperCurve: -0.2, lowerCurve: -0.2
  },
  'confused': { 
    openness: 1.0, scaleX: 1.0, top: 0.1, bot: 0.0, x: 5, y: -2,
    width: 32, height: 44, radius: 8, upperCurve: 0.1, lowerCurve: 0
  },

  // FUNCTIONAL STATES
  'listening': { 
    openness: 1.1, scaleX: 1.0, top: 0.0, bot: 0.0, x: 0, y: 0,
    width: 34, height: 46, radius: 8, upperCurve: -0.05, lowerCurve: 0
  },
  'thinking': { 
    openness: 1.0, scaleX: 1.0, top: 0.1, bot: 0.1, x: -10, y: -10,
    width: 32, height: 42, radius: 8, upperCurve: 0.1, lowerCurve: 0.1
  },
  'speaking': { 
    openness: 1.0, scaleX: 1.05, top: 0.0, bot: 0.0, x: 0, y: 0,
    width: 32, height: 44, radius: 8, upperCurve: 0, lowerCurve: 0
  },
  'sleeping': { 
    openness: 0.0, scaleX: 1.0, top: 0.5, bot: 0.5, x: 0, y: 10,
    width: 32, height: 30, radius: 10, upperCurve: 0.5, lowerCurve: 0.5
  },

  // ADDITIONAL BEHAVIORS
  'curious_idle': { 
    openness: 1.0, scaleX: 1.0, top: 0.0, bot: 0.0, x: 5, y: -3,
    width: 32, height: 44, radius: 8, upperCurve: 0, lowerCurve: 0
  },
  'shy_happy': {
    openness: 0.8, scaleX: 1.0, top: 0.2, bot: 0.3, x: 3, y: 2,
    width: 32, height: 40, radius: 10, upperCurve: 0.1, lowerCurve: 0.15
  },
  'startled': {
    openness: 1.3, scaleX: 0.8, top: 0.0, bot: 0.0, x: 0, y: -5,
    width: 26, height: 54, radius: 16, upperCurve: -0.25, lowerCurve: -0.25
  },
  'playful_mischief': {
    openness: 1.0, scaleX: 1.0, top: 0.0, bot: 0.4, x: 8, y: -1,
    width: 33, height: 43, radius: 9, upperCurve: -0.05, lowerCurve: 0.15
  },
  
  // MOVEMENTS
  'look_left':   { openness: 1.0, scaleX: 1.0, top: 0.0, bot: 0.0, x: -12, y: 0 },
  'look_right':  { openness: 1.0, scaleX: 1.0, top: 0.0, bot: 0.0, x: 12, y: 0 }
};

let currentAnim = { ...BEHAVIORS['calm_idle'] };
let targetAnim = { ...BEHAVIORS['calm_idle'] };
let blinkTimer = 0;
let nextBlink = 3.0;
let isBlinking = false;

// PHASE 6: Effect rendering
let activeEffect = 'none';
let effectTimer = 0;

function renderLoop(timestamp) {
  // Smooth interpolation
  const f = 0.15;
  
  for (let key in targetAnim) {
    if (typeof targetAnim[key] === 'number') {
      currentAnim[key] += (targetAnim[key] - currentAnim[key]) * f;
    }
  }

  // Blink system
  blinkTimer += 0.016;
  if (blinkTimer > nextBlink) {
    if (!isBlinking) {
      isBlinking = true;
      blinkTimer = 0;
    } else {
      const blinkPhase = blinkTimer;
      if (blinkPhase > 0.25) {
        isBlinking = false;
        nextBlink = 2.5 + Math.random() * 3.5;
        blinkTimer = 0;
      }
    }
  }

  let blinkFactor = 1.0;
  if (isBlinking) {
    const t = blinkTimer;
    if (t < 0.08) blinkFactor = 1.0 - (t / 0.08);
    else if (t < 0.12) blinkFactor = 0.0;
    else if (t < 0.25) blinkFactor = (t - 0.12) / 0.13;
  }

  // Apply transform
  const scaleY = currentAnim.openness * blinkFactor;
  const transform = `translate(${currentAnim.x}px, ${currentAnim.y}px) scale(${currentAnim.scaleX}, ${scaleY})`;
  
  // Calculate lid clipping
  const topH = Math.max(0, currentAnim.top * 100);
  const botH = Math.max(0, currentAnim.bot * 100);
  const clip = `polygon(0 ${topH}%, 100% ${topH}%, 100% ${100-botH}%, 0 ${100-botH}%)`;

  // Apply to eyes
  if (elements.eyeLeft) {
    elements.eyeLeft.style.transform = transform;
    elements.eyeLeft.style.clipPath = clip;
    
    // PHASE 13: Apply geometry if available
    if (currentAnim.width) {
      const w = currentAnim.width * 0.43;
      const h = currentAnim.height * 0.43;
      const r = currentAnim.radius * 0.43;
      elements.eyeLeft.style.width = w + 'px';
      elements.eyeLeft.style.height = h + 'px';
      elements.eyeLeft.style.borderRadius = r + 'px';
    }
  }
  
  if (elements.eyeRight) {
    elements.eyeRight.style.transform = transform;
    elements.eyeRight.style.clipPath = clip;
    
    if (currentAnim.width) {
      const w = currentAnim.width * 0.43;
      const h = currentAnim.height * 0.43;
      const r = currentAnim.radius * 0.43;
      elements.eyeRight.style.width = w + 'px';
      elements.eyeRight.style.height = h + 'px';
      elements.eyeRight.style.borderRadius = r + 'px';
    }
    
    // Render effects on right eye
    renderEffects();
  }

  requestAnimationFrame(renderLoop);
}

// PHASE 6: Effect rendering system
function renderEffects() {
  if (!elements.eyeRight) return;
  
  // Clear old effects
  const oldEffects = elements.eyeRight.querySelectorAll('.eye-effect-elem');
  oldEffects.forEach(e => e.remove());
  
  effectTimer += 0.016;
  
  const container = document.createElement('div');
  container.className = 'eye-effect-elem';
  container.style.cssText = 'position:absolute;width:100%;height:100%;pointer-events:none;';
  
  switch(activeEffect) {
    case 'zzz':
      if (Math.floor(effectTimer * 2) % 2 === 0) {
        container.innerHTML = '<div style="position:absolute;right:-5px;top:5px;color:#fff;font-size:10px;animation:miniZzz 2s ease-out infinite;">z</div>';
      }
      break;
      
    case 'sparkle':
      if (Math.floor(effectTimer * 5) % 3 === 0) {
        container.innerHTML = '<div style="position:absolute;right:0;top:-5px;color:#fff;font-size:8px;">âœ¨</div>';
      }
      break;
      
    case 'question':
      container.innerHTML = '<div style="position:absolute;right:-8px;top:-5px;color:#fff;font-size:12px;font-weight:bold;">?</div>';
      break;
      
    case 'dots':
      const dotCount = Math.floor(effectTimer * 2) % 3 + 1;
      let dots = '';
      for (let i = 0; i < dotCount; i++) {
        dots += '<div style="position:absolute;width:3px;height:3px;background:#fff;border-radius:50%;right:' + (5 + i*5) + 'px;top:0;"></div>';
      }
      container.innerHTML = dots;
      break;
  }
  
  if (container.innerHTML) {
    elements.eyeRight.appendChild(container);
  }
}

// Network connection
function connect() {
  state.ws = new WebSocket(CONFIG.WS_URL);
  
  state.ws.onopen = () => {
    state.isConnected = true;
    updateStatus('Connected', 'connected');
    if (elements.input) elements.input.disabled = false;
    if (elements.sendBtn) elements.sendBtn.disabled = false;
    // Request current robot state if robot is already connected
    // Server will send robot_status with state when we connect
  };

  state.ws.onmessage = (evt) => {
    let msg;
    try {
      msg = JSON.parse(evt.data);
    } catch (e) {
      console.error('[WS] Failed to parse message:', e, evt.data);
      return;
    }
    
    // Handle robot connection status
    if (msg.type === 'robot_status') {
      if (msg.state === 'ONLINE' || msg.state === 'OFFLINE') {
        // Update robot state display
        if (elements.robotState) {
          elements.robotState.innerText = msg.state;
          elements.robotState.setAttribute('data-state', msg.state);
        }
        // Update connection status
        if (msg.state === 'ONLINE') {
          updateStatus('Robot Online', 'connected');
          // Request current behavior state if robot just came online
          if (state.ws && state.isConnected) {
            state.ws.send(JSON.stringify({ type: 'request_state' }));
          }
        } else {
          updateStatus('Robot Offline', 'disconnected');
        }
      }
      // Sync behavior from robot
      else if (msg.event === 'sync_behavior') {
        setBehavior(msg.detail);
      }
    }
    // Sync from button clicks (only if not from our own command)
    else if (msg.type === 'set_behavior') {
      // Only update if this is a response, not our own command
      // We'll track if we sent this command to avoid double updates
      if (!state.pendingBehavior || state.pendingBehavior !== msg.name) {
        setBehavior(msg.name);
      }
      state.pendingBehavior = null; // Clear pending flag
    }
    // Sensor data
    else if (msg.type === 'sensor_data') {
      updateSensors(msg);
    }
    // Chat response
    else if (msg.type === 'chat_response') {
      addMessage(msg.text, 'robot');
    }
  };

  state.ws.onclose = () => {
    state.isConnected = false;
    updateStatus('Disconnected', 'disconnected');
    if (elements.input) elements.input.disabled = true;
    if (elements.sendBtn) elements.sendBtn.disabled = true;
    setTimeout(connect, 3000);
  };
}

function setBehavior(name) {
  if (BEHAVIORS[name]) {
    targetAnim = { ...BEHAVIORS[name] };
    
    const behaviorLabel = document.getElementById('currentBehavior');
    if (behaviorLabel) behaviorLabel.innerText = name;
    
    // Set effect based on behavior
    if (name.includes('sleep')) activeEffect = 'zzz';
    else if (name === 'happy' || name === 'surprised') activeEffect = 'sparkle';
    else if (name === 'confused') activeEffect = 'question';
    else if (name === 'thinking') activeEffect = 'dots';
    else activeEffect = 'none';
    
    effectTimer = 0;
  }
}

function updateSensors(data) {
  // Use readable text for distance
  if (elements.sensorDist) {
    const distance = data.distance_mm || 0;
    if (distance === 0) {
      elements.sensorDist.innerText = 'No reading';
    } else if (distance < 100) {
      elements.sensorDist.innerText = 'Very close';
    } else if (distance < 300) {
      elements.sensorDist.innerText = 'Near';
    } else {
      elements.sensorDist.innerText = 'Far';
    }
  }
  
  if (elements.sensorTouch) elements.sensorTouch.innerText = data.touch_head ? 'Yes' : 'No';
  
  const sensorLight = document.getElementById('sensorLight');
  const sensorMotion = document.getElementById('sensorMotion');
  
  // Use readable text for light
  if (sensorLight) {
    const light = data.light || 0;
    if (light > 2000) {
      sensorLight.innerText = 'Dark';
    } else if (light > 1000) {
      sensorLight.innerText = 'Dim';
    } else {
      sensorLight.innerText = 'Bright';
    }
  }
  
  if (sensorMotion) sensorMotion.innerText = data.motion ? 'Yes' : 'No';
}

function addMessage(text, type) {
  const div = document.createElement('div');
  div.className = `message ${type}`;
  div.innerHTML = `
    <div class="message-content">${text}</div>
    <div class="message-time">Just now</div>
  `;
  elements.chat.appendChild(div);
  elements.chat.scrollTop = elements.chat.scrollHeight;
}

// Commands
window.triggerBehavior = (name) => {
  if (state.ws && state.isConnected) {
    state.pendingBehavior = name;  // Track that we're sending this command
    state.ws.send(JSON.stringify({ type: 'set_behavior', name: name }));
    // Also update locally immediately for better UX
    setBehavior(name);
  }
};

window.controlServo = (angle) => {
  if (state.ws && state.isConnected) {
    state.ws.send(JSON.stringify({ type: 'servo_action', angle: angle }));
  }
};

window.setLED = (color) => {
  if (state.ws && state.isConnected) {
    state.ws.send(JSON.stringify({ type: 'led_action', color: color }));
  }
};

function sendChat() {
  const text = elements.input.value.trim();
  if (!text) return;
  
  addMessage(text, 'user');
  
  if (state.ws && state.isConnected) {
    state.ws.send(JSON.stringify({ type: 'chat_message', text: text }));
  }
  
  elements.input.value = '';
}

function updateStatus(text, status) {
  if (elements.status) {
    elements.status.querySelector('.status-text').innerText = text;
    elements.status.setAttribute('data-status', status);
  }
}

// Initialize
document.addEventListener('DOMContentLoaded', () => {
  renderLoop();
  connect();
  
  if (elements.sendBtn) elements.sendBtn.onclick = sendChat;
  if (elements.input) {
    elements.input.onkeypress = (e) => {
      if (e.key === 'Enter') sendChat();
    };
  }
  
  // =========================================================================
  // PHONE MIC - Web Speech API Implementation
  // =========================================================================
  const micButton = document.getElementById('micButton');
  const micIcon = micButton?.querySelector('.mic-icon');
  const micStatus = micButton?.querySelector('.mic-status');
  
  // Check for browser support
  const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
  
  if (micButton && SpeechRecognition) {
    const recognition = new SpeechRecognition();
    recognition.lang = 'en-IN'; // Indian English
    recognition.continuous = false;
    recognition.interimResults = false;
    recognition.maxAlternatives = 1;
    
    let isListening = false;
    
    micButton.onclick = () => {
      if (!state.isConnected) {
        alert('Not connected to server');
        return;
      }
      
      if (isListening) {
        recognition.stop();
        return;
      }
      
      try {
        recognition.start();
        isListening = true;
        micButton.classList.add('listening');
        if (micIcon) micIcon.textContent = '🔴';
        if (micStatus) micStatus.textContent = 'Listening...';
        console.log('[MIC] Speech recognition started');
      } catch (err) {
        console.error('[MIC] Failed to start:', err);
      }
    };
    
    recognition.onresult = (event) => {
      const transcript = event.results[0][0].transcript;
      const confidence = event.results[0][0].confidence;
      console.log(`[MIC] Recognized: "${transcript}" (confidence: ${(confidence * 100).toFixed(1)}%)`);
      
      // Send to chat
      if (transcript.trim()) {
        addMessage(transcript, 'user');
        if (state.ws && state.isConnected) {
          state.ws.send(JSON.stringify({ type: 'chat_message', text: transcript }));
        }
      }
    };
    
    recognition.onerror = (event) => {
      console.error('[MIC] Error:', event.error);
      if (micStatus) {
        switch(event.error) {
          case 'no-speech':
            micStatus.textContent = 'No speech detected';
            break;
          case 'audio-capture':
            micStatus.textContent = 'No microphone found';
            break;
          case 'not-allowed':
            micStatus.textContent = 'Mic permission denied';
            break;
          default:
            micStatus.textContent = `Error: ${event.error}`;
        }
      }
    };
    
    recognition.onend = () => {
      isListening = false;
      micButton.classList.remove('listening');
      if (micIcon) micIcon.textContent = '🎤';
      setTimeout(() => {
        if (micStatus) micStatus.textContent = '';
      }, 2000);
      console.log('[MIC] Speech recognition ended');
    };
    
    // Add visual hint that mic is available
    micButton.title = 'Click to speak (Speech Recognition)';
    micButton.style.opacity = '1';
  } else if (micButton) {
    // Browser doesn't support Speech Recognition
    micButton.onclick = () => {
      alert('Speech recognition not supported in this browser.\\nPlease use Chrome, Edge, or Safari.\\n\\nYou can still type messages below!');
    };
    micButton.title = 'Speech not supported - use text input';
    micButton.style.opacity = '0.5';
    console.log('[MIC] Speech Recognition not supported');
  }
  
  // Servo buttons
  const servoLeft = document.getElementById('servoLeft');
  const servoCenter = document.getElementById('servoCenter');
  const servoRight = document.getElementById('servoRight');
  
  if (servoLeft) servoLeft.onclick = () => controlServo(65);   // Cardboard safe: 60-120
  if (servoCenter) servoCenter.onclick = () => controlServo(90);
  if (servoRight) servoRight.onclick = () => controlServo(115); // Cardboard safe: 60-120

  // LED buttons
  document.querySelectorAll('.led-btn').forEach(btn => {
    btn.onclick = (e) => {
      const color = btn.getAttribute('data-color');
      setLED(color);
      // Update active state
      document.querySelectorAll('.led-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
    };
  });

  // Behavior buttons
  document.querySelectorAll('.action-btn').forEach(btn => {
    btn.onclick = () => {
      const behavior = btn.getAttribute('data-behavior');
      triggerBehavior(behavior);
    };
  });
});

// ==============================================
// Demo Panel Functions
// ==============================================

// Stopwatch controls
let stopwatchInterval = null;
let stopwatchStartTime = null;
let stopwatchElapsed = 0;

function stopwatchStart() {
  if (state.ws && state.isConnected) {
    state.ws.send(JSON.stringify({ type: 'stopwatch_start' }));
  }
  
  // Local display update
  if (!stopwatchInterval) {
    stopwatchStartTime = Date.now() - stopwatchElapsed;
    stopwatchInterval = setInterval(updateStopwatchDisplay, 10);
  }
}

function stopwatchStop() {
  if (state.ws && state.isConnected) {
    state.ws.send(JSON.stringify({ type: 'stopwatch_stop' }));
  }
  
  if (stopwatchInterval) {
    clearInterval(stopwatchInterval);
    stopwatchInterval = null;
    stopwatchElapsed = Date.now() - stopwatchStartTime;
  }
}

function stopwatchReset() {
  if (state.ws && state.isConnected) {
    state.ws.send(JSON.stringify({ type: 'stopwatch_reset' }));
  }
  
  if (stopwatchInterval) {
    clearInterval(stopwatchInterval);
    stopwatchInterval = null;
  }
  stopwatchElapsed = 0;
  updateStopwatchDisplay();
}

function updateStopwatchDisplay() {
  const display = document.getElementById('stopwatchDisplay');
  if (!display) return;
  
  let totalTime = stopwatchElapsed;
  if (stopwatchInterval && stopwatchStartTime) {
    totalTime = Date.now() - stopwatchStartTime;
  }
  
  const minutes = Math.floor(totalTime / 60000);
  const seconds = Math.floor((totalTime % 60000) / 1000);
  const centiseconds = Math.floor((totalTime % 1000) / 10);
  
  display.textContent = `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}.${centiseconds.toString().padStart(2, '0')}`;
}

// ==============================================
// Audio Testing Function
// ==============================================

function testAudio() {
  if (state.ws && state.isConnected) {
    state.ws.send(JSON.stringify({ type: 'test_audio' }));
    
    // Update local status
    const status = document.getElementById('audioStatus');
    if (status) {
      status.textContent = 'Testing audio systems...';
      status.style.color = '#007acc';
      
      // Clear status after test duration
      setTimeout(() => {
        status.textContent = 'Test completed';
        status.style.color = '#666';
        setTimeout(() => {
          status.textContent = '';
        }, 3000);
      }, 8000); // 8 seconds for full test
    }
  } else {
    alert('Not connected to DeskBot');
  }
}
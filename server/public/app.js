const CONFIG = {
  WS_URL: `ws://${window.location.host}/ws?type=controller`,
};

const state = {
  ws: null,
  isConnected: false,
  pendingBehavior: null  // Track pending behavior commands to avoid double updates
};

const elements = {
  get status() { return document.getElementById('connectionStatus'); },
  get robotState() { return document.getElementById('robotState'); },
  get eyeLeft() { return document.getElementById('eyeLeft'); },
  get eyeRight() { return document.getElementById('eyeRight'); },
  get sensorDist() { return document.getElementById('sensorDistance'); },
  get sensorTouch() { return document.getElementById('sensorTouch'); },
  get sensorTemp() { return document.getElementById('sensorTemp'); },
  get sensorLight() { return document.getElementById('sensorLight'); },
  get sensorMotion() { return document.getElementById('sensorMotion'); },
  get chat() { return document.getElementById('chatMessages'); },
  get input() { return document.getElementById('messageInput'); },
  get sendBtn() { return document.getElementById('sendButton'); },
  get langSelect() { return document.getElementById('languageSelect'); }
};

// Behavior animation parameters synced with firmware
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
    if (elements.langSelect) {
      state.ws.send(JSON.stringify({ type: 'set_language', lang: elements.langSelect.value }));
    }
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
      else if (msg.event === 'alarm_set' || msg.event === 'alarm_triggered' || msg.event === 'alarm_dismissed') {
        if (msg.detail) addMessage(`🔔 ${msg.detail}`, 'system');
      }
      else if (msg.event === 'connect') {
        updateStatus('Robot Online', 'connected');
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
    // Chat response from robot
    else if (msg.type === 'chat_response') {
      addMessage(msg.text, 'robot', msg.audio_url);
    }
    // User speech transcription (from mic)
    else if (msg.type === 'user_speech') {
      addMessage(msg.text, 'user');
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
  if (elements.sensorDist) {
    const distance = data.distance_mm || 0;
    if (distance === 0) elements.sensorDist.innerText = 'No reading';
    else if (distance < 100) elements.sensorDist.innerText = `${distance} mm (Very close)`;
    else if (distance < 1000) elements.sensorDist.innerText = `${(distance/10).toFixed(0)} cm (${distance} mm)`;
    else elements.sensorDist.innerText = `${(distance/1000).toFixed(2)} m (${(distance/10).toFixed(0)} cm)`;
  }
  
  if (elements.sensorTouch) {
    if (data.touch_head && data.touch_side) elements.sensorTouch.innerText = 'Head + Side';
    else if (data.touch_head) elements.sensorTouch.innerText = 'Head Touch';
    else if (data.touch_side) elements.sensorTouch.innerText = 'Side Touch';
    else elements.sensorTouch.innerText = 'None';
  }

  if (elements.sensorTemp) {
    elements.sensorTemp.innerText = data.temperature ? `${data.temperature.toFixed(1)} °C` : 'N/A';
  }
  
  if (elements.sensorLight) {
    const light = data.light || 0;
    if (light > 2500) elements.sensorLight.innerText = `Dark (${light})`;
    else if (light > 1200) elements.sensorLight.innerText = `Dim (${light})`;
    else elements.sensorLight.innerText = `Bright (${light})`;
  }
  
  if (elements.sensorMotion) {
    elements.sensorMotion.innerText = data.motion ? 'Detected' : 'Clear';
  }
}

function addMessage(text, type, audioUrl = null) {
  const div = document.createElement('div');
  div.className = `message ${type}`;
  
  let speechBtn = '';
  if (type === 'robot') {
    speechBtn = `<button class="speech-btn" onclick="playSpeechText(this, '${encodeURIComponent(text)}', '${audioUrl || ''}')">🔊 Speak</button>`;
  }
  
  div.innerHTML = `
    <div class="message-content">${text}</div>
    <div class="message-meta" style="display:flex; justify-content:space-between; align-items:center; margin-top:4px;">
      <span class="message-time" style="font-size:0.75rem; opacity:0.7;">Just now</span>
      ${speechBtn}
    </div>
  `;
  elements.chat.appendChild(div);
  elements.chat.scrollTop = elements.chat.scrollHeight;

  // Auto-play speech for robot responses
  if (type === 'robot') {
    playSpeechText(null, encodeURIComponent(text), audioUrl);
  }
}

window.playSpeechText = (btnEl, encodedText, audioUrl) => {
  const text = decodeURIComponent(encodedText);
  if (audioUrl && audioUrl !== 'null' && audioUrl !== 'undefined') {
    const audio = new Audio(audioUrl);
    audio.play().catch(err => {
      console.warn('[SPEECH] Kokoro audio playback blocked or pending, using WebSpeech API fallback:', err);
      speakWebSpeech(text);
    });
  } else {
    speakWebSpeech(text);
  }
};

function speakWebSpeech(text) {
  if ('speechSynthesis' in window) {
    window.speechSynthesis.cancel();
    const cleanText = text.replace(/[\u{1F600}-\u{1F64F}\u{1F300}-\u{1F5FF}\u{1F680}-\u{1F6FF}\u{2600}-\u{27BF}]/gu, '').trim();
    if (!cleanText) return;
    
    const utterance = new SpeechSynthesisUtterance(cleanText);
    utterance.rate = 1.0;
    utterance.pitch = 1.25; // Expressive high pitch for Nisya!
    
    const voices = window.speechSynthesis.getVoices();
    const femaleVoice = voices.find(v => v.lang.startsWith('en') && (v.name.includes('Female') || v.name.includes('Google') || v.name.includes('Zira') || v.name.includes('Samantha')));
    if (femaleVoice) utterance.voice = femaleVoice;
    
    window.speechSynthesis.speak(utterance);
  }
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
  if (elements.langSelect) {
    elements.langSelect.onchange = (e) => {
      if (state.ws && state.isConnected) {
        state.ws.send(JSON.stringify({ type: 'set_language', lang: e.target.value }));
        console.log(`[LANG] Switched language to: ${e.target.value}`);
      }
    };
  }
  
  // Hardware I2S Microphone (INMP441) Controls
  const micButton = document.getElementById('micButton');
  const micIcon = micButton?.querySelector('.mic-icon');
  const micStatus = micButton?.querySelector('.mic-status');

  if (micButton) {
    micButton.title = 'Robot Hardware I2S Microphone Active (INMP441)';
    micButton.style.opacity = '1';

    micButton.onclick = () => {
      if (!state.isConnected) {
        alert('Not connected to server');
        return;
      }

      // Command ESP32 Hardware I2S mic into active listening mode
      state.ws.send(JSON.stringify({ type: 'trigger_listening' }));
      triggerBehavior('listening');

      micButton.classList.add('listening');
      if (micIcon) micIcon.textContent = '🎙️';
      if (micStatus) micStatus.textContent = 'Listening...';

      setTimeout(() => {
        micButton.classList.remove('listening');
        if (micIcon) micIcon.textContent = '🎤';
        if (micStatus) micStatus.textContent = '';
      }, 4000);
    };
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
    alert('Not connected to Nisya');
  }
}

// ==============================================
// DS3231 RTC Alarm & Time Functions
// ==============================================

function setAlarm() {
  const input = document.getElementById('alarmTimeInput');
  if (!input || !input.value) {
    alert('Please select an alarm time first.');
    return;
  }
  const parts = input.value.split(':');
  const hour = parseInt(parts[0], 10);
  const minute = parseInt(parts[1], 10);
  
  if (state.ws && state.isConnected) {
    state.ws.send(JSON.stringify({
      type: 'set_alarm',
      hour: hour,
      minute: minute
    }));
    addMessage(`⏰ Alarm set for ${hour.toString().padStart(2, '0')}:${minute.toString().padStart(2, '0')}`, 'system');
  } else {
    alert('Not connected to Nisya');
  }
}

function dismissAlarm() {
  if (state.ws && state.isConnected) {
    state.ws.send(JSON.stringify({ type: 'dismiss_alarm' }));
    addMessage('🔕 Alarm dismissed / silenced', 'system');
  } else {
    alert('Not connected to Nisya');
  }
}

function syncTimeNow() {
  if (state.ws && state.isConnected) {
    const now = new Date();
    const timestamp = `${now.getFullYear()}-${now.getMonth()+1}-${now.getDate()} ${now.getHours()}:${now.getMinutes()}:${now.getSeconds()}`;
    state.ws.send(JSON.stringify({
      type: 'sync_time',
      timestamp: timestamp
    }));
    addMessage(`🔄 Synced DS3231 time: ${now.toLocaleTimeString()}`, 'system');
  } else {
    alert('Not connected to Nisya');
  }
}

// Expose functions globally for HTML onclick handlers
window.stopwatchStart = stopwatchStart;
window.stopwatchStop = stopwatchStop;
window.stopwatchReset = stopwatchReset;
window.setAlarm = setAlarm;
window.dismissAlarm = dismissAlarm;
window.syncTimeNow = syncTimeNow;
window.testAudio = testAudio;
window.triggerBehavior = triggerBehavior;
window.controlServo = controlServo;
window.setLED = setLED;
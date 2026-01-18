// ================================================
// ROBOT CONTROL DASHBOARD - JavaScript
// Production-ready WebSocket integration for EMO desk robot
// ================================================

// ================================================
// CONFIGURATION
// ================================================

const CONFIG = {
  // WebSocket server URL - connects to our Node.js server
  WS_URL: `ws://${window.location.host}/ws?type=controller&token=emo_secret_2024`,
  RECONNECT_INTERVAL: 5000, // Reconnect every 5 seconds
  MAX_MESSAGES: 100, // Maximum chat messages to keep in memory
  
  // API endpoints
  API_BASE: '', // Same origin
  
  // Eye animation sync interval (ms)
  EYE_SYNC_INTERVAL: 100,
};

// ================================================
// STATE MANAGEMENT
// ================================================

const state = {
  ws: null,
  isConnected: false,
  reconnectTimer: null,
  robotState: 'IDLE',
  currentBehavior: 'calm_idle',
  sensors: {
    light: '—',
    motion: '—',
    sound: '—',
    touch: '—',
    distance: '—',
  },
  messages: [],
  ledColor: null,
  servoAngle: 90,
};

// ================================================
// DOM ELEMENTS
// ================================================

const elements = {
  connectionStatus: document.getElementById('connectionStatus'),
  robotState: document.getElementById('robotState'),
  currentBehavior: document.getElementById('currentBehavior'),
  eyeLeft: document.getElementById('eyeLeft'),
  eyeRight: document.getElementById('eyeRight'),
  sensorLight: document.getElementById('sensorLight'),
  sensorMotion: document.getElementById('sensorMotion'),
  sensorSound: document.getElementById('sensorSound'),
  sensorTouch: document.getElementById('sensorTouch'),
  sensorDistance: document.getElementById('sensorDistance'),
  chatMessages: document.getElementById('chatMessages'),
  messageInput: document.getElementById('messageInput'),
  sendButton: document.getElementById('sendButton'),
  micButton: document.getElementById('micButton'),
  // New elements
  actionBtns: document.querySelectorAll('.action-btn'),
  ledBtns: document.querySelectorAll('.led-btn'),
  servoLeft: document.getElementById('servoLeft'),
  servoCenter: document.getElementById('servoCenter'),
  servoRight: document.getElementById('servoRight'),
};

// ================================================
// SPEECH RECOGNITION SETUP
// ================================================

// Initialize Speech Recognition API (with webkit fallback)
const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
const speechRecognitionAvailable = !!SpeechRecognition;

let recognition = null;
let isRecording = false;

if (speechRecognitionAvailable) {
  recognition = new SpeechRecognition();

  // Configure speech recognition
  recognition.continuous = false; // Stop after one phrase
  recognition.interimResults = false; // Only final results
  recognition.lang = 'ta-IN'; // Tamil (India) as primary language

  // Handle successful speech recognition
  recognition.onresult = (event) => {
    let transcript = '';
    for (let i = event.resultIndex; i < event.results.length; i++) {
      transcript += event.results[i][0].transcript;
    }
    handleUserVoice(transcript);
  };

  // Handle speech recognition start
  recognition.onstart = () => {
    isRecording = true;
    elements.micButton.classList.add('listening');
    elements.micButton.classList.remove('error');
    updateRobotState('LISTENING', 'Attentive');
  };

  // Handle speech recognition end
  recognition.onend = () => {
    isRecording = false;
    elements.micButton.classList.remove('listening');
    if (state.robotState === 'LISTENING') {
      updateRobotState('IDLE', 'Waiting');
    }
  };

  // Handle speech recognition errors
  recognition.onerror = (event) => {
    console.error('Speech recognition error:', event.error);
    isRecording = false;
    elements.micButton.classList.add('error');
    elements.micButton.classList.remove('listening');

    // Handle specific errors
    switch (event.error) {
      case 'no-speech':
        console.warn('No speech detected. Try again.');
        break;
      case 'network':
        console.error('Network error. Check your connection.');
        break;
      case 'not-allowed':
        console.error('Microphone permission denied.');
        elements.micButton.disabled = true;
        break;
      default:
        console.error('Recognition error:', event.error);
    }

    // Clear error state after 2 seconds
    setTimeout(() => {
      elements.micButton.classList.remove('error');
    }, 2000);
  };
}

// ================================================
// TEXT-TO-SPEECH IMPLEMENTATION
// ================================================

/**
 * TTS Provider abstraction
 * This allows swapping implementations (Google Cloud TTS, etc.) without changing speakText()
 */
const ttsProvider = {
  // Configuration
  config: {
    rate: 0.9, // Speech rate (0.1 - 10)
    pitch: 1.0, // Pitch (0.1 - 2)
    volume: 1.0, // Volume (0 - 1)
  },

  /**
   * Detect language from text
   * Supports Tamil, English, and Tanglish (mixed)
   * @param {string} text - Text to analyze
   * @returns {string} Language code (ta-IN, en-US)
   */
  detectLanguage(text) {
    // Tamil script detection (unicode range for Tamil: U+0B80 - U+0BFF)
    const tamilRegex = /[\u0B80-\u0BFF]/g;
    const tamilChars = text.match(tamilRegex) || [];
    
    // If more than 20% of text is Tamil script, use Tamil
    if (tamilChars.length / text.length > 0.2) {
      return 'ta-IN';
    }
    return 'en-US';
  },

  /**
   * Select appropriate voice based on language and gender preference
   * Prefers child-like, softer voices for a calm personality
   * @param {string} lang - Language code (ta-IN, en-US)
   * @returns {SpeechSynthesisVoice|null} Selected voice or null
   */
  selectVoice(lang) {
    if (!('speechSynthesis' in window)) return null;

    const voices = speechSynthesis.getVoices();
    if (voices.length === 0) return null;

    // Priority keywords for softer, child-like voices
    const softVoiceKeywords = ['female', 'woman', 'google', 'natural', 'premium'];

    // Look for language-specific voice first
    let selectedVoice = null;
    for (const voice of voices) {
      const voiceLang = voice.lang.split('-')[0]; // Extract language code
      if (voiceLang === lang.split('-')[0]) {
        // Found matching language
        if (!selectedVoice) {
          selectedVoice = voice;
        }
        // Prefer softer voices
        const voiceName = voice.name.toLowerCase();
        if (softVoiceKeywords.some((keyword) => voiceName.includes(keyword))) {
          selectedVoice = voice;
          break;
        }
      }
    }

    // Fallback to first available voice
    return selectedVoice || voices[0];
  },

  /**
   * Play speech using browser SpeechSynthesis API
   * This is a temporary implementation that can be replaced with server TTS later
   * @param {string} text - Text to speak
   * @param {Function} onStart - Callback when speech starts
   * @param {Function} onEnd - Callback when speech ends
   */
  play(text, onStart, onEnd) {
    if (!('speechSynthesis' in window)) {
      console.error('Speech Synthesis API not supported');
      if (onEnd) onEnd();
      return;
    }

    // Cancel any ongoing speech
    speechSynthesis.cancel();

    const lang = this.detectLanguage(text);
    const voice = this.selectVoice(lang);

    if (!voice) {
      console.error('No suitable voice found');
      if (onEnd) onEnd();
      return;
    }

    // Create utterance
    const utterance = new SpeechSynthesisUtterance(text);
    utterance.voice = voice;
    utterance.lang = lang;
    utterance.rate = this.config.rate;
    utterance.pitch = this.config.pitch;
    utterance.volume = this.config.volume;

    // Handle events
    utterance.onstart = () => {
      console.log(`🗣️ Speaking (${lang}):`, text);
      if (onStart) onStart();
    };

    utterance.onend = () => {
      console.log('🔇 Speech ended');
      if (onEnd) onEnd();
    };

    utterance.onerror = (event) => {
      console.error('Speech error:', event.error);
      if (onEnd) onEnd();
    };

    // Speak
    speechSynthesis.speak(utterance);
  },
};

// Track if robot is currently speaking
let isSpeaking = false;

/**
 * PUBLIC API: Play robot speech
 * 
 * This is the ONLY entry point for robot speech.
 * It abstracts the TTS implementation, allowing future replacement with server TTS.
 *
 * @param {string} text - Text for robot to speak (supports Tamil, English, Tanglish)
 *
 * EXAMPLE:
 * speakText("வணக்கம்! How are you today?");
 */
function speakText(text) {
  if (!text || typeof text !== 'string') {
    console.warn('Invalid speech text:', text);
    return;
  }

  // Prevent overlapping speech
  if (isSpeaking) {
    console.warn('Robot is already speaking');
    return;
  }

  // Check if connected
  if (!state.isConnected) {
    console.warn('Cannot speak: robot not connected');
    return;
  }

  isSpeaking = true;

  // Update robot state to SPEAKING
  updateRobotState('SPEAKING', 'Talking');

  // Play speech using current provider
  // NOTE: This call can be swapped for server TTS later:
  // Example: ttsProvider.play() → fetchServerTTS() → playAudio()
  ttsProvider.play(
    text,
    // On speech start
    () => {
      // Visual feedback already handled by updateRobotState()
    },
    // On speech end
    () => {
      isSpeaking = false;
      // Return to IDLE state
      updateRobotState('IDLE', 'Listening');
    }
  );
}

// ================================================
// WEBSOCKET CONNECTION
// ================================================

/**
 * Initialize WebSocket connection to robot server
 */
function connectWebSocket() {
  try {
    console.log('Attempting to connect to WebSocket server...');
    state.ws = new WebSocket(CONFIG.WS_URL);

    state.ws.onopen = handleWebSocketOpen;
    state.ws.onmessage = handleWebSocketMessage;
    state.ws.onerror = handleWebSocketError;
    state.ws.onclose = handleWebSocketClose;
  } catch (error) {
    console.error('WebSocket connection error:', error);
    updateConnectionStatus('disconnected');
    scheduleReconnect();
  }
}

/**
 * Handle successful WebSocket connection
 */
function handleWebSocketOpen() {
  console.log('✅ WebSocket connected');
  state.isConnected = true;
  updateConnectionStatus('connected');
  
  // Clear reconnect timer if exists
  if (state.reconnectTimer) {
    clearTimeout(state.reconnectTimer);
    state.reconnectTimer = null;
  }

  // Send initial handshake (optional)
  sendMessage({
    type: 'client_connected',
    timestamp: new Date().toISOString(),
  });
}

/**
 * Handle incoming WebSocket messages
 * @param {MessageEvent} event - WebSocket message event
 */
function handleWebSocketMessage(event) {
  try {
    const data = JSON.parse(event.data);
    console.log('📨 Received:', data);

    // Route message based on type
    switch (data.type) {
      case 'robot_status':
        updateRobotState(data.state, data.behavior);
        break;
      
      case 'behavior_change':
        updateBehavior(data.behavior);
        break;
      
      case 'sensor_update':
        updateSensors(data.sensors);
        break;
      
      case 'robot_message':
      case 'bot_response':
        addMessage('robot', data.text || data.message);
        // Automatically speak the robot message (can be disabled with data.noSpeak flag)
        if (data.noSpeak !== true && (data.text || data.message)) {
          speakText(data.text || data.message);
        }
        break;
      
      case 'eye_state':
        // Sync eye animation state from robot
        updateEyePreview(data.eyes);
        break;
      
      case 'led_state':
        // LED color feedback
        updateLedState(data.color);
        break;
      
      case 'event':
        handleRobotEvent(data.event, data.details);
        break;
      
      default:
        console.warn('Unknown message type:', data.type);
    }
  } catch (error) {
    console.error('Error parsing message:', error);
  }
}

/**
 * Handle WebSocket errors
 * @param {Event} error - Error event
 */
function handleWebSocketError(error) {
  console.error('❌ WebSocket error:', error);
  updateConnectionStatus('disconnected');
}

/**
 * Handle WebSocket connection close
 * @param {CloseEvent} event - Close event
 */
function handleWebSocketClose(event) {
  console.log('WebSocket closed:', event.code, event.reason);
  state.isConnected = false;
  updateConnectionStatus('disconnected');
  updateRobotState('OFFLINE', 'Disconnected');
  scheduleReconnect();
}

/**
 * Schedule automatic reconnection
 */
function scheduleReconnect() {
  if (state.reconnectTimer) return; // Already scheduled
  
  console.log(`Will attempt to reconnect in ${CONFIG.RECONNECT_INTERVAL / 1000}s...`);
  updateConnectionStatus('connecting');
  
  state.reconnectTimer = setTimeout(() => {
    state.reconnectTimer = null;
    connectWebSocket();
  }, CONFIG.RECONNECT_INTERVAL);
}

/**
 * Send message through WebSocket
 * @param {Object} data - Data to send
 */
function sendMessage(data) {
  if (state.ws && state.ws.readyState === WebSocket.OPEN) {
    state.ws.send(JSON.stringify(data));
    console.log('📤 Sent:', data);
  } else {
    console.warn('Cannot send message: WebSocket not connected');
  }
}

// ================================================
// UI UPDATE FUNCTIONS
// ================================================

/**
 * Update connection status indicator
 * @param {string} status - 'connected', 'disconnected', or 'connecting'
 */
function updateConnectionStatus(status) {
  elements.connectionStatus.setAttribute('data-status', status);
  
  const statusText = {
    connected: 'Connected',
    disconnected: 'Disconnected',
    connecting: 'Connecting...',
  };
  
  elements.connectionStatus.querySelector('.status-text').textContent = statusText[status] || status;
  
  // Chat works via API even without WebSocket, so always enable input
  // Only disable when sleeping
  if (state.robotState !== 'SLEEPING') {
    elements.messageInput.disabled = false;
    elements.sendButton.disabled = false;
  }
}

/**
 * Update robot state display
 * @param {string} newState - Robot state (IDLE, SPEAKING, LISTENING, etc.)
 * @param {string} behavior - Current behavior name (optional)
 */
function updateRobotState(newState, behavior) {
  state.robotState = newState;
  elements.robotState.setAttribute('data-state', newState);
  elements.robotState.textContent = newState;
  
  // Update robot panel state for animations
  const robotPanel = document.querySelector('.robot-panel');
  if (robotPanel) {
    robotPanel.setAttribute('data-state', newState);
  }
  
  // Update behavior if provided
  if (behavior) {
    updateBehavior(behavior);
  }
  
  // Disable input when sleeping, enable otherwise
  if (newState === 'SLEEPING') {
    elements.messageInput.disabled = true;
    elements.sendButton.disabled = true;
  } else {
    elements.messageInput.disabled = false;
    elements.sendButton.disabled = false;
  }
}

/**
 * Update current behavior display
 * @param {string} behavior - Behavior name
 */
function updateBehavior(behavior) {
  state.currentBehavior = behavior;
  if (elements.currentBehavior) {
    elements.currentBehavior.textContent = behavior;
  }
  
  // Update eye preview based on behavior
  animateEyePreview(behavior);
}

// Eye preview animation state (mirrors the demo's eyeState)
const previewEyeState = {
  openness: 1.0,
  offsetX: 0,
  offsetY: 0,
  scaleX: 1.0,
  bottomLid: 0.5,
  // Targets
  targetOpenness: 1.0,
  targetOffsetX: 0,
  targetOffsetY: 0,
  targetScaleX: 1.0,
  targetBottomLid: 0.5,
  // Animation
  animating: false,
  animationId: null,
  lastFrameTime: 0,
};

// Behavior definitions for preview (EXACTLY matching main demo BEHAVIORS)
// offsetX range: ±48 (EYE_MAX_X), offsetY range: ±24 (EYE_MAX_Y)
// Scale factor 0.15 applied in renderPreviewEyes
const PREVIEW_BEHAVIORS = {
  // BASE / IDLE
  'calm_idle': { openness: 1.0, scaleX: 1.0, bottomLid: 0.5, offsetX: 0, offsetY: 0, effect: null },
  'sleepy_idle': { openness: 0.5, scaleX: 1.0, bottomLid: 0.3, offsetX: 0, offsetY: 24, effect: 'zzz' },
  'curious_idle': { openness: 1.15, scaleX: 1.06, bottomLid: 0.5, offsetX: 0, offsetY: 0, effect: null },
  'shy_idle': { openness: 0.75, scaleX: 0.98, bottomLid: 0.5, offsetX: -48, offsetY: 24, effect: 'blush' },
  'scanning_idle': { openness: 1.02, scaleX: 1.0, bottomLid: 0.5, offsetX: 0, offsetY: 0, effect: 'scan' },
  
  // COGNITIVE / INTERACTION
  'listening_focused': { openness: 1.1, scaleX: 1.08, bottomLid: 0.5, offsetX: 0, offsetY: -24, effect: null },
  'listening_confused': { openness: 1.05, scaleX: 1.02, bottomLid: 0.5, offsetX: 0, offsetY: 0, effect: 'confused' },
  'thinking': { openness: 0.8, scaleX: 0.98, bottomLid: 0.5, offsetX: -48, offsetY: -24, effect: 'thinking' },
  'processing': { openness: 0.95, scaleX: 1.0, bottomLid: 0.5, offsetX: 0, offsetY: 0, effect: 'processing' },
  'understanding': { openness: 1.08, scaleX: 1.05, bottomLid: 0.5, offsetX: 0, offsetY: 0, effect: 'understanding' },
  'misunderstanding': { openness: 0.9, scaleX: 0.99, bottomLid: 0.5, offsetX: 0, offsetY: 0, effect: 'misunderstanding' },
  'startled': { openness: 1.25, scaleX: 1.15, bottomLid: 0.5, offsetX: 0, offsetY: -24, effect: 'shock' },
  'refusing_gently': { openness: 0.8, scaleX: 0.98, bottomLid: 0.4, offsetX: 0, offsetY: 0, effect: 'refusing' },
  
  // EMOTIONAL REACTIONS
  'happy': { openness: 1.0, scaleX: 1.0, bottomLid: 0.8, offsetX: 0, offsetY: 0, effect: 'glow' },
  'shy_happy': { openness: 0.95, scaleX: 0.99, bottomLid: 0.7, offsetX: 48, offsetY: -24, effect: 'hearts' },
  'soft_sad': { openness: 0.55, scaleX: 0.88, bottomLid: 0.2, offsetX: 0, offsetY: 24, effect: null },
  'disappointed': { openness: 0.82, scaleX: 0.75, bottomLid: 0.3, offsetX: -48, offsetY: 0, effect: 'disappointed_drop' },
  'embarrassed': { openness: 0.6, scaleX: 0.97, bottomLid: 0.3, offsetX: 48, offsetY: 24, effect: 'embarrassed' },
  'playful_mischief': { openness: 1.1, scaleX: 1.04, bottomLid: 0.7, offsetX: 48, offsetY: -24, effect: 'sparkles' },
  
  // SYSTEM & SLEEP
  'sleeping_deep': { openness: 0.0, scaleX: 1.0, bottomLid: 0.5, offsetX: 0, offsetY: 0, effect: null },
  'waking_up_slow': { openness: 0.0, scaleX: 1.0, bottomLid: 0.5, offsetX: 0, offsetY: 0, effect: null },
  'resting_eyes': { openness: 0.4, scaleX: 1.0, bottomLid: 0.4, offsetX: 0, offsetY: 0, effect: null },
};

/**
 * Animate eye preview to match behavior
 * Uses the same rendering approach as the main demo
 */
function animateEyePreview(behavior) {
  const preview = document.getElementById('oledPreview');
  const leftEye = elements.eyeLeft;
  const rightEye = elements.eyeRight;
  
  if (!leftEye || !rightEye || !preview) {
    console.error('Eye preview elements not found:', { leftEye, rightEye, preview });
    return;
  }
  
  // Get behavior data
  const behaviorData = PREVIEW_BEHAVIORS[behavior] || PREVIEW_BEHAVIORS['calm_idle'];
  console.log('🎯 Animating preview:', behavior, behaviorData);
  
  // Set targets
  previewEyeState.targetOpenness = behaviorData.openness;
  previewEyeState.targetScaleX = behaviorData.scaleX;
  previewEyeState.targetBottomLid = behaviorData.bottomLid;
  previewEyeState.targetOffsetX = behaviorData.offsetX;
  previewEyeState.targetOffsetY = behaviorData.offsetY;
  
  // Clear existing effects
  clearPreviewEffects();
  
  // Create new effect
  if (behaviorData.effect) {
    createPreviewEffect(preview, behaviorData.effect);
  }
  
  // Start animation loop if not already running
  if (!previewEyeState.animating) {
    previewEyeState.animating = true;
    previewEyeState.lastFrameTime = performance.now();
    previewAnimationLoop();
  }
  
  // Auto-return to calm_idle after animation (except for calm_idle itself)
  if (behavior !== 'calm_idle') {
    // Calculate total animation time based on behavior
    const holdDuration = 1500; // Default hold time
    const animDuration = 300 + holdDuration + 300; // entry + hold + exit
    
    setTimeout(() => {
      if (state.currentBehavior === behavior) {
        // Return to calm
        previewEyeState.targetOpenness = 1.0;
        previewEyeState.targetScaleX = 1.0;
        previewEyeState.targetBottomLid = 0.5;
        previewEyeState.targetOffsetX = 0;
        previewEyeState.targetOffsetY = 0;
        state.currentBehavior = 'calm_idle';
        if (elements.currentBehavior) {
          elements.currentBehavior.textContent = 'calm_idle';
        }
        clearPreviewEffects();
      }
    }, animDuration);
  }
}

/**
 * Animation loop for eye preview (matches demo's approach)
 */
function previewAnimationLoop() {
  const now = performance.now();
  const dt = Math.min((now - previewEyeState.lastFrameTime) / 1000, 0.05);
  previewEyeState.lastFrameTime = now;
  
  // Exponential smoothing (same as demo)
  const smoothing = 12;
  const factor = 1 - Math.exp(-smoothing * dt);
  
  previewEyeState.openness += (previewEyeState.targetOpenness - previewEyeState.openness) * factor;
  previewEyeState.scaleX += (previewEyeState.targetScaleX - previewEyeState.scaleX) * factor;
  previewEyeState.bottomLid += (previewEyeState.targetBottomLid - previewEyeState.bottomLid) * factor;
  previewEyeState.offsetX += (previewEyeState.targetOffsetX - previewEyeState.offsetX) * factor;
  previewEyeState.offsetY += (previewEyeState.targetOffsetY - previewEyeState.offsetY) * factor;
  
  // Render eyes
  renderPreviewEyes();
  
  // Continue animation
  previewEyeState.animationId = requestAnimationFrame(previewAnimationLoop);
}

/**
 * Render preview eyes (matches demo's renderEyes function)
 */
function renderPreviewEyes() {
  const leftEye = elements.eyeLeft;
  const rightEye = elements.eyeRight;
  
  if (!leftEye || !rightEye) return;
  
  // Scale factor for preview (demo: 96x112 eye with ±48px offset, preview: 41x48 eye)
  // Scale = 41/96 ≈ 0.43, so 48px * 0.43 ≈ 20px max offset
  const offsetScale = 0.43;
  const offsetXpx = previewEyeState.offsetX * offsetScale;
  const offsetYpx = previewEyeState.offsetY * offsetScale;
  
  // Transform: translate + scaleX + scaleY(openness) - EXACTLY like demo
  const transform = `translate(${offsetXpx}px, ${offsetYpx}px) scaleX(${previewEyeState.scaleX}) scaleY(${previewEyeState.openness})`;
  
  leftEye.style.transform = transform;
  rightEye.style.transform = transform;
  
  // Happy smile curve using clip-path - ONLY for 'happy' behavior (like demo)
  // Demo checks: behaviorState.currentBehavior === 'happy' && eyeState.bottomLid > 0.5
  const isHappyBehavior = state.currentBehavior === 'happy';
  
  if (isHappyBehavior && previewEyeState.bottomLid > 0.5) {
    // Calculate smile curve depth (scaled for 41x48 eye)
    const curveAmount = (previewEyeState.bottomLid - 0.5) / 0.3;
    const maxCurveDepth = 9; // Scaled from 20px (20 * 0.43)
    const curveDepth = curveAmount * maxCurveDepth;
    
    // Eye dimensions: 41w x 48h
    const w = 41;
    const h = 48;
    const cornerRadius = 9; // Border radius scaled
    
    // Bottom edge position (moves up as curve increases)
    const bottomY = h - curveDepth;
    const controlY = bottomY - curveDepth; // Inward curve control point
    
    // SVG path for smile (inward curve at bottom)
    const clipPath = `path('M 0,${cornerRadius} Q 0,0 ${cornerRadius},0 L ${w-cornerRadius},0 Q ${w},0 ${w},${cornerRadius} L ${w},${bottomY} Q ${w/2},${controlY} 0,${bottomY} Z')`;
    
    leftEye.style.clipPath = clipPath;
    rightEye.style.clipPath = clipPath;
  } else {
    // Normal rectangle (no clip-path for shy_happy, playful_mischief, etc.)
    leftEye.style.clipPath = 'none';
    rightEye.style.clipPath = 'none';
  }
}

/**
 * Clear all preview effects from eyes
 */
function clearPreviewEffects() {
  document.querySelectorAll('.eye-effect').forEach(el => el.remove());
}

/**
 * Create preview effect (EXACTLY matches demo's createEffect function)
 * Effects are positioned relative to the eye so they move with eye transforms
 * Scale factor ~0.43 from demo (96x112 -> 41x48)
 */
function createPreviewEffect(container, effectType) {
  clearPreviewEffects();
  
  const leftEye = elements.eyeLeft;
  const rightEye = elements.eyeRight;
  if (!leftEye || !rightEye) return;
  
  // Create effect for both eyes (like demo)
  [leftEye, rightEye].forEach(eye => {
    const effectContainer = document.createElement('div');
    effectContainer.className = 'eye-effect';
    
    if (effectType === 'blush') {
      // Three horizontal lines below eye (anime style) - BOTH eyes
      for (let i = 0; i < 3; i++) {
        const line = document.createElement('div');
        line.className = 'blush-line';
        line.style.bottom = `${-6 - i * 2}px`;
        line.style.animationDelay = `${i * 0.05}s`;
        effectContainer.appendChild(line);
      }
    } else if (effectType === 'embarrassed') {
      // Four lines (more intense) + sweat drops - BOTH eyes
      for (let i = 0; i < 4; i++) {
        const line = document.createElement('div');
        line.className = 'blush-line blush-intense';
        line.style.bottom = `${-7 - i * 2}px`;
        line.style.animationDelay = `${i * 0.05}s`;
        effectContainer.appendChild(line);
      }
      // Add sweat drops
      for (let i = 0; i < 2; i++) {
        const drop = document.createElement('div');
        drop.className = 'sweat-drop';
        drop.style.left = `${20 + i * 60}%`;
        drop.style.animationDelay = `${i * 0.3}s`;
        effectContainer.appendChild(drop);
      }
    } else if (effectType === 'sparkles') {
      // White sparkle particles - BOTH eyes (4 sparkles at radius 50%)
      for (let i = 0; i < 4; i++) {
        const sparkle = document.createElement('div');
        sparkle.className = 'sparkle';
        const angle = (i / 4) * Math.PI * 2;
        const radius = 50;
        sparkle.style.left = `${50 + Math.cos(angle) * radius}%`;
        sparkle.style.top = `${50 + Math.sin(angle) * radius}%`;
        sparkle.style.animationDelay = `${i * 0.15}s`;
        effectContainer.appendChild(sparkle);
      }
    } else if (effectType === 'shock') {
      // Lightning bolts (4 rotated) - BOTH eyes
      for (let i = 0; i < 4; i++) {
        const bolt = document.createElement('div');
        bolt.className = 'lightning-bolt';
        bolt.style.transform = `rotate(${i * 90}deg)`;
        bolt.style.animationDelay = `${i * 0.03}s`;
        effectContainer.appendChild(bolt);
      }
    } else if (effectType === 'glow') {
      // White glow overlay - BOTH eyes
      const glow = document.createElement('div');
      glow.className = 'happy-glow';
      effectContainer.appendChild(glow);
    } else if (effectType === 'disappointed_drop') {
      // Falling sweat drop - RIGHT eye only
      if (eye === rightEye) {
        const drop = document.createElement('div');
        drop.className = 'disappointed-drop';
        effectContainer.appendChild(drop);
      }
    } else if (effectType === 'hearts') {
      // Floating hearts - RIGHT eye only (3 hearts with staggered delays)
      if (eye === rightEye) {
        for (let i = 0; i < 3; i++) {
          const heart = document.createElement('div');
          heart.className = 'floating-heart';
          heart.style.animationDelay = `${i * 0.3}s`;
          effectContainer.appendChild(heart);
        }
      }
    } else if (effectType === 'zzz') {
      // Floating Z's - RIGHT eye only (3 Z's with increasing size)
      if (eye === rightEye) {
        for (let i = 0; i < 3; i++) {
          const z = document.createElement('div');
          z.className = 'floating-z';
          z.textContent = 'Z';
          z.style.animationDelay = `${i * 0.6}s`;
          z.style.fontSize = `${8 + i * 3}px`;
          effectContainer.appendChild(z);
        }
      }
    } else if (effectType === 'scan') {
      // Scanning ray beam - BOTH eyes
      const beam = document.createElement('div');
      beam.className = 'scan-beam';
      effectContainer.appendChild(beam);
    } else if (effectType === 'confused') {
      // Question marks - RIGHT eye only (3 floating up)
      if (eye === rightEye) {
        for (let i = 0; i < 3; i++) {
          const qmark = document.createElement('div');
          qmark.className = 'question-mark';
          qmark.textContent = '?';
          qmark.style.animationDelay = `${i * 0.3}s`;
          qmark.style.fontSize = `${7 + i * 2}px`;
          qmark.style.right = `${-6 - i * 4}px`;
          qmark.style.top = `${-2 - i * 5}px`;
          effectContainer.appendChild(qmark);
        }
      }
    } else if (effectType === 'thinking') {
      // Ellipsis dots - RIGHT eye only
      if (eye === rightEye) {
        for (let i = 0; i < 3; i++) {
          const dot = document.createElement('div');
          dot.className = 'thinking-dot';
          dot.style.animationDelay = `${i * 0.2}s`;
          dot.style.right = `${-5 - i * 5}px`;
          dot.style.top = '-2px';
          effectContainer.appendChild(dot);
        }
      }
    } else if (effectType === 'processing') {
      // Spinning loader - RIGHT eye only
      if (eye === rightEye) {
        const spinner = document.createElement('div');
        spinner.className = 'processing-spinner';
        effectContainer.appendChild(spinner);
      }
    } else if (effectType === 'understanding') {
      // Checkmark - RIGHT eye only
      if (eye === rightEye) {
        const check = document.createElement('div');
        check.className = 'understanding-check';
        effectContainer.appendChild(check);
      }
    } else if (effectType === 'misunderstanding') {
      // X mark - RIGHT eye only
      if (eye === rightEye) {
        const xmark = document.createElement('div');
        xmark.className = 'misunderstanding-x';
        effectContainer.appendChild(xmark);
      }
    } else if (effectType === 'refusing') {
      // X mark for refusing - RIGHT eye only
      if (eye === rightEye) {
        const xmark = document.createElement('div');
        xmark.className = 'misunderstanding-x';
        effectContainer.appendChild(xmark);
      }
    }
    
    if (effectContainer.children.length > 0) {
      eye.appendChild(effectContainer);
    }
  });
}

/**
 * Update eye preview from robot eye state (real-time sync from WebSocket)
 * @param {Object} eyes - Eye state from robot {openness, offsetX, offsetY, scaleX, bottomLid}
 */
function updateEyePreview(eyes) {
  if (!eyes) return;
  
  // Update targets from robot state
  if (eyes.openness !== undefined) previewEyeState.targetOpenness = eyes.openness;
  if (eyes.scaleX !== undefined) previewEyeState.targetScaleX = eyes.scaleX;
  if (eyes.bottomLid !== undefined) previewEyeState.targetBottomLid = eyes.bottomLid;
  if (eyes.offsetX !== undefined) previewEyeState.targetOffsetX = eyes.offsetX;
  if (eyes.offsetY !== undefined) previewEyeState.targetOffsetY = eyes.offsetY;
  
  // Start animation if not running
  if (!previewEyeState.animating) {
    previewEyeState.animating = true;
    previewEyeState.lastFrameTime = performance.now();
    previewAnimationLoop();
  }
}

/**
 * Update LED state display
 * @param {string} color - LED color or 'off'
 */
function updateLedState(color) {
  state.ledColor = color;
  
  // Update button states
  elements.ledBtns.forEach(btn => {
    btn.classList.toggle('active', btn.dataset.color === color);
  });
}

/**
 * Update sensor displays
 * @param {Object} sensors - Sensor data object
 */
function updateSensors(sensors) {
  if (sensors.light !== undefined) {
    state.sensors.light = sensors.light;
    elements.sensorLight.textContent = sensors.light;
  }
  
  if (sensors.motion !== undefined) {
    state.sensors.motion = sensors.motion;
    elements.sensorMotion.textContent = sensors.motion;
  }
  
  if (sensors.sound !== undefined) {
    state.sensors.sound = sensors.sound;
    elements.sensorSound.textContent = sensors.sound;
  }
  
  if (sensors.touch !== undefined) {
    state.sensors.touch = sensors.touch;
    elements.sensorTouch.textContent = sensors.touch;
  }
  
  if (sensors.distance !== undefined) {
    state.sensors.distance = sensors.distance;
    if (elements.sensorDistance) {
      elements.sensorDistance.textContent = 
        typeof sensors.distance === 'number' ? `${sensors.distance}cm` : sensors.distance;
    }
  }
}

/**
 * Add message to chat
 * @param {string} from - 'robot' or 'user'
 * @param {string} text - Message text
 */
function addMessage(from, text) {
  // Create message element
  const messageDiv = document.createElement('div');
  messageDiv.className = `message ${from}`;
  
  const contentDiv = document.createElement('div');
  contentDiv.className = 'message-content';
  contentDiv.textContent = text;
  
  const timeDiv = document.createElement('div');
  timeDiv.className = 'message-time';
  timeDiv.textContent = formatTime(new Date());
  
  messageDiv.appendChild(contentDiv);
  messageDiv.appendChild(timeDiv);
  
  // Add to DOM
  elements.chatMessages.appendChild(messageDiv);
  
  // Store in state
  state.messages.push({ from, text, timestamp: new Date() });
  
  // Limit stored messages
  if (state.messages.length > CONFIG.MAX_MESSAGES) {
    state.messages.shift();
    elements.chatMessages.firstChild?.remove();
  }
  
  // Auto-scroll to bottom
  elements.chatMessages.scrollTop = elements.chatMessages.scrollHeight;
}

/**
 * Handle robot events (motion detected, light change, etc.)
 * @param {string} eventType - Type of event
 * @param {Object} details - Event details
 */
function handleRobotEvent(eventType, details) {
  console.log('🔔 Robot event:', eventType, details);
  
  // You can add visual feedback or notifications here
  // For example, show a system message in chat:
  // addMessage('robot', `[Event: ${eventType}]`);
}

// ================================================
// USER INTERACTION HANDLERS
// ================================================

/**
 * Send message to chat API and handle response
 * @param {string} text - User message
 * @param {string} inputMethod - 'text' or 'voice'
 */
async function sendChatMessage(text, inputMethod = 'text') {
  if (!text.trim()) return;
  
  // Add user message to chat
  addMessage('user', text);
  
  // Show typing indicator
  updateRobotState('THINKING', 'Thinking...');
  
  try {
    // Call the chat API
    const response = await fetch(`${CONFIG.API_BASE}/api/chat`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ message: text })
    });
    
    if (!response.ok) {
      throw new Error(`API error: ${response.status}`);
    }
    
    const data = await response.json();
    console.log('📨 Chat response:', data);
    
    // Add bot response to chat
    addMessage('robot', data.response);
    
    // Update robot state based on emotion
    if (data.emotion) {
      updateRobotState('SPEAKING', data.emotion);
    }
    
    // Play audio if available, otherwise use browser TTS
    if (data.audio) {
      playAudioFromServer(data.audio);
    } else {
      speakText(data.response);
    }
    
    // Also send to robot via WebSocket (for expression sync)
    if (state.isConnected && state.ws) {
      sendMessage({
        type: 'behavior',
        name: data.emotion || 'neutral'
      });
    }
    
  } catch (error) {
    console.error('Chat API error:', error);
    // Fallback: use local response
    const fallbackResponse = "Sorry, I'm having trouble thinking right now. Please try again!";
    addMessage('robot', fallbackResponse);
    speakText(fallbackResponse);
  } finally {
    updateRobotState('IDLE', 'Ready');
  }
}

/**
 * Play audio file from server
 * @param {string} audioUrl - URL to audio file
 */
function playAudioFromServer(audioUrl) {
  const audio = new Audio(audioUrl);
  audio.onplay = () => updateRobotState('SPEAKING', 'Speaking');
  audio.onended = () => updateRobotState('IDLE', 'Ready');
  audio.onerror = () => {
    console.warn('Audio playback failed, falling back to TTS');
    updateRobotState('IDLE', 'Ready');
  };
  audio.play().catch(err => {
    console.warn('Audio autoplay blocked:', err);
    updateRobotState('IDLE', 'Ready');
  });
}

/**
 * Handle send button click
 */
function handleSendClick() {
  const text = elements.messageInput.value.trim();
  
  if (!text) return;
  
  // Clear input
  elements.messageInput.value = '';
  elements.messageInput.focus();
  
  // Send to chat API
  sendChatMessage(text, 'text');
}

/**
 * Handle Enter key in input field
 * @param {KeyboardEvent} event - Keyboard event
 */
function handleInputKeyPress(event) {
  if (event.key === 'Enter' && !event.shiftKey) {
    event.preventDefault();
    handleSendClick();
  }
}

/**
 * Handle microphone button press (start listening)
 */
function handleMicDown(event) {
  if (!speechRecognitionAvailable || !recognition) {
    console.warn('Speech recognition not available');
    return;
  }

  event.preventDefault();
  recognition.start();
}

/**
 * Handle microphone button release (stop listening)
 */
function handleMicUp(event) {
  event.preventDefault();
  if (isRecording && recognition) {
    recognition.stop();
  }
}

/**
 * Process recognized voice input
 * This function handles the text recognized by Web Speech API
 * @param {string} text - Recognized speech text
 */
function handleUserVoice(text) {
  if (!text.trim()) {
    console.warn('Empty speech input');
    return;
  }

  console.log('🎤 Voice input:', text);

  // Send to chat API (handles adding to chat, getting response, etc.)
  sendChatMessage(text, 'voice');
}

// ================================================
// UTILITY FUNCTIONS
// ================================================

/**
 * Format time for message display
 * @param {Date} date - Date object
 * @returns {string} Formatted time string
 */
function formatTime(date) {
  const now = new Date();
  const diff = now - date;
  
  if (diff < 60000) return 'Just now';
  if (diff < 3600000) return `${Math.floor(diff / 60000)}m ago`;
  
  return date.toLocaleTimeString('en-US', { 
    hour: 'numeric', 
    minute: '2-digit',
    hour12: true 
  });
}

// ================================================
// INITIALIZATION
// ================================================

/**
 * Initialize the application
 */
function init() {
  console.log('🚀 EMO DeskBot Dashboard initialized');
  
  // Attach event listeners for text input
  elements.sendButton.addEventListener('click', handleSendClick);
  elements.messageInput.addEventListener('keypress', handleInputKeyPress);
  
  // Attach event listeners for microphone (mobile only)
  if (speechRecognitionAvailable) {
    // Press-and-hold behavior: start on mousedown/touchstart, stop on mouseup/touchend
    elements.micButton.addEventListener('mousedown', handleMicDown);
    elements.micButton.addEventListener('touchstart', handleMicDown);
    
    document.addEventListener('mouseup', handleMicUp);
    document.addEventListener('touchend', handleMicUp);
  } else {
    // Disable mic button if not supported
    elements.micButton.disabled = true;
    elements.micButton.title = 'Speech recognition not supported on this browser';
  }
  
  // Behavior quick action buttons
  elements.actionBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const behavior = btn.dataset.behavior;
      if (behavior) {
        triggerBehavior(behavior);
      }
    });
  });
  
  // LED color buttons
  elements.ledBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const color = btn.dataset.color;
      setLedColor(color);
    });
  });
  
  // Servo control buttons
  if (elements.servoLeft) {
    elements.servoLeft.addEventListener('click', () => setServoAngle(45));
  }
  if (elements.servoCenter) {
    elements.servoCenter.addEventListener('click', () => setServoAngle(90));
  }
  if (elements.servoRight) {
    elements.servoRight.addEventListener('click', () => setServoAngle(135));
  }
  
  // Initialize eye preview with calm_idle state
  renderPreviewEyes();
  
  // Attempt WebSocket connection
  // NOTE: This will fail until you have a WebSocket server running
  // Comment out the line below if you want to test the UI without a server
  connectWebSocket();
  
  // For development/testing without a server, uncomment this:
  // simulateRobotConnection();
}

// ================================================
// ROBOT CONTROL FUNCTIONS
// ================================================

/**
 * Trigger a behavior on the robot
 * @param {string} behavior - Behavior name
 */
function triggerBehavior(behavior) {
  console.log('🎭 Triggering behavior:', behavior);
  
  // Update local preview immediately
  updateBehavior(behavior);
  
  // Send to robot
  sendMessage({
    type: 'trigger_behavior',
    behavior: behavior,
    timestamp: new Date().toISOString(),
  });
}

/**
 * Set LED color
 * @param {string} color - Hex color or 'off'
 */
function setLedColor(color) {
  console.log('💡 Setting LED color:', color);
  
  // Update local state
  updateLedState(color);
  
  // Send to robot
  sendMessage({
    type: 'set_led',
    color: color,
    timestamp: new Date().toISOString(),
  });
}

/**
 * Set servo angle for head movement
 * @param {number} angle - Angle in degrees (0-180)
 */
function setServoAngle(angle) {
  console.log('⚙️ Setting servo angle:', angle);
  
  state.servoAngle = angle;
  
  // Send to robot
  sendMessage({
    type: 'set_servo',
    angle: angle,
    timestamp: new Date().toISOString(),
  });
}

// ================================================
// DEVELOPMENT/TESTING HELPERS
// ================================================

/**
 * Simulate robot connection for testing UI without backend
 * Uncomment to use for development
 */
function simulateRobotConnection() {
  console.log('🧪 Running in simulation mode (no WebSocket server)');
  
  // Simulate connection
  setTimeout(() => {
    state.isConnected = true;
    updateConnectionStatus('connected');
    updateRobotState('IDLE', 'calm_idle');
    updateSensors({
      light: 'Bright',
      motion: 'None',
      sound: 'Quiet',
      touch: 'None',
      distance: '45cm',
    });
  }, 1000);
  
  // Simulate robot-initiated message with speech
  setTimeout(() => {
    addMessage('robot', 'வணக்கம்! I\'m EMO, your desk companion! 😊');
    // Speak the message using TTS
    speakText('வணக்கம்! I am EMO, your desk companion!');
  }, 3000);
  
  // Simulate sensor updates
  setInterval(() => {
    if (state.isConnected) {
      updateSensors({
        distance: `${Math.round(20 + Math.random() * 80)}cm`,
        light: Math.random() > 0.5 ? 'Bright' : 'Normal',
      });
    }
  }, 5000);
  
  // Simulate random blinks
  setInterval(() => {
    if (state.isConnected && state.currentBehavior === 'calm_idle') {
      simulateBlink();
    }
  }, 4000 + Math.random() * 4000);
}

/**
 * Simulate a blink animation in the eye preview
 */
function simulateBlink() {
  const leftEye = elements.eyeLeft;
  const rightEye = elements.eyeRight;
  
  if (!leftEye || !rightEye) return;
  
  leftEye.classList.add('blinking');
  rightEye.classList.add('blinking');
  
  setTimeout(() => {
    leftEye.classList.remove('blinking');
    rightEye.classList.remove('blinking');
  }, 150);
}

/**
 * TEST FUNCTION: Try different behaviors
 * Call this in browser console to test:
 * 
 * testBehavior('happy');
 * testBehavior('sleepy_idle');
 */
function testBehavior(behavior) {
  if (!state.isConnected) {
    state.isConnected = true;
    updateConnectionStatus('connected');
  }
  
  triggerBehavior(behavior);
}

/**
 * TEST FUNCTION: Try different languages
 * Call this in browser console to test:
 * 
 * testSpeech('en-US');  // English
 * testSpeech('ta-IN');  // Tamil
 * testSpeech('mix');    // Mixed Tanglish
 */
function testSpeech(lang = 'en-US') {
  if (!state.isConnected) {
    state.isConnected = true;
    updateConnectionStatus('connected');
  }

  const testTexts = {
    'en-US': 'Hello! I am DeskBot, your friendly robot companion.',
    'ta-IN': 'வணக்கம்! நான் உங்கள் நண்பன்.',
    'mix': 'நீ ஒரு really good friend. எப்படி இருக்கீங்க?',
  };

  const text = testTexts[lang] || testTexts['en-US'];
  console.log('Testing speech:', text);
  speakText(text);
}

// Start the application when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}

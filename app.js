/**
 * ═══════════════════════════════════════════════════════════════════════════
 * UNIFIED EYE ANIMATION ENGINE - EMO ROBOT BEHAVIORAL SYSTEM
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * ARCHITECTURE (ESP32 C++ Ready):
 * 1. Single RAF loop: ONE requestAnimationFrame @ 60 FPS
 * 2. Unified eye state: Single source of truth (eyeState object)
 * 3. Deterministic interpolation: All motion uses lerp + easing
 * 4. Motion-based expression: Pure openness/offsetX/scaleX/topLid/bottomLid
 * 5. ESP32 portable: Pure math, no browser-specific tricks
 *
 * BEHAVIOR LIFECYCLE:
 * Phase | Duration      | Action
 * ------|---------------|-------
 * Entry | N ms          | Interpolate from current to entry target + easing
 * Hold  | N ms          | Maintain entry target state (stable)
 * Exit  | N ms          | Interpolate from hold target to exit target
 * → Auto-return to calm_idle
 *
 * ANIMATION SYSTEM:
 * • Blinking: Automatic system with personality modes (NO_BLINK_BEHAVIORS list)
 * • Eye Movement: Random drift during idle (DRIFT_BEHAVIORS: calm_idle, curious_idle)
 * • Visual Effects: 14 unique animations (hearts, sparkles, Z's, particles, etc.)
 * • Eyelid Curves: SVG clip-path for happy smile (topLid/bottomLid parameters)
 *
 * CONFLICT PREVENTION:
 * • NO_BLINK_BEHAVIORS: Prevents blinking during visual effects
 * • clearEffect(): Clears all animations when switching behaviors
 * • Priority system: Higher priority behaviors interrupt lower ones
 * • Token system: Prevents race conditions in async animations
 *
 * 22 CORE BEHAVIORS:
 * Idle (5): calm, sleepy, curious, shy, scanning
 * Cognitive (8): listening, confused, thinking, processing, understanding,
 *               misunderstanding, refusing, startled
 * Emotional (6): happy, shy_happy, embarrassed, sad, disappointed, mischief
 * System (3): deep_sleep, waking_up, resting_eyes
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

// ============================================================================
// DISPLAY CONFIG & GLOBALS
// ============================================================================

const CONFIG = {
    DISPLAY_WIDTH: 128,
    DISPLAY_HEIGHT: 64,
};

// ============================================================================
// ROBOEYES-STYLE EYE MOVEMENT CONSTANTS (MANDATORY - DO NOT MODIFY)
// ============================================================================

// Maximum eye travel range in pixels (absolute coordinates)
const EYE_MAX_X = 48;   // Horizontal edge reach (±48px from center)
const EYE_MAX_Y = 24;   // Vertical edge reach (±24px from center)

// Discrete gaze positions (RoboEyes fixed-position model)
const GAZE = {
  LOOK_CENTER:       { x: 0,          y: 0 },
  LOOK_LEFT:         { x: -EYE_MAX_X, y: 0 },
  LOOK_RIGHT:        { x: +EYE_MAX_X, y: 0 },
  LOOK_UP:           { x: 0,          y: -EYE_MAX_Y },
  LOOK_DOWN:         { x: 0,          y: +EYE_MAX_Y },
  LOOK_TOP_LEFT:     { x: -EYE_MAX_X, y: -EYE_MAX_Y },
  LOOK_TOP_RIGHT:    { x: +EYE_MAX_X, y: -EYE_MAX_Y },
  LOOK_BOTTOM_LEFT:  { x: -EYE_MAX_X, y: +EYE_MAX_Y },
  LOOK_BOTTOM_RIGHT: { x: +EYE_MAX_X, y: +EYE_MAX_Y },
};

let RANDOMNESS_ENABLED = true;

// Global state tracking
let animationLoopId = null;
let currentBehaviorToken = 0;
let randomnessSchedulerId = null;

const behaviorState = {
  currentBehavior: 'calm_idle',
  currentPhase: null,
  phaseStartTime: 0,
  currentToken: 0,
  priority: 0,
};

// Activity prop system for EMO-style expressions
const propState = {
  phase: 0,     // For oscillation animation
  mic: 0,       // Opacity 0-1
  bowl: 0,      // Opacity 0-1
  thread: 0,    // Opacity 0-1
  blush: 0,     // Opacity 0-1
};;

// ============================================================================
// MANDATORY: UNIFIED EYE STATE (Single Source of Truth)
// ============================================================================

/**
 * ALL rendering comes from this object, ONLY.
 * No hidden state, no direct DOM manipulation.
 * Pure mathematics: current → target interpolation.
 * 
 * COORDINATE SYSTEM (RoboEyes-compatible):
 * - offsetX/offsetY are ABSOLUTE PIXEL values, not normalized
 * - offsetX range: -48px to +48px (EYE_MAX_X)
 * - offsetY range: -24px to +24px (EYE_MAX_Y)
 * - (0, 0) = center gaze
 */
const eyeState = {
  // Current values (being smoothly interpolated via exponential smoothing)
  openness: 1.0,      // 0.0 (closed) to 1.25 (very wide)
  offsetX: 0.0,       // ABSOLUTE PIXELS: -48 to +48 (horizontal gaze)
  offsetY: 0.0,       // ABSOLUTE PIXELS: -24 to +24 (vertical gaze)
  scaleX: 1.0,        // 0.8 to 1.3 (horizontal scale)
  topLid: 0.5,        // 0.0 (flat/angry) to 1.0 (highly curved/surprised)
  bottomLid: 0.5,     // 0.0 (flat/sad) to 1.0 (curved up/happy)

  // Target values (animation loop exponentially smooths toward these)
  targetOpenness: 1.0,
  targetOffsetX: 0.0,  // ABSOLUTE PIXELS
  targetOffsetY: 0.0,  // ABSOLUTE PIXELS
  targetScaleX: 1.0,
  targetTopLid: 0.5,
  targetBottomLid: 0.5,

  // Delta-time smoothing factors (exponential decay)
  // Formula: value += (target - value) * (1 - exp(-smoothing * dt))
  // Higher = faster response, lower = smoother
  smoothing: {
    openness: 12,    // Speed of eye openness changes
    offsetX: 15,     // Speed of horizontal gaze (faster for RoboEyes snap)
    offsetY: 15,     // Speed of vertical gaze (faster for RoboEyes snap)
    scaleX: 12,      // Speed of scale changes
    topLid: 3,       // Speed of top eyelid curve changes (ultra slow = ultra smooth)
    bottomLid: 3,    // Speed of bottom eyelid curve changes (ultra slow = ultra smooth)
  },

  // Rhythmic modulation (singing, chewing)
  rhythmic: {
    active: false,
    baseValue: 0,
    amplitude: 0,
    frequency: 0,
    startTime: 0,
  },

  // Frame timing (delta-time based)
  lastFrameTime: 0,
};

// ============================================================================
// BLINKING SYSTEM (Global, RoboEyes-style with Personality)
// ============================================================================

const blinkState = {
  nextBlinkTime: 0,
  isBlinking: false,
  blinkStartTime: 0,
  blinkDuration: 150, // ms for full blink cycle
  
  // Personality-driven blinking
  canBlink: true,
  personalityMode: 'calm', // calm, focused, nervous, sleepy
  consecutiveBlinks: 0,
  lastBlinkTime: 0,
};

// Blinking personality profiles (realistic human-like patterns)
const BLINK_PROFILES = {
  calm: {
    minInterval: 4000,   // 4-8 seconds (relaxed, infrequent)
    maxInterval: 8000,
    doubleBlink: 0.1,    // 10% chance of double blink
  },
  focused: {
    minInterval: 8000,   // 8-15 seconds (concentrating, very rare)
    maxInterval: 15000,
    doubleBlink: 0.05,   // Rarely double blink when focused
  },
  nervous: {
    minInterval: 1500,   // 1.5-3.5 seconds (anxious, frequent)
    maxInterval: 3500,
    doubleBlink: 0.3,    // 30% chance of double blink
  },
  sleepy: {
    minInterval: 6000,   // 6-12 seconds (slow, heavy blinks)
    maxInterval: 12000,
    doubleBlink: 0.05,
  },
  excited: {
    minInterval: 2500,   // 2.5-5 seconds (energetic)
    maxInterval: 5000,
    doubleBlink: 0.2,
  },
};

// ============================================================================
// EYE MOVEMENT PERSONALITY & RULES
// ============================================================================

/* C++ PORT NOTES:
 * eyePersonality struct controls drift behavior and blinking patterns
 * All values are float 0-1 for easy mapping to robot personality profiles
 * driftInterval: milliseconds between position changes (convert to millis())
 */

const eyePersonality = {
  // Current emotional context
  emotionalState: 'neutral', // neutral, happy, sad, curious, shy, focused
  
  // Movement tendencies
  restlessness: 0.5,  // 0-1: How often eyes drift when idle
  attentiveness: 0.7, // 0-1: How focused gaze is
  shyness: 0.3,       // 0-1: Tendency to look away
  
  // Drift behavior (subtle eye movements during idle)
  driftEnabled: true,
  lastDriftTime: 0,
  driftInterval: 3000, // Check for drift every 3 seconds
};

/* C++ PORT NOTES - CONFLICT PREVENTION:
 * NO_BLINK_BEHAVIORS: Array of behavior names that disable automatic blinking
 * Critical for preventing blink interruption during visual effects/animations
 * In C++: Use const char* array or std::vector<std::string>
 */

// Rules: Which behaviors should NOT blink
const NO_BLINK_BEHAVIORS = [
  'listening_focused', 
  'processing',         // Processing spinner animation
  'thinking',           // Thinking dots animation
  'understanding',      // Checkmark animation
  'happy',              // Smile curve animation
  'scanning_idle',      // Scanning beam animation
  'listening_confused', // Question marks animation
  'misunderstanding',   // X mark animation
  'refusing_gently',    // X morph animation
  'disappointed',       // Sweat drop animation
  'sleepy_idle',        // Floating Z's animation
  'shy_happy',          // Blush lines animation
  'startled',           // Shock lines animation
  'embarrassed',        // Blush + sweat animation
  'playful_mischief',   // Sparkles animation
];

// Rules: Behaviors with faster blinking
const FAST_BLINK_BEHAVIORS = [
  'shy', 'shy_idle', 'shy_happy',
  'embarrassed',
  'startled',
  'nervous',
];

// Rules: Behaviors with slower blinking  
const SLOW_BLINK_BEHAVIORS = [
  'sleepy_idle',
  'resting_eyes',
  'sleeping_deep',
  // calm_idle removed - uses drift system instead
];

// Rules: Behaviors that should have active eye drift
const DRIFT_BEHAVIORS = [
  'calm_idle',
  'curious_idle',
  // scanning_idle removed - uses fixed scanning beam instead of drift
];

function scheduleNextBlink(currentTime) {
  const profile = BLINK_PROFILES[blinkState.personalityMode] || BLINK_PROFILES.calm;
  let interval = profile.minInterval + Math.random() * (profile.maxInterval - profile.minInterval);
  
  // Add natural variation (±20%)
  const variation = 0.8 + Math.random() * 0.4;
  interval *= variation;
  
  // Check for double blink pattern (realistic human behavior)
  if (Math.random() < profile.doubleBlink && blinkState.consecutiveBlinks === 0) {
    // Schedule quick second blink
    interval = 300 + Math.random() * 200; // 300-500ms for double blink
    blinkState.consecutiveBlinks = 1;
  } else {
    blinkState.consecutiveBlinks = 0;
  }
  
  blinkState.nextBlinkTime = currentTime + interval;
  blinkState.lastBlinkTime = currentTime;
}

// ============================================================================
// VISUAL EFFECTS SYSTEM (Pure CSS/JS, no images)
// ============================================================================

const effectState = {
  active: null,
  container: null,
};

function createEffect(type) {
  clearEffect();
  
  const leftEye = document.getElementById('eyeLeft');
  const rightEye = document.getElementById('eyeRight');
  
  if (!leftEye || !rightEye) return;
  
  // Add shake animation to eyes for misunderstanding
  if (type === 'misunderstanding') {
    leftEye.classList.add('eye-shake');
    rightEye.classList.add('eye-shake');
    // Remove shake class after animation completes
    setTimeout(() => {
      leftEye.classList.remove('eye-shake');
      rightEye.classList.remove('eye-shake');
    }, 600);
  }
  
  // Add X-fade animation for refusing
  if (type === 'refusing') {
    // Create X shape overlays on top of eyes
    const display = document.querySelector('.display');
    
    [leftEye, rightEye].forEach(eye => {
      // Add fade-out class to eye background
      eye.classList.add('eye-fade-out');
      
      // Create X overlay positioned absolutely over this eye
      const xShape = document.createElement('div');
      xShape.className = 'x-shape-overlay';
      
      // Position X exactly over the eye
      const rect = eye.getBoundingClientRect();
      const displayRect = display.getBoundingClientRect();
      xShape.style.left = (rect.left - displayRect.left) + 'px';
      xShape.style.top = (rect.top - displayRect.top) + 'px';
      xShape.style.width = rect.width + 'px';
      xShape.style.height = rect.height + 'px';
      
      display.appendChild(xShape);
    });
    
    // Remove animation and X shapes after full cycle
    setTimeout(() => {
      leftEye.classList.remove('eye-fade-out');
      rightEye.classList.remove('eye-fade-out');
      document.querySelectorAll('.x-shape-overlay').forEach(x => x.remove());
    }, 1400);
    
    // Don't create additional effect containers for refusing
    effectState.active = type;
    return;
  }
  
  // Create effect for both eyes
  [leftEye, rightEye].forEach(eye => {
    const container = document.createElement('div');
    container.className = `effect-${type} eye-effect`;
    
    if (type === 'blush') {
      // Three horizontal lines stacked below eye (anime/manga style)
      for (let i = 0; i < 3; i++) {
        const line = document.createElement('div');
        line.className = 'blush-line';
        line.style.bottom = `${-15 - i * 4}px`;
        line.style.animationDelay = `${i * 0.05}s`;
        container.appendChild(line);
      }
    } else if (type === 'embarrassed') {
      // Four horizontal lines (more intense) + sweat drops
      for (let i = 0; i < 4; i++) {
        const line = document.createElement('div');
        line.className = 'blush-line blush-intense';
        line.style.bottom = `${-18 - i * 4}px`;
        line.style.animationDelay = `${i * 0.05}s`;
        container.appendChild(line);
      }
      // Add sweat drops
      for (let i = 0; i < 2; i++) {
        const drop = document.createElement('div');
        drop.className = 'sweat-drop';
        drop.style.left = `${20 + i * 60}%`;
        drop.style.animationDelay = `${i * 0.3}s`;
        container.appendChild(drop);
      }
    } else if (type === 'sparkles') {
      // White sparkle particles
      for (let i = 0; i < 4; i++) {
        const sparkle = document.createElement('div');
        sparkle.className = 'sparkle';
        const angle = (i / 4) * Math.PI * 2;
        const radius = 50;
        sparkle.style.left = `${50 + Math.cos(angle) * radius}%`;
        sparkle.style.top = `${50 + Math.sin(angle) * radius}%`;
        sparkle.style.animationDelay = `${i * 0.15}s`;
        container.appendChild(sparkle);
      }
    } else if (type === 'shock') {
      // Lightning bolt effect (zigzag lines)
      for (let i = 0; i < 4; i++) {
        const bolt = document.createElement('div');
        bolt.className = 'lightning-bolt';
        const angle = i * 90;
        bolt.style.transform = `rotate(${angle}deg)`;
        bolt.style.animationDelay = `${i * 0.03}s`;
        container.appendChild(bolt);
      }
    } else if (type === 'glow') {
      // White glow overlay
      const glow = document.createElement('div');
      glow.className = 'happy-glow';
      container.appendChild(glow);
    } else if (type === 'disappointed_drop') {
      // Falling sweat drop for disappointed (only on right eye)
      if (eye === rightEye) {
        const drop = document.createElement('div');
        drop.className = 'disappointed-drop';
        container.appendChild(drop);
      }
    } else if (type === 'hearts') {
      // Floating hearts for shy_happy (only on right eye)
      if (eye === rightEye) {
        for (let i = 0; i < 3; i++) {
          const heart = document.createElement('div');
          heart.className = 'floating-heart';
          heart.style.animationDelay = `${i * 0.3}s`;
          heart.textContent = '♥';
          container.appendChild(heart);
        }
      }
    } else if (type === 'zzz') {
      // Floating Z's for sleepy (only on right eye)
      if (eye === rightEye) {
        for (let i = 0; i < 3; i++) {
          const z = document.createElement('div');
          z.className = 'floating-z';
          z.textContent = 'Z';
          z.style.animationDelay = `${i * 0.6}s`;
          z.style.fontSize = `${18 + i * 6}px`;
          container.appendChild(z);
        }
      }
    } else if (type === 'scan') {
      // Sci-fi scanning ray beam
      const beam = document.createElement('div');
      beam.className = 'scan-beam';
      container.appendChild(beam);
    } else if (type === 'confused') {
      // Question marks for confusion (only on right eye)
      if (eye === rightEye) {
        for (let i = 0; i < 3; i++) {
          const qmark = document.createElement('div');
          qmark.className = 'question-mark';
          qmark.textContent = '?';
          qmark.style.animationDelay = `${i * 0.3}s`;
          qmark.style.fontSize = `${16 + i * 4}px`;
          qmark.style.right = `${-15 - i * 8}px`;
          qmark.style.top = `${-5 - i * 10}px`;
          container.appendChild(qmark);
        }
      }
    } else if (type === 'thinking') {
      // Ellipsis dots for thinking (only on right eye)
      if (eye === rightEye) {
        for (let i = 0; i < 3; i++) {
          const dot = document.createElement('div');
          dot.className = 'thinking-dot';
          dot.textContent = '•';
          dot.style.animationDelay = `${i * 0.2}s`;
          dot.style.right = `${-10 - i * 12}px`;
          dot.style.top = `${-5}px`;
          container.appendChild(dot);
        }
      }
    } else if (type === 'processing') {
      // Spinning loader for processing (only on right eye)
      if (eye === rightEye) {
        const spinner = document.createElement('div');
        spinner.className = 'processing-spinner';
        container.appendChild(spinner);
      }
    } else if (type === 'understanding') {
      // Light bulb or checkmark for understanding (only on right eye)
      if (eye === rightEye) {
        const check = document.createElement('div');
        check.className = 'understanding-check';
        check.textContent = '✓';
        container.appendChild(check);
      }
    } else if (type === 'misunderstanding') {
      // X mark for misunderstanding (only on right eye)
      if (eye === rightEye) {
        const xmark = document.createElement('div');
        xmark.className = 'misunderstanding-x';
        xmark.textContent = '✗';
        container.appendChild(xmark);
      }
    }
    
    eye.appendChild(container);
  });
  
  effectState.active = type;
}

function clearEffect() {
  const existingEffects = document.querySelectorAll('.eye-effect');
  existingEffects.forEach(effect => effect.remove());
  effectState.active = null;
  
  // Also clear special animation classes and elements
  const leftEye = document.getElementById('eyeLeft');
  const rightEye = document.getElementById('eyeRight');
  if (leftEye) {
    leftEye.classList.remove('eye-shake', 'eye-fade-out');
  }
  if (rightEye) {
    rightEye.classList.remove('eye-shake', 'eye-fade-out');
  }
  
  // Remove X-shape overlays from refusing animation
  document.querySelectorAll('.x-shape-overlay').forEach(x => x.remove());
}

function updateBlinkPersonality(behaviorName) {
  // Determine personality mode based on behavior
  if (FAST_BLINK_BEHAVIORS.some(b => behaviorName.includes(b))) {
    blinkState.personalityMode = 'nervous';
    blinkState.canBlink = true;
    eyePersonality.emotionalState = 'shy';
  } else if (NO_BLINK_BEHAVIORS.includes(behaviorName)) {
    blinkState.personalityMode = 'focused';
    blinkState.canBlink = false;
    eyePersonality.emotionalState = 'focused';
    eyePersonality.driftEnabled = false;
  } else if (SLOW_BLINK_BEHAVIORS.includes(behaviorName)) {
    blinkState.personalityMode = 'sleepy';
    blinkState.canBlink = true;
    eyePersonality.emotionalState = 'sleepy';
    eyePersonality.driftEnabled = false;
  } else if (behaviorName.includes('happy') || behaviorName === 'playful_mischief') {
    blinkState.personalityMode = 'excited';
    blinkState.canBlink = true;
    eyePersonality.emotionalState = 'happy';
  } else if (behaviorName.includes('sad') || behaviorName === 'disappointed') {
    blinkState.personalityMode = 'calm';
    blinkState.canBlink = true;
    eyePersonality.emotionalState = 'sad';
    eyePersonality.driftEnabled = false;
  } else if (DRIFT_BEHAVIORS.includes(behaviorName)) {
    // Special handling for drift behaviors (calm_idle, curious_idle, scanning_idle)
    blinkState.personalityMode = 'calm';
    blinkState.canBlink = true;
    eyePersonality.emotionalState = behaviorName.includes('curious') ? 'curious' : 'neutral';
    eyePersonality.driftEnabled = true;
  } else if (behaviorName.includes('curious') || behaviorName.includes('scanning')) {
    blinkState.personalityMode = 'calm';
    blinkState.canBlink = true;
    eyePersonality.emotionalState = 'curious';
    eyePersonality.driftEnabled = true;
  } else {
    // Default calm state
    blinkState.personalityMode = 'calm';
    blinkState.canBlink = true;
    eyePersonality.emotionalState = 'neutral';
    eyePersonality.driftEnabled = DRIFT_BEHAVIORS.includes(behaviorName);
    console.log(`🔍 updateBlinkPersonality("${behaviorName}"): driftEnabled = ${eyePersonality.driftEnabled} (in DRIFT_BEHAVIORS: ${DRIFT_BEHAVIORS.includes(behaviorName)})`);
  }
}

function processBlink(currentTime) {
  // Don't blink during extreme look directions
  const isExtremeLook = Math.abs(eyeState.offsetX) > 40 || Math.abs(eyeState.offsetY) > 20;
  
  if (!blinkState.canBlink || isExtremeLook) {
    return eyeState.targetOpenness; // Return current target, no blink
  }
  
  // Check if it's time to start a new blink
  if (!blinkState.isBlinking && currentTime >= blinkState.nextBlinkTime) {
    blinkState.isBlinking = true;
    blinkState.blinkStartTime = currentTime;
  }
  
  // Process active blink
  if (blinkState.isBlinking) {
    const elapsed = currentTime - blinkState.blinkStartTime;
    
    if (elapsed >= blinkState.blinkDuration) {
      // Blink complete
      blinkState.isBlinking = false;
      scheduleNextBlink(currentTime);
      return eyeState.targetOpenness;
    } else {
      // During blink - quick down and up
      const progress = elapsed / blinkState.blinkDuration;
      // Use sine wave for smooth blink: fully closed at 50% progress
      const blinkCurve = Math.sin(progress * Math.PI);
      return eyeState.targetOpenness * (1 - blinkCurve * 0.9); // Close to ~10% openness
    }
  }
  
  return eyeState.targetOpenness;
}

// ============================================================================
// SUBTLE EYE DRIFT (Realistic micro-movements during idle)
// ============================================================================

function processEyeDrift(currentTime) {
  if (!eyePersonality.driftEnabled) return;
  
  // Only drift during idle behaviors (not during active animations)
  const currentBehavior = behaviorState.currentBehavior;
  if (!DRIFT_BEHAVIORS.includes(currentBehavior)) return;
  
  // Only drift during hold phase
  if (behaviorState.phase !== 'hold') return;
  
  // Check if it's time for a drift
  if (currentTime - eyePersonality.lastDriftTime < eyePersonality.driftInterval) return;
  
  // Random chance to drift based on restlessness
  if (Math.random() > eyePersonality.restlessness) {
    eyePersonality.lastDriftTime = currentTime;
    return;
  }
  
  // Generate truly random offsets in all directions
  // Random angle from 0 to 360 degrees
  const angle = Math.random() * Math.PI * 2;
  const distance = 0.4 + Math.random() * 0.3; // 40-70% of max range (larger, more noticeable movements)
  
  // Sometimes look at center
  if (Math.random() < 0.2) {
    eyeState.targetOffsetX = 0;
    eyeState.targetOffsetY = 0;
  } else {
    // Use polar coordinates for even distribution
    eyeState.targetOffsetX = Math.cos(angle) * distance * EYE_MAX_X;
    eyeState.targetOffsetY = Math.sin(angle) * distance * EYE_MAX_Y;
  }
  
  // Slow down eye movement for realistic drift (natural eye movement is slower than reactions)
  eyeState.smoothing.offsetX = 3;  // Faster horizontal drift
  eyeState.smoothing.offsetY = 3;  // Faster vertical drift
  
  eyePersonality.lastDriftTime = currentTime;
  
  // Variable next drift time
  eyePersonality.driftInterval = 4000 + Math.random() * 4000; // 4-8 seconds
}

// ============================================================================
// PRIORITY SYSTEM
// ============================================================================

const PRIORITY = {
  IDLE: 0,
  MICRO: 1,
  BEHAVIOR: 2,
  REACTION: 3,
};

// ============================================================================
// BEHAVIOR DEFINITIONS - REFACTORED v2 (Hardware-Ready)
// ============================================================================
//
// REFACTORING SUMMARY:
// - Reduced from 50+ to 22 core behavioral states (55% reduction)
// - Removed all activity-based emotions (singing, humming, eating, etc.)
// - Removed redundant semantic-only idles (playful_idle, daydreaming, etc.)
// - Consolidated overlapping reactions (reacting_* merged, excited removed)
// - Focus: Pure eye movement primitives (openness, offsetX/Y, scaleX)
// - Target: Hardware-ready robotics development tool
//
// FINAL STATE BREAKDOWN:
// 🟢 Base / Idle (5):        calm_idle, sleepy_idle, curious_idle, shy_idle, scanning_idle
// 🟡 Cognitive / Interaction (8): listening_focused, listening_confused, thinking, processing,
//                                 understanding, misunderstanding, refusing_gently, startled
// 🟣 Emotional Reactions (6):     happy, shy_happy, embarrassed, soft_sad, disappointed, playful_mischief
// ⚙️ System / Sleep (3):          sleeping_deep, waking_up_slow, resting_eyes
// 🧪 Micro (2):                   double_blink, half_blink_hold
//
// Activities (postponed): Will be re-introduced via sprite animations in next phase.
//

/**
 * Behavior structure (RoboEyes-compatible):
 * {
 *   name: 'id',
 *   entryPhase: { openness, offsetX, offsetY, scaleX, duration, easing },
 *   holdPhase: { duration },
 *   exitPhase: { openness, offsetX, offsetY, scaleX, duration, easing },
 *   priority: PRIORITY level,
 *   gaze: GAZE position (optional, discrete eye position)
 *   blush: boolean (optional, for shy/embarrassed states)
 * 
 * COORDINATE SYSTEM:
 * - offsetX/offsetY are ABSOLUTE PIXELS, not normalized
 * - Use GAZE.LOOK_* constants for discrete positions
 * - Both eyes move together (no independent offsets)
 * }
 */

const BEHAVIORS = {};

// ---- BASE / IDLE (5) ----

BEHAVIORS.calm_idle = {
  name: 'calm_idle',
  entryPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 200, easing: 'easeOutSine' },
  holdPhase: { duration: 10000 },  // Longer hold for more drift time
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 200, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,  // Explicit center to reset from other behaviors
  priority: PRIORITY.IDLE,
};

BEHAVIORS.sleepy_idle = {
  name: 'sleepy_idle',
  entryPhase: { openness: 0.5, scaleX: 1.0, topLid: 0.2, bottomLid: 0.3, duration: 1000, easing: 'easeInOutSine' },
  holdPhase: { duration: 2000 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 1000, easing: 'easeOutSine' },
  priority: PRIORITY.IDLE,
  gaze: GAZE.LOOK_DOWN,
  effect: 'zzz',
};

BEHAVIORS.curious_idle = {
  name: 'curious_idle',
  entryPhase: { openness: 1.15, scaleX: 1.06, topLid: 0.6, bottomLid: 0.5, duration: 200, easing: 'easeOutSine' },
  holdPhase: { duration: 2000 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 200, easing: 'easeOutSine' },
  priority: PRIORITY.IDLE,
  // No gaze - allows free drift
};

// Removed: playful_idle, daydreaming, zoning_out, relaxed_blinking, slow_scanning, quiet_wait, patient_idle
// These were semantically named but visually redundant with core idle states.

BEHAVIORS.shy_idle = {
  name: 'shy_idle',
  entryPhase: { openness: 0.75, scaleX: 0.98, duration: 180, easing: 'easeInOutSine' },
  holdPhase: { duration: 2000 },
  exitPhase: { openness: 1.0, scaleX: 1.0, duration: 180, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_BOTTOM_LEFT,
  effect: 'blush',
  priority: PRIORITY.IDLE,
};

BEHAVIORS.scanning_idle = {
  name: 'scanning_idle',
  entryPhase: { openness: 1.02, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 150, easing: 'easeInOutSine' },
  holdPhase: { duration: 8000 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 600, easing: 'easeInOutSine' },
  gaze: GAZE.LOOK_CENTER,  // Fixed center position - no drift
  effect: 'scan',
  priority: PRIORITY.IDLE,
};

// Removed: confident_idle, awkward_idle, lonely_idle (semantically named but redundant)

// ---- COGNITIVE / INTERACTION (8) ----

BEHAVIORS.listening_focused = {
  name: 'listening_focused',
  entryPhase: { openness: 1.1, scaleX: 1.08, duration: 150, easing: 'easeOutSine' },
  holdPhase: { duration: 1500 },
  exitPhase: { openness: 1.0, scaleX: 1.0, duration: 180, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_UP,
  priority: PRIORITY.BEHAVIOR,
};

BEHAVIORS.listening_confused = {
  name: 'listening_confused',
  entryPhase: { openness: 1.05, scaleX: 1.02, duration: 180, easing: 'easeInOutSine' },
  holdPhase: { duration: 1500 },
  exitPhase: { openness: 1.0, scaleX: 1.0, duration: 180, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,  // Slight right-up offset but near center
  effect: 'confused',
  priority: PRIORITY.BEHAVIOR,
};



BEHAVIORS.thinking = {
  name: 'thinking',
  entryPhase: { openness: 0.8, scaleX: 0.98, duration: 180, easing: 'easeInOutSine' },
  holdPhase: { duration: 2000 },
  exitPhase: { openness: 1.0, scaleX: 1.0, duration: 180, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_TOP_LEFT,
  effect: 'thinking',
  priority: PRIORITY.BEHAVIOR,
};

BEHAVIORS.processing = {
  name: 'processing',
  entryPhase: { openness: 0.95, scaleX: 1.0, duration: 150, easing: 'easeOutSine' },
  holdPhase: { duration: 1200 },
  exitPhase: { openness: 1.0, scaleX: 1.0, duration: 150, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,
  effect: 'processing',
  priority: PRIORITY.BEHAVIOR,
};

BEHAVIORS.understanding = {
  name: 'understanding',
  entryPhase: { openness: 1.08, scaleX: 1.05, duration: 150, easing: 'easeOutSine' },
  holdPhase: { duration: 1200 },
  exitPhase: { openness: 1.0, scaleX: 1.0, duration: 180, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,  // Slight up-left but near center
  effect: 'understanding',
  priority: PRIORITY.BEHAVIOR,
};

BEHAVIORS.misunderstanding = {
  name: 'misunderstanding',
  entryPhase: { openness: 0.9, scaleX: 0.99, duration: 180, easing: 'easeInOutSine' },
  holdPhase: { duration: 1500 },
  exitPhase: { openness: 1.0, scaleX: 1.0, duration: 180, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,  // Confused right-up
  effect: 'misunderstanding',
  priority: PRIORITY.BEHAVIOR,
};

// Removed: reacting_softly, reacting_fast, reacting_slow (semantically distinct but visually similar)

BEHAVIORS.startled = {
  name: 'startled',
  entryPhase: { openness: 1.25, scaleX: 1.15, topLid: 0.9, bottomLid: 0.5, duration: 100, easing: 'easeOutSine' },
  holdPhase: { duration: 500 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 200, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_UP,
  effect: 'shock',
  priority: PRIORITY.REACTION,
};



BEHAVIORS.refusing_gently = {
  name: 'refusing_gently',
  entryPhase: { openness: 0.8, scaleX: 0.98, topLid: 0.4, bottomLid: 0.4, duration: 400, easing: 'easeInOutSine' },
  holdPhase: { duration: 600 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 400, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,  // Gentle left-down
  effect: 'refusing',
  priority: PRIORITY.BEHAVIOR,
};

// ---- ACTIVITIES (POSTPONED - Will be implemented via sprite animations)
// Removed: singing, humming, eating, chewing, sipping, stretching, self_entertainment
// These will be re-introduced in a future phase with proper sprite/animation support.

// ---- SYSTEM & SLEEP STATES ----

BEHAVIORS.sleeping_deep = {
  name: 'sleeping_deep',
  entryPhase: { openness: 0.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 1500, easing: 'easeInOutSine' },
  holdPhase: { duration: 3000 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 1500, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,
  priority: PRIORITY.BEHAVIOR,
};

BEHAVIORS.waking_up_slow = {
  name: 'waking_up_slow',
  entryPhase: { openness: 0.0, scaleX: 1.0, topLid: 0.3, bottomLid: 0.4, duration: 100, easing: 'easeOutSine' },
  holdPhase: { duration: 300 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 2000, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,
  priority: PRIORITY.BEHAVIOR,
};

BEHAVIORS.resting_eyes = {
  name: 'resting_eyes',
  entryPhase: { openness: 0.4, scaleX: 1.0, topLid: 0.3, bottomLid: 0.4, duration: 1000, easing: 'easeInOutSine' },
  holdPhase: { duration: 2500 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 1000, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,
  priority: PRIORITY.IDLE,
};

// ---- EMOTIONAL REACTIONS (6) ----

BEHAVIORS.happy = {
  name: 'happy',
  entryPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.6, bottomLid: 0.8, duration: 800, easing: 'easeInOutSine' },
  holdPhase: { duration: 1500 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 800, easing: 'easeInOutSine' },
  gaze: GAZE.LOOK_CENTER,  // Smile curve at bottom creates happy expression
  effect: 'glow',
  priority: PRIORITY.REACTION,
};

BEHAVIORS.shy_happy = {
  name: 'shy_happy',
  entryPhase: { openness: 0.95, scaleX: 0.99, topLid: 0.5, bottomLid: 0.7, duration: 400, easing: 'easeInOutSine' },
  holdPhase: { duration: 1500 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 500, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_TOP_RIGHT,  // Shy right-up glance
  effect: 'hearts',
  priority: PRIORITY.REACTION,
};

BEHAVIORS.soft_sad = {
  name: 'soft_sad',
  entryPhase: { openness: 0.55, scaleX: 0.88, topLid: 0.3, bottomLid: 0.2, duration: 800, easing: 'easeInOutQuad' },
  holdPhase: { duration: 2500 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 1000, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_DOWN,  // Droopy, sad lids looking down
  priority: PRIORITY.REACTION,
};

BEHAVIORS.disappointed = {
  name: 'disappointed',
  entryPhase: { openness: 0.82, scaleX: 0.75, topLid: 0.25, bottomLid: 0.3, duration: 400, easing: 'easeInOutSine' },
  holdPhase: { duration: 1800 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 600, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_LEFT,  // Flat, unimpressed lids looking away
  effect: 'disappointed_drop',
  priority: PRIORITY.REACTION,
};

BEHAVIORS.embarrassed = {
  name: 'embarrassed',
  entryPhase: { openness: 0.6, scaleX: 0.97, topLid: 0.3, bottomLid: 0.3, duration: 400, easing: 'easeInOutSine' },
  holdPhase: { duration: 1500 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 600, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_BOTTOM_RIGHT,  // Embarrassed right-down
  effect: 'embarrassed',
  priority: PRIORITY.REACTION,
};

// Removed: proud, teasing, affectionate (semantic overlap with playful_mischief / happy states)

BEHAVIORS.playful_mischief = {
  name: 'playful_mischief',
  entryPhase: { openness: 1.1, scaleX: 1.04, topLid: 0.6, bottomLid: 0.7, duration: 300, easing: 'easeOutSine' },
  holdPhase: { duration: 1200 },
  exitPhase: { openness: 1.0, scaleX: 1.0, topLid: 0.5, bottomLid: 0.5, duration: 400, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_TOP_RIGHT,  // Playful right-up (sly look)
  effect: 'sparkles',
  priority: PRIORITY.REACTION,
};

// ---- MICRO BEHAVIORS (Special) ----

BEHAVIORS.double_blink = {
  name: 'double_blink',
  entryPhase: { openness: 0.1, scaleX: 1.0, duration: 150, easing: 'easeOutSine' },
  holdPhase: { duration: 100 },
  exitPhase: { openness: 1.0, scaleX: 1.0, duration: 150, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,
  priority: PRIORITY.MICRO,
};

BEHAVIORS.half_blink_hold = {
  name: 'half_blink_hold',
  entryPhase: { openness: 0.5, scaleX: 1.0, duration: 200, easing: 'easeOutSine' },
  holdPhase: { duration: 500 },
  exitPhase: { openness: 1.0, scaleX: 1.0, duration: 200, easing: 'easeOutSine' },
  gaze: GAZE.LOOK_CENTER,
  priority: PRIORITY.MICRO,
};

// ============================================================================
// EASING FUNCTIONS (Pure math, portable to C++)
// ============================================================================

const easingFunctions = {
  linear: (t) => t,

  easeInOutSine: (t) => -Math.cos(Math.PI * t) / 2 + 0.5,
  easeOutSine: (t) => Math.sin((t * Math.PI) / 2),
  easeInSine: (t) => -Math.cos(t * Math.PI) + 1,

  easeInOutQuad: (t) => (t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t),
  easeOutQuad: (t) => 1 - (1 - t) * (1 - t),
  easeInQuad: (t) => t * t,

  easeInOutCubic: (t) => (t < 0.5 ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1),
  easeOutCubic: (t) => 1 - (1 - t) ** 3,
  easeInCubic: (t) => t ** 3,
};

function getEasing(name) {
  return easingFunctions[name] || easingFunctions.easeOutSine;
}

function clamp(val, min, max) {
  return Math.min(max, Math.max(min, val));
}

function lerp(a, b, t) {
  return a + (b - a) * t;
}

// ============================================================================
// MANDATORY: UNIFIED ANIMATION LOOP (Single RAF)
// ============================================================================

/**
 * Core animation loop - called 60 FPS
 * DETERMINISTIC ORDER:
 * 1. Calculate dt (delta time)
 * 2. Resolve active targets (with rhythmic modulation)
 * 3. Interpolate current → target
 * 4. Render to DOM (only)
 * 5. Update debug display
 */
function animationLoop(timestamp) {
  const dt = Math.min((timestamp - eyeState.lastFrameTime) / 1000, 0.05);
  eyeState.lastFrameTime = timestamp;

  // Process subtle eye drift for idle states
  processEyeDrift(timestamp);

  // Resolve current targets (including rhythmic if active)
  const targets = resolveActiveTargets(timestamp);

  // Interpolate toward targets
  interpolateToTargets(targets, dt);

  // Render to DOM (ONLY place that touches DOM)
  renderEyes();

  // Debug info
  updateDebugDisplay();

  // Schedule next frame
  animationLoopId = requestAnimationFrame(animationLoop);
}

/**
 * Compute active targets with rhythmic modulation and blinking
 */
function resolveActiveTargets(timestamp) {
  let targets = {
    openness: eyeState.targetOpenness,
    offsetX: eyeState.targetOffsetX,
    offsetY: eyeState.targetOffsetY,
    scaleX: eyeState.targetScaleX,
    topLid: eyeState.targetTopLid,
    bottomLid: eyeState.targetBottomLid,
  };

  // Apply rhythmic modulation (singing, chewing, etc.)
  if (eyeState.rhythmic.active) {
    const elapsed = (timestamp - eyeState.rhythmic.startTime) / 1000;
    const phase = elapsed * eyeState.rhythmic.frequency * Math.PI * 2;
    const modulation = Math.sin(phase) * eyeState.rhythmic.amplitude;
    targets.openness = clamp(eyeState.rhythmic.baseValue + modulation, 0, 1.25);
  }

  // Apply global blinking (orthogonal to expressions)
  targets.openness = processBlink(timestamp);

  return targets;
}

/**
 * Delta-time based exponential smoothing for frame-rate independence
 * Uses: value += (target - value) * (1 - exp(-smoothing * dt))
 * This provides smooth, physically-based animation regardless of frame rate
 */
function interpolateToTargets(targets, dt) {
  ['openness', 'offsetX', 'offsetY', 'scaleX', 'topLid', 'bottomLid'].forEach((prop) => {
    const current = eyeState[prop];
    const target = targets[prop];

    if (Math.abs(current - target) < 0.0001) {
      eyeState[prop] = target;
      return;
    }

    // Exponential smoothing: frame-rate independent
    const smoothing = eyeState.smoothing[prop] || 10;
    const factor = 1 - Math.exp(-smoothing * dt);
    eyeState[prop] = current + (target - current) * factor;
  });
}

function ensureAnimationLoopRunning() {
  if (animationLoopId === null) {
    eyeState.lastFrameTime = performance.now();
    animationLoopId = requestAnimationFrame(animationLoop);
  }
}

// ============================================================================
// RENDER (ONLY place that touches DOM)
// ============================================================================

/**
 * Render eyes and props to DOM
 * ONLY place that modifies the DOM
 * 
 * RoboEyes-style rendering: offsetX/offsetY are already in absolute pixels
 */
function renderEyes() {
  const leftEye = document.getElementById('eyeLeft');
  const rightEye = document.getElementById('eyeRight');

  if (!leftEye || !rightEye) return;

  // Eye rendering using ABSOLUTE pixel coordinates (RoboEyes model)
  // offsetX/offsetY are already in pixels, no multiplication needed
  const offsetXpx = eyeState.offsetX;
  const offsetYpx = eyeState.offsetY;

  // Keep rectangle shape, use scaleY for openness and border-radius for lid curves
  // Each animation gets unique shape through combination of scale and corner curves
  const transform = `translate(${offsetXpx}px, ${offsetYpx}px) scaleX(${eyeState.scaleX}) scaleY(${eyeState.openness})`;

  leftEye.style.transform = transform;
  rightEye.style.transform = transform;
  
  // Only apply clip-path curve when bottomLid meaningfully increases (> 0.52)
  // This keeps the default rectangle clean while enabling smooth smile transitions
  
  // Check if we're in happy behavior and should apply smile curve
  const isHappyBehavior = behaviorState.currentBehavior === 'happy';
  
  if (isHappyBehavior && eyeState.bottomLid > 0.5) {
    // Happy animation ONLY: smooth smile curve
    // Strategy: Always use clip-path during happy (no mode switching = no jumps)
    // When bottomLid = 0.5: clip-path creates normal rectangle (visually identical to default)
    // When bottomLid increases to 0.8: smoothly morphs into inward smile curve
    
    const curveAmount = (eyeState.bottomLid - 0.5) / 0.3; // 0.0 at 0.5, 1.0 at 0.8
    const maxCurveDepth = 20; // Maximum inward curve depth in pixels
    const curveDepth = curveAmount * maxCurveDepth;
    
    // Bottom edge position (moves up as curve increases)
    const bottomY = 90 - curveDepth;
    
    // Control point for quadratic bezier (creates inward smile)
    // When curveDepth = 0: controlY = 90 (straight line)
    // When curveDepth = 20: controlY = 70 (20px above = inward curve)
    const controlY = bottomY - curveDepth;
    
    const clipPath = `path('M 0,10 Q 0,0 10,0 L 90,0 Q 100,0 100,10 L 100,${bottomY} Q 50,${controlY} 0,${bottomY} Z')`;
    
    leftEye.style.clipPath = clipPath;
    rightEye.style.clipPath = clipPath;
    leftEye.style.borderRadius = '15px';
    rightEye.style.borderRadius = '15px';
  } else {
    // Default: normal rounded rectangle
    leftEye.style.clipPath = 'none';
    rightEye.style.clipPath = 'none';
    leftEye.style.borderRadius = '15px';
    rightEye.style.borderRadius = '15px';
  }
}

function updateDebugDisplay() {
  const stateDisplay = document.getElementById('state-display');
  const opennessDisplay = document.getElementById('openness-display');
  const behaviorInfo = document.getElementById('behavior-info');

  if (stateDisplay) {
    stateDisplay.textContent = `State: ${behaviorState.currentBehavior}`;
  }

  if (opennessDisplay) {
    const percentage = Math.round(eyeState.openness * 100);
    opennessDisplay.textContent = `Openness: ${percentage}%`;
  }

  if (behaviorInfo) {
    let info = `Phase: ${behaviorState.phase}`;
    if (eyeState.offsetX !== 0 || eyeState.offsetY !== 0) {
      info += ` | Gaze: (${Math.round(eyeState.offsetX)}, ${Math.round(eyeState.offsetY)})`;
    }
    behaviorInfo.textContent = info;
  }
}

// ============================================================================
// BEHAVIOR EXECUTION
// ============================================================================

function executeBehavior(behaviorName, force = false) {
  const behavior = BEHAVIORS[behaviorName];
  if (!behavior) {
    console.warn(`[Behavior] Not found: ${behaviorName}`);
    return;
  }

  if (behavior.priority < behaviorState.priority && !force) {
    return;
  }
  
  // Clear any existing effects from previous behavior
  clearEffect();

  behaviorState.currentBehavior = behaviorName;
  behaviorState.priority = behavior.priority;
  behaviorState.phase = 'entry';
  const token = ++currentBehaviorToken;

  // Update blinking and eye movement personality based on current behavior
  updateBlinkPersonality(behaviorName);

  ensureAnimationLoopRunning();

  // Execute entry phase
  startPhase(behavior, 'entry', token, () => {
    if (currentBehaviorToken !== token) return;

    // Proceed to hold or exit
    if (behavior.holdPhase.duration > 0) {
      behaviorState.phase = 'hold';
      setTimeout(() => {
        if (currentBehaviorToken !== token) return;
        startPhase(behavior, 'exit', token, () => {
          if (currentBehaviorToken !== token) return;
          executeBehavior('calm_idle', false);
        });
      }, behavior.holdPhase.duration);
    } else {
      startPhase(behavior, 'exit', token, () => {
        if (currentBehaviorToken !== token) return;
        executeBehavior('calm_idle', false);
      });
    }
  });
}

/**
 * Execute micro behavior (alias for executeBehavior with force=true)
 * Used for special micro behaviors like double_blink, half_blink_hold
 */
function executeMicroBehavior(behaviorName) {
  if (behaviorName === 'double_blink') {
    performDoubleBlink();
  } else {
    executeBehavior(behaviorName, true);
  }
}

/**
 * Perform double blink - two complete blink cycles without auto-reset
 */
/**
 * Perform double blink - two complete blink cycles
 * Works with delta-time exponential smoothing (openness smoothing = 12)
 */
function performDoubleBlink() {
  ensureAnimationLoopRunning();
  
  // For exponential smoothing with factor 12:
  // ~63% completion at 83ms, ~95% completion at 250ms
  // We use adjusted timing to account for smooth interpolation
  
  // First blink: close eyes
  eyeState.targetOpenness = 0.1;
  
  // Open after 300ms (allows closing + hold time)
  setTimeout(() => {
    eyeState.targetOpenness = 1.0;
    
    // Small gap before second blink (250ms)
    setTimeout(() => {
      // Second blink: close eyes again
      eyeState.targetOpenness = 0.1;
      
      // Final open (300ms)
      setTimeout(() => {
        eyeState.targetOpenness = 1.0;
      }, 300);
    }, 250);
  }, 300);
}

/**
 * Reset to calm idle state
 */
function resetToCalmIdle() {
  executeBehavior('calm_idle', true);
}

/**
 * Execute a single phase of a behavior
 */
function startPhase(behavior, phase, token, onComplete) {
  const phaseData = behavior[`${phase}Phase`];

  // Disable rhythmic if transitioning
  eyeState.rhythmic.active = false;

  // Set targets - exponential smoothing will interpolate to these
  eyeState.targetOpenness = phaseData.openness;
  eyeState.targetScaleX = phaseData.scaleX;
  eyeState.targetTopLid = phaseData.topLid !== undefined ? phaseData.topLid : 0.5;
  eyeState.targetBottomLid = phaseData.bottomLid !== undefined ? phaseData.bottomLid : 0.5;

  // Duration is used only for timing the phase length, not for animation speed
  // (animation speed is determined by smoothing factors in eyeState.smoothing)

  // Handle discrete gaze position - both eyes move together (RoboEyes style)
  if (phase === 'entry' && behavior.gaze) {
    // Apply gaze position during entry
    eyeState.targetOffsetX = behavior.gaze.x;
    eyeState.targetOffsetY = behavior.gaze.y;
    // Reset to normal speed for discrete gaze positions (fast reactions)
    eyeState.smoothing.offsetX = 12;
    eyeState.smoothing.offsetY = 12;
  } else if (phase === 'exit' && behavior.gaze) {
    // Return to center on exit ONLY if behavior had a gaze position
    eyeState.targetOffsetX = 0;
    eyeState.targetOffsetY = 0;
    // Reset to normal speed
    eyeState.smoothing.offsetX = 12;
    eyeState.smoothing.offsetY = 12;
  }
  // Note: During 'hold' phase, targets can be modified by drift for idle behaviors

  // Handle visual effects
  if (behavior.effect && phase === 'entry') {
    createEffect(behavior.effect);
  } else if (phase === 'exit') {
    clearEffect();
  }

  // Activate prop if specified
  if (behavior.prop && phase === 'entry') {
    const propType = behavior.prop.type;
    if (propType === 'mic') propState.mic = 1.0;
    else if (propType === 'bowl') propState.bowl = 1.0;
    else if (propType === 'thread') propState.thread = 1.0;
  } else if (phase === 'exit' && behavior.prop) {
    // Fade out prop during exit
    const propType = behavior.prop.type;
    if (propType === 'mic') propState.mic = 0;
    else if (propType === 'bowl') propState.bowl = 0;
    else if (propType === 'thread') propState.thread = 0;
  }

  // Start rhythmic after entry completes (if applicable)
  if (behavior.rhythmic && phase !== 'exit') {
    setTimeout(() => {
      if (currentBehaviorToken !== token) return;
      eyeState.rhythmic.active = true;
      eyeState.rhythmic.baseValue = phaseData.openness;
      eyeState.rhythmic.amplitude = behavior.rhythmic.amplitude;
      eyeState.rhythmic.frequency = behavior.rhythmic.frequency;
      eyeState.rhythmic.startTime = performance.now();
    }, phaseData.duration);
  }

  // Schedule completion
  if (phaseData.duration > 0) {
    setTimeout(() => {
      if (currentBehaviorToken !== token) return;
      if (onComplete) onComplete();
    }, phaseData.duration);
  } else {
    if (onComplete) onComplete();
  }
}

// ============================================================================
// EYE MOVEMENT (Look left/right/center/down)
// ============================================================================

/**
 * Look left - sets target to discrete left edge position
 */
function lookLeft() {
  eyeState.targetOffsetX = GAZE.LOOK_LEFT.x;
  eyeState.targetOffsetY = GAZE.LOOK_LEFT.y;
}

/**
 * Look right - sets target to discrete right edge position
 */
function lookRight() {
  eyeState.targetOffsetX = GAZE.LOOK_RIGHT.x;
  eyeState.targetOffsetY = GAZE.LOOK_RIGHT.y;
}

/**
 * Look center - return to neutral gaze
 */
function lookCenter() {
  eyeState.targetOffsetX = GAZE.LOOK_CENTER.x;
  eyeState.targetOffsetY = GAZE.LOOK_CENTER.y;
}

/**
 * Look down - for focused/sad states
 */
function lookDown() {
  eyeState.targetOffsetX = GAZE.LOOK_DOWN.x;
  eyeState.targetOffsetY = GAZE.LOOK_DOWN.y;
}

/**
 * Look up - for surprised/startled states
 */
function lookUp() {
  eyeState.targetOffsetX = GAZE.LOOK_UP.x;
  eyeState.targetOffsetY = GAZE.LOOK_UP.y;
}

// ============================================================================
// UI CONTROLS
// ============================================================================

function initializeControls() {
  // Expression buttons
  document.querySelectorAll('button[data-expression]').forEach((button) => {
    button.addEventListener('click', () => {
      executeBehavior(button.dataset.expression, true);
    });
  });

  // Randomness toggle
  const toggleBtn = document.getElementById('toggle-randomness');
  if (toggleBtn) {
    toggleBtn.addEventListener('click', () => {
      RANDOMNESS_ENABLED = !RANDOMNESS_ENABLED;
      toggleBtn.textContent = RANDOMNESS_ENABLED ? 'Randomness: ON' : 'Randomness: OFF';
      toggleBtn.style.background = RANDOMNESS_ENABLED ? '#0066cc' : '#cc6600';
    });
  }

  // Keyboard shortcuts
  document.addEventListener('keydown', (e) => {
    const key = e.key.toLowerCase();

    if (key === 'r') executeBehavior('calm_idle', true);
    else if (key === 't') RANDOMNESS_ENABLED = !RANDOMNESS_ENABLED;
    else if (key === 'm') triggerRandomMicroBehavior();
    else if (key === 's') executeBehavior('singing', true);
    else if (key === 'h') executeBehavior('humming', true);
    else if (key >= '1' && key <= '9') {
      const names = Object.keys(BEHAVIORS);
      const idx = parseInt(key) - 1;
      if (names[idx]) executeBehavior(names[idx], true);
    } else if (key === '0') {
      const names = Object.keys(BEHAVIORS);
      if (names[9]) executeBehavior(names[9], true);
    }
  });

  console.log('[Init] Controls ready');
}

// ============================================================================
// MICRO-BEHAVIORS & SCHEDULING
// ============================================================================

function triggerRandomMicroBehavior() {
  const microBehaviors = ['curious_idle', 'playful_idle', 'slow_scanning'];
  const choice = microBehaviors[Math.floor(Math.random() * microBehaviors.length)];
  executeBehavior(choice, false);
}

function scheduleRandomBehaviors() {
  if (randomnessSchedulerId) clearTimeout(randomnessSchedulerId);

  if (!RANDOMNESS_ENABLED) return;

  const delayMs = 5000 + Math.random() * 10000;

  randomnessSchedulerId = setTimeout(() => {
    const idleBehaviors = [
      { name: 'calm_idle', weight: 0.4 },
      { name: 'curious_idle', weight: 0.2 },
      { name: 'bored_idle', weight: 0.15 },
      { name: 'playful_idle', weight: 0.1 },
      { name: 'slow_scanning', weight: 0.15 },
    ];

    let totalWeight = idleBehaviors.reduce((sum, b) => sum + b.weight, 0);
    let rand = Math.random() * totalWeight;

    for (const behavior of idleBehaviors) {
      rand -= behavior.weight;
      if (rand <= 0) {
        executeBehavior(behavior.name, false);
        break;
      }
    }

    scheduleRandomBehaviors();
  }, delayMs);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

function init() {
  console.log('╔═══════════════════════════════════════════════════════╗');
  console.log('║   UNIFIED EYE ANIMATION ENGINE - DETERMINISTIC        ║');
  console.log('║   Single RAF Loop | Motion-based | ESP32 Portable     ║');
  console.log('╚═══════════════════════════════════════════════════════╝');
  console.log(`[Init] Behaviors: ${Object.keys(BEHAVIORS).length}`);
  console.log('[Init] Architecture: Single RAF + Unified State + Pure Math');

  renderEyes();
  updateDebugDisplay();
  initializeControls();

  ensureAnimationLoopRunning();

  // Initialize blinking system
  scheduleNextBlink(performance.now());

  setTimeout(() => {
    executeBehavior('calm_idle', true);
  }, 100);

  scheduleRandomBehaviors();

  console.log('[Init] Ready | R=reset T=toggle M=micro S=sing H=hum');
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}

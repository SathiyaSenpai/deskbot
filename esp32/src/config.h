#ifndef CONFIG_H
#define CONFIG_H

// DEVICE SETTINGS
#define DEVICE_NAME "nisya-companion"
#define FW_VERSION "1.4.0"

// ============================================================================
// WIFI CONFIGURATION (STATION MODE)
// ============================================================================
#define WIFI_SSID     "OnePlus Nord 4"
#define WIFI_PASSWORD "123456789"

// ============================================================================
// WIFI AP MODE (SETUP FALLBACK)
// ============================================================================
#define WIFI_MANAGER_AP_NAME "Nisya-Setup"
#define WIFI_MANAGER_AP_PASS "12345678"
#define WIFI_MANAGER_TIMEOUT 180  // seconds

// ============================================================================
// SERVER CONFIGURATION
// ============================================================================
#define WS_HOST "10.121.79.219"   
#define WS_PORT 3000             
#define WS_PATH "/ws?type=robot"

// HARDWARE SETTINGS
#define WS_RECONNECT_INTERVAL 3000   
#define SENSOR_READ_INTERVAL  100    
#define AUDIO_ENABLED true           
#define AUDIO_VOLUME 18

// MEMORY OPTIMIZATION & MIC CONFIGURATION
#define ENABLE_MICROPHONE true       // Enabled with time-multiplexing and 16-bit DMA
// NOTE: getLoudness() returns 0-100 (RMS / 50). Threshold must be in this range.
#define MIC_VAD_THRESHOLD 45         // Voice trigger level (0-100 scale). 45 = medium sensitivity
#define MIC_RECORD_TIMEOUT_MS 8000   // Max voice recording duration (8 sec)
#define MIC_STREAM_CHUNK_MS 100      // Stream audio chunks every 100ms

#ifndef DEBUG_VERBOSE
#define DEBUG_VERBOSE false
#endif

#endif // CONFIG_H
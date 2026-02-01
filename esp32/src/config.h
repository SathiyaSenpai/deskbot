#ifndef CONFIG_H
#define CONFIG_H

// DEVICE SETTINGS
#define DEVICE_NAME "deskbot-sathiya"
#define FW_VERSION "1.3.0"

// ============================================================================
// WIFI CONFIGURATION (STATION MODE)
// ============================================================================
#define WIFI_SSID     "OnePlus Nord 4"
#define WIFI_PASSWORD "123456789"

// ============================================================================
// WIFI AP MODE (SETUP FALLBACK)
// ============================================================================
#define WIFI_MANAGER_AP_NAME "DeskBot-Setup"
#define WIFI_MANAGER_AP_PASS "12345678"
#define WIFI_MANAGER_TIMEOUT 180  // seconds

// ============================================================================
// SERVER CONFIGURATION
// ============================================================================
#define WS_HOST "10.35.234.34"   
#define WS_PORT 3000             
#define WS_PATH "/ws?type=robot"

// HARDWARE SETTINGS
#define WS_RECONNECT_INTERVAL 3000   
#define SENSOR_READ_INTERVAL  100    
#define AUDIO_ENABLED true           
#define AUDIO_VOLUME 18

// MEMORY OPTIMIZATION
#define ENABLE_MICROPHONE false  // Set to true once basic features work

#endif // CONFIG_H
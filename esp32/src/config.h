#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// WiFi Configuration
// ============================================================================
// Default credentials (used if WiFiManager config not set)
// These work with your phone's hotspot!
#define WIFI_SSID     "OnePlus Nord 4"
#define WIFI_PASSWORD "123456789"

// WiFiManager settings (for easy reconfiguration without reflashing!)
#define WIFI_MANAGER_AP_NAME "EMO-Setup"      // WiFi name ESP32 creates for setup
#define WIFI_MANAGER_AP_PASS "emo12345"       // Password for setup WiFi
#define WIFI_MANAGER_TIMEOUT 180              // 3 minutes to configure, then use defaults

// ============================================================================
// WebSocket Server Configuration  
// ============================================================================
// For HOME use: Your laptop IP (run: hostname -I)
// For PORTABLE use: Your phone IP (usually 192.168.x.1 when hotspot)
//
// TIP: When using phone hotspot, phone IP is typically:
//      192.168.43.1 (Android default) or check in hotspot settings

#define WS_HOST "192.168.43.1"   // Phone hotspot IP (Android default)
#define WS_PORT 3000             // Server runs on port 3000
#define WS_PATH "/ws?type=robot&token=emo_secret_2024"

// For home use, change to your laptop IP:
// #define WS_HOST "10.31.240.236"

// Auth token (must match server's AUTH_TOKEN)
#define AUTH_TOKEN "emo_secret_2024"

// ============================================================================
// Device Identity
// ============================================================================
#define DEVICE_NAME "emo-deskbot"
#define FW_VERSION "0.2.0"

// ============================================================================
// Timing Configuration
// ============================================================================
#define WS_RECONNECT_INTERVAL 5000   // Reconnect every 5 seconds if disconnected
#define SENSOR_READ_INTERVAL  100    // Read sensors every 100ms
#define STATE_BROADCAST_INTERVAL 1000 // Send state to server every 1 second

// ============================================================================
// Audio Configuration
// ============================================================================
#define AUDIO_ENABLED true           // Enable audio playback
#define AUDIO_VOLUME 80              // Default volume (0-100)

#endif // CONFIG_H

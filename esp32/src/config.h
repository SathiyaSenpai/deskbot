#ifndef CONFIG_H
#define CONFIG_H

// DEVICE SETTINGS
#define DEVICE_NAME "deskbot-sathiya"
#define FW_VERSION "1.3.0"

// WIFI CONFIGURATION (STATION MODE)

#define WIFI_SSID     "Your wifi name"
#define WIFI_PASSWORD "wifi password"

#define WIFI_MANAGER_AP_NAME "DeskBot-Setup"
#define WIFI_MANAGER_AP_PASS "12345678"
#define WIFI_MANAGER_TIMEOUT 180  // seconds

#define WS_HOST "10.238.191.24"   
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

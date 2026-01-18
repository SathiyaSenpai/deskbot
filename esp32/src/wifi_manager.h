/**
 * WiFi Manager for EMO Deskbot
 * 
 * Features:
 * - Auto-connects to saved WiFi
 * - Creates setup portal if no WiFi configured
 * - Allows reconfiguration via button hold
 * - Stores credentials in flash (survives reboot)
 * 
 * NO MORE REFLASHING FOR WIFI CHANGES!
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include "config.h"
#include "pins.h"

// ============================================================================
// WiFi Manager Class
// ============================================================================

class WiFiManager {
private:
    Preferences preferences;
    WebServer* server;
    DNSServer* dnsServer;
    
    String savedSSID;
    String savedPassword;
    String serverIP;
    int serverPort;
    
    bool portalRunning = false;
    unsigned long portalStartTime = 0;
    
    // HTML for configuration portal
    const char* portalHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>EMO Setup</title>
    <style>
        body { font-family: Arial; background: #1a1a2e; color: #fff; padding: 20px; }
        .container { max-width: 400px; margin: 0 auto; }
        h1 { color: #00d4ff; text-align: center; }
        .emoji { font-size: 60px; text-align: center; }
        input, select { width: 100%; padding: 12px; margin: 8px 0; border: none; border-radius: 8px; box-sizing: border-box; }
        input[type=submit] { background: #00d4ff; color: #000; font-weight: bold; cursor: pointer; }
        input[type=submit]:hover { background: #00a8cc; }
        label { display: block; margin-top: 15px; color: #888; }
        .networks { background: #2a2a4e; padding: 15px; border-radius: 8px; margin: 15px 0; }
        .network { padding: 10px; cursor: pointer; border-radius: 5px; }
        .network:hover { background: #3a3a6e; }
        .info { background: #2a4a2a; padding: 10px; border-radius: 8px; margin-top: 20px; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="emoji">🤖</div>
        <h1>EMO Setup</h1>
        
        <form action="/save" method="POST">
            <label>WiFi Network</label>
            <select name="ssid" id="ssid">
                <option value="">Scanning...</option>
            </select>
            <input type="text" name="ssid_manual" placeholder="Or enter SSID manually">
            
            <label>WiFi Password</label>
            <input type="password" name="password" placeholder="Enter WiFi password">
            
            <label>Server IP (your laptop/phone)</label>
            <input type="text" name="server_ip" placeholder="192.168.43.1" value="%SERVER_IP%">
            
            <label>Server Port</label>
            <input type="number" name="server_port" value="%SERVER_PORT%">
            
            <input type="submit" value="Save & Connect">
        </form>
        
        <div class="info">
            <strong>📱 Phone Hotspot?</strong><br>
            Server IP is usually: 192.168.43.1<br><br>
            <strong>💻 Home PC?</strong><br>
            Run <code>hostname -I</code> to find IP
        </div>
    </div>
    
    <script>
        fetch('/scan').then(r => r.json()).then(networks => {
            const select = document.getElementById('ssid');
            select.innerHTML = '<option value="">-- Select Network --</option>';
            networks.forEach(n => {
                select.innerHTML += `<option value="${n.ssid}">${n.ssid} (${n.rssi}dBm)</option>`;
            });
        });
    </script>
</body>
</html>
)rawliteral";

public:
    // Current connection state
    bool isConnected = false;
    String currentIP = "";
    
    /**
     * Initialize WiFi Manager
     */
    void begin() {
        preferences.begin("emo-wifi", false);
        
        // Load saved credentials
        savedSSID = preferences.getString("ssid", WIFI_SSID);
        savedPassword = preferences.getString("pass", WIFI_PASSWORD);
        serverIP = preferences.getString("server_ip", WS_HOST);
        serverPort = preferences.getInt("server_port", WS_PORT);
        
        Serial.println("[WiFi] Saved SSID: " + savedSSID);
        Serial.println("[WiFi] Server: " + serverIP + ":" + String(serverPort));
    }
    
    /**
     * Try to connect to saved WiFi
     * Returns true if connected, false if need configuration
     */
    bool autoConnect() {
        if (savedSSID.length() == 0) {
            Serial.println("[WiFi] No saved credentials, starting portal");
            return false;
        }
        
        Serial.println("[WiFi] Connecting to: " + savedSSID);
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        
        // Wait for connection (max 15 seconds)
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            isConnected = true;
            currentIP = WiFi.localIP().toString();
            Serial.println("\n[WiFi] Connected! IP: " + currentIP);
            return true;
        }
        
        Serial.println("\n[WiFi] Connection failed!");
        return false;
    }
    
    /**
     * Start configuration portal
     * ESP32 creates a WiFi network you can connect to
     */
    void startPortal() {
        Serial.println("[WiFi] Starting configuration portal...");
        
        // Create Access Point
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASS);
        
        IPAddress apIP = WiFi.softAPIP();
        Serial.println("[WiFi] Portal started!");
        Serial.println("[WiFi] Connect to WiFi: " + String(WIFI_MANAGER_AP_NAME));
        Serial.println("[WiFi] Password: " + String(WIFI_MANAGER_AP_PASS));
        Serial.println("[WiFi] Then open: http://" + apIP.toString());
        
        // Start DNS server (captive portal)
        dnsServer = new DNSServer();
        dnsServer->start(53, "*", apIP);
        
        // Start web server
        server = new WebServer(80);
        
        // Serve main page
        server->on("/", HTTP_GET, [this]() {
            String html = String(portalHTML);
            html.replace("%SERVER_IP%", serverIP);
            html.replace("%SERVER_PORT%", String(serverPort));
            server->send(200, "text/html", html);
        });
        
        // Scan for networks
        server->on("/scan", HTTP_GET, [this]() {
            String json = "[";
            int n = WiFi.scanNetworks();
            for (int i = 0; i < n; i++) {
                if (i > 0) json += ",";
                json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
            }
            json += "]";
            server->send(200, "application/json", json);
        });
        
        // Save configuration
        server->on("/save", HTTP_POST, [this]() {
            String ssid = server->arg("ssid");
            String ssidManual = server->arg("ssid_manual");
            String password = server->arg("password");
            String ip = server->arg("server_ip");
            int port = server->arg("server_port").toInt();
            
            // Use manual SSID if provided
            if (ssidManual.length() > 0) {
                ssid = ssidManual;
            }
            
            if (ssid.length() > 0) {
                // Save to flash
                preferences.putString("ssid", ssid);
                preferences.putString("pass", password);
                preferences.putString("server_ip", ip);
                preferences.putInt("server_port", port);
                
                savedSSID = ssid;
                savedPassword = password;
                serverIP = ip;
                serverPort = port;
                
                Serial.println("[WiFi] Saved new config!");
                Serial.println("[WiFi] SSID: " + ssid);
                Serial.println("[WiFi] Server: " + ip + ":" + String(port));
                
                server->send(200, "text/html", 
                    "<html><body style='background:#1a1a2e;color:#fff;text-align:center;padding:50px;font-family:Arial'>"
                    "<h1>✅ Saved!</h1>"
                    "<p>EMO will now restart and connect to:<br><strong>" + ssid + "</strong></p>"
                    "<p>Server: " + ip + ":" + String(port) + "</p>"
                    "</body></html>");
                
                // Restart after 2 seconds
                delay(2000);
                ESP.restart();
            } else {
                server->send(400, "text/html", "Please select a WiFi network");
            }
        });
        
        // Handle all other requests (captive portal)
        server->onNotFound([this]() {
            server->sendHeader("Location", "http://192.168.4.1/");
            server->send(302, "text/plain", "");
        });
        
        server->begin();
        portalRunning = true;
        portalStartTime = millis();
        
        Serial.println("[WiFi] Portal ready!");
    }
    
    /**
     * Handle portal requests (call in loop)
     */
    void handlePortal() {
        if (!portalRunning) return;
        
        dnsServer->processNextRequest();
        server->handleClient();
        
        // Timeout - use default credentials
        if (millis() - portalStartTime > WIFI_MANAGER_TIMEOUT * 1000) {
            Serial.println("[WiFi] Portal timeout, using defaults");
            stopPortal();
            autoConnect();
        }
    }
    
    /**
     * Stop the configuration portal
     */
    void stopPortal() {
        if (!portalRunning) return;
        
        server->stop();
        dnsServer->stop();
        delete server;
        delete dnsServer;
        portalRunning = false;
        
        Serial.println("[WiFi] Portal stopped");
    }
    
    /**
     * Reset saved credentials (hold button to trigger)
     */
    void resetCredentials() {
        Serial.println("[WiFi] Resetting credentials...");
        preferences.clear();
        Serial.println("[WiFi] Done! Restarting...");
        ESP.restart();
    }
    
    /**
     * Get server IP for WebSocket connection
     */
    String getServerIP() {
        return serverIP;
    }
    
    /**
     * Get server port for WebSocket connection
     */
    int getServerPort() {
        return serverPort;
    }
    
    /**
     * Check if portal is running
     */
    bool isPortalRunning() {
        return portalRunning;
    }
};

// Global instance
WiFiManager wifiManager;

#endif // WIFI_MANAGER_H

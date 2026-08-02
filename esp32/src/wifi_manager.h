#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <esp_task_wdt.h>
#include <ESPmDNS.h>
#include "config.h"

// HTML string stored in PROGMEM (Flash memory) to save DRAM
static const char HTML[] PROGMEM = R"(
<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Nisya Setup</title><style>body{font-family:Arial;background:#1a1a2e;color:#fff;padding:20px}
.container{max-width:400px;margin:0 auto}h1{color:#00d4ff;text-align:center}
input{width:100%;padding:12px;margin:8px 0;border:none;border-radius:8px;box-sizing:border-box}
input[type=submit]{background:#00d4ff;color:#000;font-weight:bold;cursor:pointer}
</style></head><body><div class='container'><h1>🤖 Nisya Setup</h1>
<form action='/save' method='POST'>
WiFi SSID:<input name='ssid' required><br>
Password:<input name='pass' type='password'><br>
Server IP:<input name='ip' value='deskbot.local'><br>
Server Port:<input name='port' type='number' value='3000'><br>
<input type='submit' value='Save & Restart'>
</form></div></body></html>
)";

class WiFiManager {
private:
    Preferences prefs;
    WebServer* server = nullptr;
    DNSServer* dnsServer = nullptr;
    
    String savedSSID;
    String savedPassword;
    String serverIP;
    int serverPort;
    bool portalRunning = false;
    unsigned long portalStartTime = 0;

public:
    void begin() {
        // Use consistent namespace
        if (!prefs.begin("nisya-wifi", false)) {
            Serial.println(F("[WiFi] NVS init failed, erasing..."));
            prefs.clear();
            prefs.end();
            prefs.begin("nisya-wifi", false);
        }
        
        savedSSID = prefs.getString("ssid", "");
        savedPassword = prefs.getString("pass", "");
        serverIP = prefs.getString("ip", WS_HOST);
        if (serverIP == "10.121.79.219" || serverIP == "10.118.223.125" || serverIP.length() == 0) {
            serverIP = WS_HOST;
            prefs.putString("ip", serverIP);
        }
        serverPort = prefs.getInt("port", WS_PORT);
        
        // Fallback to config.h if NVS empty
        if (savedSSID.length() == 0) {
            savedSSID = WIFI_SSID;
            savedPassword = WIFI_PASSWORD;
        }
        
        Serial.printf("[WiFi] Config: %s / Server: %s:%d\n", 
                     savedSSID.c_str(), serverIP.c_str(), serverPort);
    }

    bool autoConnect() {
        if (savedSSID.length() == 0) {
            Serial.println(F("[WiFi] No credentials"));
            return false;
        }
        
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        
        Serial.print(F("[WiFi] Connecting"));
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 35) {
            esp_task_wdt_reset(); // Feed WDT — prevents 5s WDT reset during slow WiFi handshake
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            MDNS.begin("nisya-companion");
            return true;
        }
        
        Serial.println(F("\n[WiFi] Connection failed"));
        return false;
    }

    void startPortal() {
        // Guard against memory leaks if portal already running
        stopPortal();

        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASS);
        
        IPAddress ip = WiFi.softAPIP();
        Serial.printf("[WiFi] Portal started at %s\n", ip.toString().c_str());
        
        dnsServer = new DNSServer();
        dnsServer->start(53, "*", ip);
        
        server = new WebServer(80);
        
        server->on("/", [this](){ server->send_P(200, "text/html", HTML); });
        
        server->on("/save", HTTP_POST, [this](){
            String ssid = server->arg("ssid");
            String pass = server->arg("pass");
            String ip = server->arg("ip");
            int port = server->arg("port").toInt();
            
            if (ssid.length() > 0) {
                prefs.putString("ssid", ssid);
                prefs.putString("pass", pass);
                prefs.putString("ip", ip);
                prefs.putInt("port", port);
                
                server->send(200, "text/html", 
                    "<html><body style='background:#1a1a2e;color:#fff;text-align:center;padding:50px'>"
                    "<h1>✅ Saved!</h1><p>Restarting...</p></body></html>");
                
                delay(2000);
                ESP.restart();
            } else {
                server->send(400, "text/plain", "Invalid input");
            }
        });
        
        server->begin();
        portalRunning = true;
        portalStartTime = millis();
    }

    void handlePortal() {
        if (!portalRunning) return;
        
        if (dnsServer) dnsServer->processNextRequest();
        if (server) server->handleClient();
        
        // Timeout after WIFI_MANAGER_TIMEOUT seconds
        if (millis() - portalStartTime > WIFI_MANAGER_TIMEOUT * 1000) {
            Serial.println(F("[WiFi] Portal timeout"));
            stopPortal();
        }
    }

    void stopPortal() {
        if (server) { 
            server->stop(); 
            delete server; 
            server = nullptr; 
        }
        if (dnsServer) { 
            dnsServer->stop(); 
            delete dnsServer; 
            dnsServer = nullptr; 
        }
        portalRunning = false;
    }

    bool isPortalRunning() { return portalRunning; }
    String getServerIP() { return serverIP; }
    int getServerPort() { return serverPort; }
};

#endif
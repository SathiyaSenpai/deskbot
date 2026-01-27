#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <Arduino.h>
#include <functional>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "config.h"

class RobotWebSocket {
public:
  using MessageCallback = std::function<void(const char*, JsonDocument&)>;

private:
  WebSocketsClient ws;
  bool connected = false;
  MessageCallback callback;
  String serverHost;
  int serverPort;

  void handleEvent(WStype_t type, uint8_t* payload, size_t len) {
    switch(type) {
      case WStype_DISCONNECTED:
        if (connected) Serial.println("[WS] Disconnected");
        connected = false;
        break;
        
      case WStype_CONNECTED:
        Serial.printf("[WS] Connected to %s\n", (char*)payload);
        connected = true;
        sendStatus("connect", "online");
        // Send current behavior state immediately after connection for sync
        // Note: This will be called from main.cpp after activeBehavior is set
        break;
        
      case WStype_TEXT:
        if (callback) {
          StaticJsonDocument<1024> doc;
          DeserializationError err = deserializeJson(doc, payload);
          if (!err) {
            const char* type = doc["type"];
            if (type) callback(type, doc);
          }
        }
        break;
        
      default:
        break;
    }
  }

public:
  void setServer(const char* host, int port) {
    serverHost = host;
    serverPort = port;
  }

  void begin() {
    const char* host = serverHost.length() > 0 ? serverHost.c_str() : WS_HOST;
    int port = serverPort > 0 ? serverPort : WS_PORT;

    Serial.printf("[WS] Connecting to %s:%d%s\n", host, port, WS_PATH);
    
    ws.begin(host, port, WS_PATH);
    ws.onEvent([this](WStype_t type, uint8_t* payload, size_t len) {
      handleEvent(type, payload, len);
    });
    // Increased reconnect interval to reduce connection loop issues
    ws.setReconnectInterval(10000);  // 10 seconds instead of 5
    // More lenient heartbeat: 20s interval, 5s timeout, 3 retries
    ws.enableHeartbeat(20000, 5000, 3);
  }

  void loop() { ws.loop(); }
  bool isConnected() { return connected; }
  void setCallback(MessageCallback cb) { callback = cb; }

  void sendStatus(const char* event, const char* detail) {
    if (!connected) return;
    
    StaticJsonDocument<256> doc;
    doc["type"] = "robot_status";
    doc["event"] = event;
    doc["detail"] = detail;
    
    String output;
    serializeJson(doc, output);
    ws.sendTXT(output);
  }

  void sendSensors(const SensorData& s) {
    if (!connected) return;
    
    StaticJsonDocument<384> doc;
    doc["type"] = "sensor_data";
    doc["light"] = s.light;
    doc["motion"] = s.motion;
    doc["distance_mm"] = s.distance_mm;
    doc["touch_head"] = s.touchHead;
    doc["touch_side"] = s.touchSide;
    doc["soundLevel"] = s.soundLevel;
    
    String output;
    serializeJson(doc, output);
    ws.sendTXT(output);
  }
};

#endif
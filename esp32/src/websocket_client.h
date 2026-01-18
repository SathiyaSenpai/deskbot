#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <Arduino.h>
#include <functional>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "config.h"

class RobotWebSocket {
public:
  using MessageCallback = std::function<void(const char* type, JsonDocument& doc)>;

  // Set server dynamically (from WiFiManager)
  void setServer(const char* host, int port) {
    serverHost_ = String(host);
    serverPort_ = port;
  }

  void begin() {
    // Use dynamic server if set, otherwise use defaults from config
    if (serverHost_.length() > 0) {
      ws_.begin(serverHost_.c_str(), serverPort_, WS_PATH);
    } else {
      ws_.begin(WS_HOST, WS_PORT, WS_PATH);
    }
    ws_.onEvent([this](WStype_t type, uint8_t* payload, size_t len){ handle(type, payload, len); });
    ws_.setReconnectInterval(5000);
    Serial.printf("[WS] Connecting to %s:%d\n", 
                  serverHost_.length() > 0 ? serverHost_.c_str() : WS_HOST, 
                  serverHost_.length() > 0 ? serverPort_ : WS_PORT);
  }

  void loop() { ws_.loop(); }
  bool connected() const { return connected_; }
  void setCallback(MessageCallback cb) { cb_ = cb; }

  // Send a JSON document
  void send(JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    ws_.sendTXT(out);
  }

  void sendStatus(const char* state, const char* behavior) {
    StaticJsonDocument<256> doc;
    doc["type"] = "robot_status";
    doc["device"] = DEVICE_NAME;
    doc["state"] = state;
    doc["behavior"] = behavior;
    doc["ts"] = millis();
    send(doc);
  }

  template <class SensorData>
  void sendSensors(const SensorData& s) {
    StaticJsonDocument<256> doc;
    doc["type"] = "sensor_data";
    doc["light"] = s.light;
    doc["motion"] = s.motion;
    doc["distance_mm"] = s.distance_mm;
    doc["touch_head"] = s.touchHead;
    doc["touch_side"] = s.touchSide;
    doc["sound"] = s.soundLevel;
    send(doc);
  }

private:
  WebSocketsClient ws_;
  bool connected_ = false;
  MessageCallback cb_;
  String serverHost_ = "";
  int serverPort_ = 3000;

  void handle(WStype_t type, uint8_t* payload, size_t len) {
    switch (type) {
      case WStype_CONNECTED: {
        connected_ = true;
        StaticJsonDocument<128> doc;
        doc["type"] = "hello";
        doc["device"] = DEVICE_NAME;
        doc["version"] = FW_VERSION;
        send(doc);
        Serial.println("WebSocket connected");
        break;
      }
      case WStype_DISCONNECTED:
        connected_ = false;
        Serial.println("WebSocket disconnected");
        break;
      case WStype_TEXT: {
        StaticJsonDocument<512> doc;
        auto err = deserializeJson(doc, payload, len);
        if (err) {
          Serial.printf("WS JSON error: %s\n", err.c_str());
          return;
        }
        const char* t = doc["type"] | "unknown";
        if (cb_) cb_(t, doc);
        break;
      }
      default:
        break;
    }
  }
};

#endif // WEBSOCKET_CLIENT_H

#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Message types for the queue
enum WsMessageType {
  WS_MSG_NONE = 0,
  WS_MSG_SET_BEHAVIOR,
  WS_MSG_SERVO_ACTION,
  WS_MSG_LED_ACTION,
  WS_MSG_PLAY_AUDIO,
  WS_MSG_REQUEST_STATE,
  WS_MSG_STOPWATCH_START,
  WS_MSG_STOPWATCH_STOP,
  WS_MSG_STOPWATCH_RESET,
  WS_MSG_SET_ALARM,
  WS_MSG_DISMISS_ALARM,
  WS_MSG_SYNC_TIME,
  WS_MSG_STOP_LISTENING
};

// Queue message structure
struct WsQueueMessage {
  WsMessageType type;
  char data[128];      // For strings like behavior name, color, URL, timestamp
  int intValue;        // For integers like servo angle or alarm hour
  int intValue2;       // For secondary integers like alarm minute
};

class RobotWebSocket {
private:
  WebSocketsClient ws;
  bool connected = false;
  bool pendingConnectStatus = false;
  String serverHost;
  int serverPort;
  QueueHandle_t messageQueue = NULL;

  void handleEvent(WStype_t type, uint8_t* payload, size_t len) {
    switch(type) {
      case WStype_DISCONNECTED:
        if (connected) Serial.println(F("[WS] Disconnected"));
        connected = false;
        break;
        
      case WStype_CONNECTED:
        Serial.printf("[WS] Connected to %s\n", (char*)payload);
        connected = true;
        pendingConnectStatus = true; // Defer sending status to loop() outside callback context
        break;
        
      case WStype_TEXT:
        handleMessage(payload, len);
        break;
        
      default:
        break;
    }
  }

  void handleMessage(uint8_t* payload, size_t len) {
    // Local stack JsonDocument automatically frees heap allocations upon function exit in ArduinoJson 7
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, len);
    if (err) {
      Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
      return;
    }

    const char* msgType = doc["type"];
    if (!msgType) return;

    WsQueueMessage qMsg;
    memset(&qMsg, 0, sizeof(qMsg));
    bool sendToQueue = true;

    if (strcmp(msgType, "set_behavior") == 0) {
      qMsg.type = WS_MSG_SET_BEHAVIOR;
      const char* name = doc["name"];
      if (name) {
        strncpy(qMsg.data, name, 127);
      }
    }
    else if (strcmp(msgType, "servo_action") == 0) {
      qMsg.type = WS_MSG_SERVO_ACTION;
      qMsg.intValue = doc["angle"] | 90;
    }
    else if (strcmp(msgType, "led_action") == 0) {
      qMsg.type = WS_MSG_LED_ACTION;
      const char* color = doc["color"];
      if (color) {
        strncpy(qMsg.data, color, 127);
      }
    }
    else if (strcmp(msgType, "play_audio") == 0) {
      qMsg.type = WS_MSG_PLAY_AUDIO;
      const char* url = doc["url"];
      if (url) {
        strncpy(qMsg.data, url, 127);
      }
    }
    else if (strcmp(msgType, "request_state") == 0) {
      qMsg.type = WS_MSG_REQUEST_STATE;
    }
    else if (strcmp(msgType, "stopwatch_start") == 0) {
      qMsg.type = WS_MSG_STOPWATCH_START;
    }
    else if (strcmp(msgType, "stopwatch_stop") == 0) {
      qMsg.type = WS_MSG_STOPWATCH_STOP;
    }
    else if (strcmp(msgType, "stopwatch_reset") == 0) {
      qMsg.type = WS_MSG_STOPWATCH_RESET;
    }
    else if (strcmp(msgType, "set_alarm") == 0) {
      qMsg.type = WS_MSG_SET_ALARM;
      qMsg.intValue = doc["hour"] | 0;
      qMsg.intValue2 = doc["minute"] | 0;
    }
    else if (strcmp(msgType, "dismiss_alarm") == 0) {
      qMsg.type = WS_MSG_DISMISS_ALARM;
    }
    else if (strcmp(msgType, "sync_time") == 0) {
      qMsg.type = WS_MSG_SYNC_TIME;
      const char* timestamp = doc["timestamp"];
      if (timestamp) {
        strncpy(qMsg.data, timestamp, 127);
      }
    }
    else if (strcmp(msgType, "stop_listening") == 0) {
      qMsg.type = WS_MSG_STOP_LISTENING;
    }
    else {
      sendToQueue = false;
    }

    if (sendToQueue && qMsg.type != WS_MSG_NONE) {
      if (messageQueue != NULL && xQueueSend(messageQueue, &qMsg, 0) != pdTRUE) {
        Serial.println(F("[WS] Queue full, message dropped"));
      }
    }
  }

  String discoverServerIP() {
    WiFiUDP udp;
    if (!udp.begin(3001)) return "";

    IPAddress localIP = WiFi.localIP();
    IPAddress subnetBroadcast = localIP;
    subnetBroadcast[3] = 255; // Subnet directed broadcast (e.g. 10.250.209.255)

    Serial.printf("[UDP Auto-Discovery] Broadcasting query to 255.255.255.255 and %s:3001...\n", subnetBroadcast.toString().c_str());

    for (int attempt = 0; attempt < 4; attempt++) {
      // 1. General broadcast
      udp.beginPacket(IPAddress(255, 255, 255, 255), 3001);
      udp.print("DISCOVER_DESKBOT");
      udp.endPacket();

      // 2. Directed subnet broadcast (fixes lwIP socket filtering on Android Hotspots)
      udp.beginPacket(subnetBroadcast, 3001);
      udp.print("DISCOVER_DESKBOT");
      udp.endPacket();

      unsigned long start = millis();
      while (millis() - start < 450) {
        int packetSize = udp.parsePacket();
        if (packetSize > 0) {
          char reply[64] = {0};
          udp.read(reply, 63);
          String repStr = String(reply);
          if (repStr.startsWith("DESKBOT_SERVER:")) {
            IPAddress remoteIP = udp.remoteIP();
            Serial.printf("[UDP Auto-Discovery] SUCCESS! Server found at IP: %s\n", remoteIP.toString().c_str());
            udp.stop();
            return remoteIP.toString();
          }
        }
        delay(10);
      }
    }
    udp.stop();
    Serial.println(F("[UDP Auto-Discovery] Broadcast timeout. Falling back to mDNS / Config IP..."));
    return "";
  }

public:
  void setServer(const char* host, int port) {
    serverHost = host;
    serverPort = port;
  }

  void begin() {
    // Delete previous queue to prevent memory leaks if begin() is called multiple times
    if (messageQueue != NULL) {
      vQueueDelete(messageQueue);
      messageQueue = NULL;
    }

    // Queue depth 8: handles burst of commands without dropping
    messageQueue = xQueueCreate(8, sizeof(WsQueueMessage));
    if (messageQueue == NULL) {
      Serial.println(F("[WS] Failed to create message queue!"));
    }

    const char* host = serverHost.length() > 0 ? serverHost.c_str() : WS_HOST;
    int port = serverPort > 0 ? serverPort : WS_PORT;

    String resolvedHost = host;

    // 1. Try UDP Auto-Discovery Broadcast first (Instant & Works on all Android Hotspots)
    String discoveredIP = discoverServerIP();
    if (discoveredIP.length() > 0) {
      resolvedHost = discoveredIP;
    }
    else if (String(host).endsWith(".local")) {
      MDNS.begin("nisya-companion");
      String mname = String(host);
      mname.replace(".local", "");
      Serial.printf("[mDNS] Querying host: %s.local...\n", mname.c_str());
      IPAddress ip = MDNS.queryHost(mname);
      if (ip != IPAddress(0, 0, 0, 0)) {
        resolvedHost = ip.toString();
        Serial.printf("[mDNS] Resolved %s.local -> %s\n", host, resolvedHost.c_str());
      } else {
        Serial.printf("[mDNS] Resolution failed for %s\n", host);
      }
    }

    Serial.printf("[WS] Connecting to %s:%d%s\n", resolvedHost.c_str(), port, WS_PATH);
    
    ws.begin(resolvedHost.c_str(), port, WS_PATH);
    ws.onEvent([this](WStype_t type, uint8_t* payload, size_t len) {
      handleEvent(type, payload, len);
    });
    ws.setReconnectInterval(10000);
    ws.enableHeartbeat(20000, 5000, 3);
  }

  void loop() { 
    ws.loop(); 
    if (connected && pendingConnectStatus) {
      pendingConnectStatus = false;
      sendStatus("connect", "online");
    }
  }
  bool isConnected() { return connected; }

  // Get next message from queue (non-blocking)
  bool getMessage(WsQueueMessage& msg) {
    if (messageQueue == NULL) return false;
    return xQueueReceive(messageQueue, &msg, 0) == pdTRUE;
  }

  void sendStatus(const char* event, const char* detail) {
    if (!connected) return;
    
    JsonDocument doc;
    doc["type"] = "robot_status";
    doc["event"] = event;
    doc["detail"] = detail;
    
    char output[128];
    serializeJson(doc, output, sizeof(output));
    ws.sendTXT(output);
  }

  void sendSensors(const SensorData& s) {
    if (!connected) return;
    
    JsonDocument doc;
    doc["type"] = "sensor_data";
    doc["light"] = s.light;
    doc["motion"] = s.motion;
    doc["distance_mm"] = s.distance_mm;
    doc["touch_head"] = s.touchHead;
    doc["touch_side"] = s.touchSide;
    doc["temperature"] = s.temperature;
    
    char output[192];
    serializeJson(doc, output, sizeof(output));
    ws.sendTXT(output);
  }

  // Stream raw PCM audio data over WebSocket
  void sendAudioChunk(const uint8_t* pcmData, size_t len) {
    if (!connected || pcmData == nullptr || len == 0) return;
    ws.sendBIN(pcmData, len);
  }

  void sendRaw(const char* json) {
    if (!connected) return;
    ws.sendTXT(json);
  }
};

#endif

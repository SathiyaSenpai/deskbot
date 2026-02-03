#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
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
  WS_MSG_STOPWATCH_RESET
};

// Queue message structure
struct WsQueueMessage {
  WsMessageType type;
  char data[128];      // For strings like behavior name, color, URL
  int intValue;        // For integers like servo angle
};

class RobotWebSocket {
private:
  WebSocketsClient ws;
  bool connected = false;
  String serverHost;
  int serverPort;
  QueueHandle_t messageQueue;

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
        break;
        
      case WStype_TEXT:
        handleMessage(payload, len);
        break;
        
      default:
        break;
    }
  }

  void handleMessage(uint8_t* payload, size_t len) {
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload);
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
    else {
      sendToQueue = false;
    }

    if (sendToQueue && qMsg.type != WS_MSG_NONE) {
      if (xQueueSend(messageQueue, &qMsg, 0) != pdTRUE) {
        Serial.println("[WS] Queue full, message dropped");
      }
    }
  }

public:
  void setServer(const char* host, int port) {
    serverHost = host;
    serverPort = port;
  }

  void begin() {
    // Create FreeRTOS queue for messages
    messageQueue = xQueueCreate(10, sizeof(WsQueueMessage));
    if (messageQueue == NULL) {
      Serial.println("[WS] Failed to create message queue!");
    }

    const char* host = serverHost.length() > 0 ? serverHost.c_str() : WS_HOST;
    int port = serverPort > 0 ? serverPort : WS_PORT;

    Serial.printf("[WS] Connecting to %s:%d%s\n", host, port, WS_PATH);
    
    ws.begin(host, port, WS_PATH);
    ws.onEvent([this](WStype_t type, uint8_t* payload, size_t len) {
      handleEvent(type, payload, len);
    });
    ws.setReconnectInterval(10000);
    ws.enableHeartbeat(20000, 5000, 3);
  }

  void loop() { ws.loop(); }
  bool isConnected() { return connected; }

  // Get next message from queue (non-blocking)
  bool getMessage(WsQueueMessage& msg) {
    return xQueueReceive(messageQueue, &msg, 0) == pdTRUE;
  }

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
    
    String output;
    serializeJson(doc, output);
    ws.sendTXT(output);
  }
  
  void sendRaw(const char* json) {
    if (!connected) return;
    ws.sendTXT(json);
  }
};

#endif

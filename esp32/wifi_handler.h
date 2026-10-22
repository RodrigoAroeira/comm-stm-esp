#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <cstring>
#include <string>

#include "protocol.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
// Fallback so a fresh clone still builds; override with esp32/secrets.h.
#define WIFI_SSID "OptmaLab2"
#define WIFI_PASS "optma_lab_at"
#endif

// The server is exposed through an accessor instead of a namespace-scope
// global: a function-local static in an inline function is guaranteed to be a
// single instance for the whole program, so this header can never produce a
// duplicate symbol nor a second HTTP server if another .cpp includes it.
inline AsyncWebServer &http_server() {
  static AsyncWebServer instance(80);
  return instance;
}

inline void start_wifi() {
  // Wi-Fi Credentials
  const char *ssid = WIFI_SSID;
  const char *passphrase = WIFI_PASS;

  Serial.println("Beginning Wi-Fi connection...");

  // Connect to Wi-Fi
  WiFi.begin(ssid, passphrase);

  // Bounded wait: a missing AP must not block the rest of setup()
  // (Bluetooth in particular) forever.
  constexpr uint32_t kConnectTimeoutMs = 20000;
  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - startedAt) < kConnectTimeoutMs) {
    delay(500);
    Serial.print(".");
    Serial.flush();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWi-Fi connection failed; HTTP endpoint disabled.");
    return;
  }
  Serial.println("\nWi-Fi Connected! IP Address: " + WiFi.localIP().toString());

  // HTTP Endpoint: POST http://<esp32-ip>/api/run-test
  http_server().on(
      "/api/run-test", HTTP_POST, [](AsyncWebServerRequest *request) {},
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        // Null-terminate the incoming string
        char jsonBuf[len + 1];
        memcpy(jsonBuf, data, len);
        jsonBuf[len] = '\0';

        Serial.println("Received json via HTTP:");
        Serial.println(jsonBuf);
        // Convert and forward to STM32
        const char *status;
        int code;
        if (processAndSendJson(jsonBuf)) {
          status = "FORWARD_SUCCESS";
          code = 200;
        } else {
          status = "FORWARD_FAIL";
          code = 400;
        }
        std::string content =
            std::string("{\"status\":") + '"' + status + "\"}";
        request->send(code, "application/json", content.c_str());
      });

  http_server().begin();
}

#endif // WIFI_HANDLER_H

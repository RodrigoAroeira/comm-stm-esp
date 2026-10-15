#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <cstring>

#include "../shared.h"

#define STM32_UART_RX_PIN 16 // Reader
#define STM32_UART_TX_PIN 17 // Transmiter

HardwareSerial STM32_Serial(1);
AsyncWebServer server(80);

// #define ssid "esp32"
// #define passphrase "optma"

// Forward declaration
bool processAndSendJson(const char *jsonStr);

void start_wifi() {
  // Wi-Fi Credentials
  constexpr const char *ssid = "OptmaLab2";
  constexpr const char *passphrase = "optma_lab_at";

  Serial.println("Beginning...");
  // USB Serial for PC Debug / Direct Cable Interface
  Serial.begin(115200);

  // Hardware UART to STM32
  STM32_Serial.begin(115200, SERIAL_8N1, STM32_UART_RX_PIN, STM32_UART_TX_PIN);

  // Connect to Wi-Fi
  WiFi.begin(ssid, passphrase);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    Serial.flush();
  }
  Serial.println("\nWi-Fi Connected! IP Address: " + WiFi.localIP().toString());

  // HTTP Endpoint: POST http://<esp32-ip>/api/run-test
  server.on(
      "/api/run-test", HTTP_POST, [](AsyncWebServerRequest *request) {},
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        // Null-terminate the incoming string
        char jsonBuf[len + 1];
        memcpy(jsonBuf, data, len);
        jsonBuf[len] = '\0';

        Serial.println("Received json:");
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

  server.begin();
}

void setup() { start_wifi(); }

void loop() {
  // Listen on USB Cable (Serial Monitor / PC Control)
  if (Serial.available()) {
    String jsonStr = Serial.readStringUntil('\n');
    jsonStr.trim();
    if (jsonStr.length() > 0) {
      processAndSendJson(jsonStr.c_str());
    }
  }
}

// Convert incoming JSON -> TestPacket_t C-Struct -> UART Bytes
bool processAndSendJson(const char *jsonStr) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);

  if (error) {
    Serial.print("JSON Parsing Failed: ");
    Serial.println(error.f_str());
    return false;
  }

  TestPacket_t packet = {0};

  // 1. Map Capacitor Model Enum
  const char *modelStr = doc["cap_model"] | "";
  if (strlen(modelStr) == 0) {
    return false;
  }
  if (strcmp(modelStr, "MODEL_A_100F") == 0)
    packet.cap_model = SUPERCAP_MODEL_A_100F;
  else if (strcmp(modelStr, "MODEL_B_350F") == 0)
    packet.cap_model = SUPERCAP_MODEL_B_350F;
  else if (strcmp(modelStr, "MODEL_C_500F") == 0)
    packet.cap_model = SUPERCAP_MODEL_C_500F;

  // 2. Map Test Type & Specific Union Parameters
  const char *testTypeStr = doc["test_type"] | "";
  if (strlen(testTypeStr) == 0) {
    return false;
  }

  if (strcmp(testTypeStr, "ESR") == 0) {
    packet.test_type = TEST_TYPE_ESR;
    packet.params.esr.pulse_current_ma = doc["params"]["pulse_ma"] | 0;
    packet.params.esr.pulse_duration_ms = doc["params"]["pulse_ms"] | 0;
  } else if (strcmp(testTypeStr, "CAPACITANCE") == 0) {
    packet.test_type = TEST_TYPE_CAPACITANCE;
    packet.params.cap.charge_current_ma = doc["params"]["charge_ma"] | 0;
    packet.params.cap.max_voltage_mv = doc["params"]["max_mv"] | 0;
    packet.params.cap.hold_time_sec = doc["params"]["hold_sec"] | 0;
  } else if (strcmp(testTypeStr, "LEAKAGE") == 0) {
    packet.test_type = TEST_TYPE_LEAKAGE_CURRENT;
    packet.params.leakage.test_voltage_mv = doc["params"]["test_mv"] | 0;
    packet.params.leakage.soak_time_sec = doc["params"]["soak_sec"] | 0;
  }

  // 3. Output Binary Struct to STM32 UART
  if (STM32_Serial.write((uint8_t *)&packet, sizeof(TestPacket_t))) {
    Serial.println("Packet forwarded to STM32.");
    return true;
  }

  Serial.println("Unable to send packet to STM32.");
  return false;
}

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstring>

#include "../shared.h"
#include "bluetooth_handler.h"
#include "protocol.h"
#include "wifi_handler.h"

#define STM32_UART_RX_PIN 16 // Reader
#define STM32_UART_TX_PIN 17 // Transmiter

HardwareSerial STM32_Serial(1);

void setup() {
  Serial.begin(115200);
  STM32_Serial.begin(115200, SERIAL_8N1, STM32_UART_RX_PIN, STM32_UART_TX_PIN);

  start_wifi();
  start_bluetooth();
}

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

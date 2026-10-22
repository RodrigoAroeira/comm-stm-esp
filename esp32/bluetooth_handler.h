#ifndef BLUETOOTH_HANDLER_H
#define BLUETOOTH_HANDLER_H

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <string>

#include "protocol.h"

// Accessors instead of namespace-scope globals: a function-local static in an
// inline function is a single instance for the whole program, so this header
// can neither produce a duplicate symbol nor spawn a second Bluetooth stack
// if another .cpp includes it.
inline BluetoothSerial &bt_serial() {
  static BluetoothSerial instance;
  return instance;
}

// Accumulates SPP chunks until a complete newline-terminated line is available.
inline String &bt_rx_line() {
  static String buffer;
  return buffer;
}

inline void send_bluetooth_status(bool ok) {
  const char *status = ok ? "FORWARD_SUCCESS" : "FORWARD_FAIL";
  std::string content = std::string("{\"status\":") + '"' + status + "\"}\n";
  bt_serial().print(content.c_str());
}

// SPP hands us arbitrary chunks, so we frame on '\n' instead of assuming
// that one chunk == one JSON document.
inline void handle_bluetooth_data(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    const char c = static_cast<char>(data[i]);
    if (c == '\n' || c == '\r') {
      String &line = bt_rx_line();
      line.trim();
      if (line.length() > 0) {
        Serial.println("Received json via Bluetooth:");
        Serial.println(line);
        send_bluetooth_status(processAndSendJson(line.c_str()));
      }
      line = "";
      continue;
    }
    bt_rx_line() += c;
  }
}

inline void start_bluetooth() {
  constexpr const char *device_name = "ESP32_Test_Gateway";

  Serial.println("Beginning Bluetooth...");

  // Initialize Bluetooth Classic
  if (!bt_serial().begin(device_name)) {
    Serial.println("An error occurred initializing Bluetooth");
    return;
  }
  Serial.println("Bluetooth Started! Device Name: " + String(device_name));

  // Registering a data callback diverts traffic away from the internal RX
  // queue (BluetoothSerial.cpp:337), so available()/read() never see it and
  // all framing has to happen here.
  bt_serial().onData([](const uint8_t *data, size_t len) {
    handle_bluetooth_data(data, len);
  });
}

#endif // BLUETOOTH_HANDLER_H

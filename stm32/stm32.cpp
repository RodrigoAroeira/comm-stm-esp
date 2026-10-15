#include <Arduino.h>
#include "../shared.h"

// ---------------------------------------------------------------------------
// STM32 receiver
//
// The ESP32 "sender" (see sender/sender.cpp) parses incoming JSON and forwards
// a TestPacket_t over UART as a raw binary struct. This sketch reads those
// bytes back into a TestPacket_t and reports the decoded test request.
//
// Wiring (ESP32 -> STM32):
//   ESP32 TX (GPIO17) -> STM32 RX (PA10 / USART1_RX)
//   ESP32 RX (GPIO16) <- STM32 TX (PA9  / USART1_TX)
//   GND               <-> GND
//
// NOTE: the sender writes sizeof(TestPacket_t) bytes with no header/framing.
// If a byte is ever dropped the stream desynchronizes; add a sync byte or
// sequence number on both ends if that becomes a problem.
// ---------------------------------------------------------------------------

#define LINK_BAUD 115200

HardwareSerial &Link = Serial1; // USART1 on STM32F103 (Blue Pill)

static uint8_t rxBuffer[sizeof(TestPacket_t)];
static size_t rxCount = 0;

static const char *capModelName(uint8_t model) {
  switch (model) {
    case SUPERCAP_MODEL_A_100F: return "MODEL_A_100F";
    case SUPERCAP_MODEL_B_350F: return "MODEL_B_350F";
    case SUPERCAP_MODEL_C_500F: return "MODEL_C_500F";
    default:                    return "UNKNOWN";
  }
}

static void handlePacket(const TestPacket_t &packet) {
  Serial.print(F("[RX] cap_model="));
  Serial.print(capModelName(packet.cap_model));
  Serial.print(F(" test_type="));

  switch (packet.test_type) {
    case TEST_TYPE_ESR:
      Serial.println(F("ESR"));
      Serial.print(F("     pulse_current_ma="));
      Serial.println(packet.params.esr.pulse_current_ma);
      Serial.print(F("     pulse_duration_ms="));
      Serial.println(packet.params.esr.pulse_duration_ms);
      break;

    case TEST_TYPE_CAPACITANCE:
      Serial.println(F("CAPACITANCE"));
      Serial.print(F("     charge_current_ma="));
      Serial.println(packet.params.cap.charge_current_ma);
      Serial.print(F("     max_voltage_mv="));
      Serial.println(packet.params.cap.max_voltage_mv);
      Serial.print(F("     hold_time_sec="));
      Serial.println(packet.params.cap.hold_time_sec);
      break;

    case TEST_TYPE_LEAKAGE_CURRENT:
      Serial.println(F("LEAKAGE"));
      Serial.print(F("     test_voltage_mv="));
      Serial.println(packet.params.leakage.test_voltage_mv);
      Serial.print(F("     soak_time_sec="));
      Serial.println(packet.params.leakage.soak_time_sec);
      break;

    default:
      Serial.println(F("NONE/UNKNOWN"));
      break;
  }

  // TODO: hand the decoded parameters to the actual supercapacitor test logic.
}

void setup() {
  Serial.begin(115200);  // USB serial (debug)
  Link.begin(LINK_BAUD); // UART link from the ESP32 sender
  rxCount = 0;
  Serial.println(F("STM32 receiver ready."));
}

void loop() {
  while (Link.available()) {
    rxBuffer[rxCount++] = static_cast<uint8_t>(Link.read());

    if (rxCount == sizeof(TestPacket_t)) {
      TestPacket_t packet;
      memcpy(&packet, rxBuffer, sizeof(TestPacket_t));
      handlePacket(packet);
      rxCount = 0;
    }
  }
}

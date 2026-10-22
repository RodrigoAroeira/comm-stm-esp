#ifndef ESP32_PROTOCOL_H
#define ESP32_PROTOCOL_H

// Defined in sender.cpp: converts an incoming JSON document into the binary
// TestPacket_t struct that is written to the STM32 over UART.
// Returns true when the packet was successfully forwarded.
bool processAndSendJson(const char *jsonStr);

#endif // ESP32_PROTOCOL_H

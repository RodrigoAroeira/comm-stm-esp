#ifndef SHARED_H
#define SHARED_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 1. Enum for Supercapacitor Models
typedef enum SupercapType {
  SUPERCAP_MODEL_A_100F = 0,
  SUPERCAP_MODEL_B_350F = 1,
  SUPERCAP_MODEL_C_500F = 2
} SupercapType_t;

typedef enum TestType {
  TEST_TYPE_NONE = 0,
  TEST_TYPE_ESR,
  TEST_TYPE_CAPACITANCE,
  TEST_TYPE_LEAKAGE_CURRENT
} TestType_t;

typedef struct {
  uint16_t pulse_current_ma;  // Pulse magnitude
  uint16_t pulse_duration_ms; // Pulse duration
} ESRParams_t;

typedef struct {
  uint16_t charge_current_ma; // Constant charge current
  uint16_t max_voltage_mv;    // Cutoff threshold
  uint16_t hold_time_sec;     // Hold duration at full charge
} CapParams_t;

typedef struct {
  uint16_t test_voltage_mv; // Hold voltage
  uint16_t soak_time_sec;   // Stabilization time before measurement
} LeakageParams_t;

#pragma pack(push, 1)
typedef struct {
  uint8_t cap_model; // Cast to/from SupercapType_t
  uint8_t test_type; // Cast to/from TestType_t

  // Union: Memory is shared between test configurations.
  // Memory footprint is only as large as the LARGEST struct inside (6 bytes).
  union {
    ESRParams_t esr;
    CapParams_t cap;
    LeakageParams_t leakage;
  } params;
} TestPacket_t;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif

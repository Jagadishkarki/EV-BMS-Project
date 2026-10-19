#pragma once
// ============================================================================
// RelaySafetyConfig.h
// Tunable constants for the non-blocking protection relay (Task 2).
// All timing is expressed in milliseconds and evaluated against millis(),
// never blocked on with delay().
// ============================================================================

#include <stdint.h>

// ---- Protected-parameter thresholds (example: pack current, amps) --------
constexpr float TRIP_CURRENT_A   = 25.0f;  // trip if current stays above this
constexpr float RESET_CURRENT_A  = 21.0f;  // must drop below this to start recovery
// Gap between TRIP and RESET is the hysteresis band that prevents chatter
// right at the boundary — the relay can't trip/reset/trip on tiny wobble.

// ---- Debounce / anti-chatter timing ---------------------------------------
constexpr uint32_t FAULT_DEBOUNCE_MS     = 300;   // condition must persist this long before tripping
constexpr uint32_t RECOVERY_HOLD_MS      = 5000;  // cool-down after condition clears, relay still open
constexpr uint32_t RECOVERY_VERIFY_MS    = 2000;  // must stay clean this long before relay re-closes

// ---- Sensor monitor parameters --------------------------------------------
constexpr float    SENSOR_MIN_VALID      = -5.0f;   // out-of-range low bound (amps)
constexpr float    SENSOR_MAX_VALID      = 60.0f;   // out-of-range high bound (amps)
constexpr float    SENSOR_MAX_SLEW_PER_MS= 0.5f;    // physically impossible rate of change (A/ms)
constexpr uint16_t SENSOR_FROZEN_WINDOW_MS = 4000;  // unchanged-for-this-long => frozen fault
constexpr float    SENSOR_FROZEN_EPSILON = 0.002f;  // "unchanged" tolerance (amps)

constexpr float    SPIKE_SIGMA           = 3.0f;    // deviations beyond this many std-devs = statistical spike
constexpr uint8_t  PERSIST_CONFIRM       = 3;        // consecutive confirming samples = genuine rapid change, not noise

// ---- Relay pin (adjust to your wiring) -------------------------------------
constexpr uint8_t  RELAY_PIN             = 26;  // ESP32 GPIO driving the relay/MOSFET gate

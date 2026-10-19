#pragma once
// ============================================================================
// FsmConfig.h
// Tunable constants for the system state machine and fault detectors (Task 4).
// All values are starting points; tune them against your real hardware.
// ============================================================================

#include <stdint.h>

// ---- State machine timing --------------------------------------------------
constexpr uint32_t DEGRADED_MIN_DWELL_MS  = 1000;   // stay in DEGRADED at least this long (anti-cycling)
constexpr uint32_t DEGRADED_ESCALATION_MS = 10000;  // DEGRADED that never clears escalates to FAILSAFE

// Repeated-fault escalation: N DEGRADED entries within the window => FAILSAFE.
constexpr uint8_t  FAULT_REPEAT_LIMIT     = 3;
constexpr uint32_t FAULT_REPEAT_WINDOW_MS = 30000;

// ---- FAILSAFE recovery -------------------------------------------------------
constexpr uint32_t FAILSAFE_HOLD_MS   = 5000;  // conditions must stay clear this long before verification starts
constexpr uint8_t  MAX_BACKOFF_STEPS  = 3;     // hold time grows up to this multiple after repeated FAILSAFE entries
constexpr uint32_t STRIKE_RESET_MS    = 30000; // continuous calm operation this long forgets past FAILSAFE entries
constexpr uint32_t VERIFY_MS          = 4000;  // post-hold window: must remain fault-free this long before NORMAL

// ---- Terminal SHUTDOWN escalation ---------------------------------------------
// If FAILSAFE recovery fails this many times in a row (verification keeps
// failing / faults keep recurring), the system gives up on automatic
// recovery and goes to SHUTDOWN, which requires an explicit external reset.
constexpr uint8_t  SHUTDOWN_STRIKE_LIMIT = 4;

// ---- ADC detectors -----------------------------------------------------------
constexpr uint8_t  MAX_ADC_CHANNELS      = 8;      // fixed-size channel array in FaultManager
constexpr uint32_t ADC_FROZEN_MS         = 3000;   // identical raw counts for this long => frozen
constexpr uint16_t ADC_FROZEN_TOLERANCE  = 0;      // LSBs; 0 = must be bit-identical
constexpr uint16_t ADC_MAX_COUNTS        = 4095;   // 12-bit ESP32 ADC
constexpr uint16_t ADC_RAIL_MARGIN       = 8;      // counts within this of 0 / max are treated as railed

// ---- Relay feedback ------------------------------------------------------------
constexpr uint32_t RELAY_SETTLE_MS       = 150;    // ignore feedback right after a command change
constexpr uint32_t RELAY_MISMATCH_MS     = 300;    // mismatch must persist this long to count

// ---- Communication watchdog ------------------------------------------------------
constexpr uint32_t COMM_TIMEOUT_MS       = 2000;   // no valid message for this long => timeout

// ---- Transition log ------------------------------------------------------------------
constexpr uint8_t  FSM_LOG_CAPACITY      = 32;

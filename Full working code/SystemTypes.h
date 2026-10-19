#pragma once
// ============================================================================
// SystemTypes.h
// Shared vocabulary used across Task 4 (FSM), Task 5 (telemetry), and
// Task 6 (analytics dashboard). Lives in Task4_FaultFsm because Task 4 is
// the module that owns the operating-state model; the others depend on it
// (see each library's library.properties "depends" field) rather than
// redefining their own copies.
// ============================================================================

#include <stdint.h>

// Exactly the four operating states required by Task 4.
enum class OpState : uint8_t {
    NORMAL = 0,
    DEGRADED,
    FAILSAFE,
    SHUTDOWN
};

enum class FaultSource : uint8_t {
    NONE = 0,
    CELL,
    RELAY,
    COMM,
    ADC
};

inline const char* opStateName(OpState s) {
    switch (s) {
        case OpState::NORMAL:   return "NORMAL";
        case OpState::DEGRADED: return "DEGRADED";
        case OpState::FAILSAFE: return "FAILSAFE";
        case OpState::SHUTDOWN: return "SHUTDOWN";
        default:                return "?";
    }
}

inline const char* faultSourceName(FaultSource f) {
    switch (f) {
        case FaultSource::CELL:  return "CELL";
        case FaultSource::RELAY: return "RELAY";
        case FaultSource::COMM:  return "COMM";
        case FaultSource::ADC:   return "ADC";
        default:                 return "NONE";
    }
}

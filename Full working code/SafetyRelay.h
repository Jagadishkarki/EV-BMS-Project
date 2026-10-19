#pragma once
// ============================================================================
// SafetyRelay.h
// Task 2: Non-Blocking Protection Relay and Safety System
//
// A timer/state-machine driven relay controller. No delay() anywhere —
// every timed behaviour (debounce, recovery hold, recovery verify) is
// implemented by storing a millis() timestamp and comparing elapsed time
// on each update() call, so the rest of the program (LCD, comms, other
// tasks) is never blocked.
//
// States:
//   NORMAL          - relay closed, everything nominal
//   FAULT_DEBOUNCE  - a trip condition appeared; timing to confirm it's
//                      real before acting (anti-chatter)
//   TRIPPED         - relay open, condition confirmed unsafe
//   RECOVERY_HOLD   - condition cleared; relay stays open for a fixed
//                      cool-down before we even consider closing it
//   RECOVERY_VERIFY - cool-down passed; relay still open while we watch
//                      for continued stability before fully resuming
// ============================================================================

#include <stdint.h>
#include "RelaySafetyConfig.h"
#include "SensorMonitor.h"

enum class RelayState : uint8_t {
    NORMAL,
    FAULT_DEBOUNCE,
    TRIPPED,
    RECOVERY_HOLD,
    RECOVERY_VERIFY
};

enum class TripReason : uint8_t {
    NONE,
    OVER_THRESHOLD,
    SENSOR_FROZEN,
    SENSOR_JUMP,
    SENSOR_OUT_OF_RANGE
};

struct RelayLogEntry {
    uint32_t    timestampMs;
    RelayState  prevState;
    RelayState  newState;
    TripReason  reason;
};

class SafetyRelay {
public:
    SafetyRelay();

    void begin(); // sets pin mode, closes relay, resets state

    // Call every loop() with the CLEAN value + anomaly result already
    // produced by a SensorMonitor::ingest() call this cycle.
    void update(float cleanValue, SensorAnomaly anomaly, uint32_t nowMs);

    RelayState getState()      const;
    bool       isRelayClosed() const;
    TripReason getLastReason() const;

    // Structured log access (for Task 4/5 reuse, or direct Serial printing)
    uint8_t getLogCount() const;
    const RelayLogEntry& getLogEntry(uint8_t indexFromNewest) const;

private:
    RelayState state;
    TripReason lastReason;

    uint32_t debounceStartMs;
    uint32_t recoveryHoldStartMs;
    uint32_t recoveryVerifyStartMs;

    static constexpr uint8_t LOG_CAPACITY = 16;
    RelayLogEntry logBuf[LOG_CAPACITY];
    uint8_t logHead;   // next write position (ring buffer)
    uint8_t logCount;

    void setRelay(bool closed);
    void transitionTo(RelayState newState, TripReason reason, uint32_t nowMs);
    bool conditionUnsafe(float cleanValue, SensorAnomaly anomaly, TripReason& outReason) const;
    bool conditionSafeForReset(float cleanValue, SensorAnomaly anomaly) const;
};

#pragma once
// ============================================================================
// Fsm.h
// Task 4: Fault State Machine with Structured Recovery
//
// Exactly four operating states, as required:
//   NORMAL    - everything nominal
//   DEGRADED  - a fault is active; system operating in a reduced/cautious mode
//   FAILSAFE  - DEGRADED escalated (never cleared in time, or faults repeated
//                too often); system held in a safe condition
//   SHUTDOWN  - terminal state: FAILSAFE recovery has failed too many times
//                in a row. No automatic exit — requires an explicit reset()
//                call (e.g. an operator/maintenance action), so the system
//                never silently keeps retrying a recovery that isn't working.
//
// FAILSAFE's "verification process" (hold, then watch for continued
// stability) is implemented as two internal timed phases inside the
// FAILSAFE state rather than as extra enum values, so the state machine
// still exposes exactly the four states the spec requires while still
// never returning to NORMAL on a single lucky reading.
//
// Fault isolation: every fault carries a FaultSource (CELL/RELAY/COMM/ADC)
// and a numeric ID. This is retained in `currentFaultSource`/`currentFaultId`
// for as long as the condition is unresolved.
// ============================================================================

#include <stdint.h>
#include "FsmConfig.h"
#include "SystemTypes.h"

enum class FsmEvent : uint8_t {
    NONE = 0,
    FAULT_RAISED,     // NORMAL -> DEGRADED
    FAULT_CLEARED,     // DEGRADED -> NORMAL
    ESCALATE,           // DEGRADED -> FAILSAFE
    RECOVERED,           // FAILSAFE -> NORMAL (hold + verification both passed)
    GIVE_UP               // FAILSAFE -> SHUTDOWN (too many failed recovery attempts)
};

struct FsmLogEntry {
    uint32_t    timestampMs;
    OpState     prevState;
    OpState     newState;
    FaultSource faultSource;
    uint16_t    faultId;
};

class Fsm {
public:
    Fsm();

    void begin(uint32_t nowMs);

    // Call every loop() with the aggregated fault picture for this tick
    // (typically from FaultManager::hasActiveFault()).
    void tick(bool faultActive, FaultSource src, uint16_t faultId, uint32_t nowMs);

    // Explicit external reset out of SHUTDOWN. Does nothing in any other
    // state. This is the one deliberate "manual" transition in the system.
    void reset(uint32_t nowMs);

    OpState     getState()           const;
    FaultSource getFaultSource()     const;
    uint16_t    getFaultId()         const;
    uint8_t     getFailsafeStrikes() const;

    uint8_t getLogCount() const;
    const FsmLogEntry& getLogEntry(uint8_t indexFromNewest) const;

private:
    OpState  state;
    uint32_t stateEnteredMs;

    FaultSource currentFaultSource;
    uint16_t    currentFaultId;

    // DEGRADED-state bookkeeping
    uint32_t degradedEnteredMs;
    uint32_t repeatTimestamps[FAULT_REPEAT_LIMIT];
    uint8_t  repeatHead;
    uint8_t  repeatCount;

    // FAILSAFE bookkeeping: two internal phases, HOLD then VERIFY
    enum class FailsafePhase : uint8_t { HOLD, VERIFY };
    FailsafePhase failsafePhase;
    uint32_t      failsafePhaseStartMs;
    uint8_t       failsafeStrikes; // consecutive failed recovery attempts
    bool          prevFaultActive;  // for rising-edge detection of a failed recovery attempt

    uint32_t lastUnsafeMs; // last time we were DEGRADED or FAILSAFE, for strike decay

    FsmLogEntry logBuf[FSM_LOG_CAPACITY];
    uint8_t logHead;
    uint8_t logCount;

    void applyTransition(OpState to, FsmEvent ev, FaultSource src, uint16_t faultId, uint32_t nowMs);
    void recordDegradedEntry(uint32_t nowMs);
    bool repeatLimitHit(uint32_t nowMs) const;
    uint32_t currentFailsafeHoldMs() const;
};

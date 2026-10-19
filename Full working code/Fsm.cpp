// ============================================================================
// Fsm.cpp
// ============================================================================

#include "Fsm.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Explicit transition table: the ONLY legal (from, event) -> to moves.
// Guard logic in tick() decides which event fires; this table is a pure,
// state-timing-free lookup, so the legal transition set is visible in one
// place rather than scattered across if/else chains.
// ---------------------------------------------------------------------------
struct TransitionRow {
    OpState  from;
    FsmEvent event;
    OpState  to;
};

static const TransitionRow TRANSITION_TABLE[] = {
    { OpState::NORMAL,   FsmEvent::FAULT_RAISED,  OpState::DEGRADED },
    { OpState::DEGRADED, FsmEvent::FAULT_CLEARED, OpState::NORMAL   },
    { OpState::DEGRADED, FsmEvent::ESCALATE,      OpState::FAILSAFE },
    { OpState::FAILSAFE, FsmEvent::RECOVERED,     OpState::NORMAL   },
    { OpState::FAILSAFE, FsmEvent::GIVE_UP,       OpState::SHUTDOWN },
    // SHUTDOWN has no table-driven exit — see reset().
};
static const uint8_t TRANSITION_TABLE_LEN = sizeof(TRANSITION_TABLE) / sizeof(TRANSITION_TABLE[0]);

static bool resolveTransition(OpState from, FsmEvent ev, OpState& outTo) {
    for (uint8_t i = 0; i < TRANSITION_TABLE_LEN; i++) {
        if (TRANSITION_TABLE[i].from == from && TRANSITION_TABLE[i].event == ev) {
            outTo = TRANSITION_TABLE[i].to;
            return true;
        }
    }
    return false;
}

Fsm::Fsm()
    : state(OpState::NORMAL), stateEnteredMs(0),
      currentFaultSource(FaultSource::NONE), currentFaultId(0),
      degradedEnteredMs(0), repeatHead(0), repeatCount(0),
      failsafePhase(FailsafePhase::HOLD), failsafePhaseStartMs(0), failsafeStrikes(0), prevFaultActive(false),
      lastUnsafeMs(0), logHead(0), logCount(0)
{
    for (uint8_t i = 0; i < FAULT_REPEAT_LIMIT; i++) repeatTimestamps[i] = 0;
    for (uint8_t i = 0; i < FSM_LOG_CAPACITY; i++) {
        logBuf[i] = { 0, OpState::NORMAL, OpState::NORMAL, FaultSource::NONE, 0 };
    }
}

void Fsm::begin(uint32_t nowMs) {
    state = OpState::NORMAL;
    stateEnteredMs = nowMs;
    currentFaultSource = FaultSource::NONE;
    currentFaultId = 0;
    failsafeStrikes = 0;
    lastUnsafeMs = nowMs;
}

void Fsm::recordDegradedEntry(uint32_t nowMs) {
    repeatTimestamps[repeatHead] = nowMs;
    repeatHead = (repeatHead + 1) % FAULT_REPEAT_LIMIT;
    if (repeatCount < FAULT_REPEAT_LIMIT) repeatCount++;
}

bool Fsm::repeatLimitHit(uint32_t nowMs) const {
    if (repeatCount < FAULT_REPEAT_LIMIT) return false;
    uint32_t oldest = repeatTimestamps[repeatHead]; // slot about to be overwritten = oldest
    return (nowMs - oldest) <= FAULT_REPEAT_WINDOW_MS;
}

uint32_t Fsm::currentFailsafeHoldMs() const {
    uint8_t steps = failsafeStrikes < MAX_BACKOFF_STEPS ? failsafeStrikes : MAX_BACKOFF_STEPS;
    return FAILSAFE_HOLD_MS * (uint32_t)(steps + 1);
}

void Fsm::applyTransition(OpState to, FsmEvent ev, FaultSource src, uint16_t faultId, uint32_t nowMs) {
    (void)ev; // event kept in the signature for traceability/debugging
    FsmLogEntry entry;
    entry.timestampMs = nowMs;
    entry.prevState    = state;
    entry.newState       = to;
    entry.faultSource      = src;
    entry.faultId           = faultId;

    logBuf[logHead] = entry;
    logHead = (logHead + 1) % FSM_LOG_CAPACITY;
    if (logCount < FSM_LOG_CAPACITY) logCount++;

    Serial.print(F("[FSM] t="));  Serial.print(nowMs);
    Serial.print(F("ms  "));      Serial.print(opStateName(state));
    Serial.print(F(" -> "));      Serial.print(opStateName(to));
    Serial.print(F("  src="));    Serial.print(faultSourceName(src));
    Serial.print(F(" id="));      Serial.println(faultId);

    state = to;
    stateEnteredMs = nowMs;

    if (src != FaultSource::NONE) {
        currentFaultSource = src;
        currentFaultId = faultId;
    }
    if (to == OpState::NORMAL) {
        currentFaultSource = FaultSource::NONE;
        currentFaultId = 0;
        failsafeStrikes = 0; // a verified recovery is a genuine success — reset strikes
    }
    if (to == OpState::DEGRADED) {
        degradedEnteredMs = nowMs;
        recordDegradedEntry(nowMs);
    }
    if (to == OpState::FAILSAFE) {
        failsafePhase = FailsafePhase::HOLD;
        failsafePhaseStartMs = nowMs;
        prevFaultActive = true; // the fault that caused escalation counts as "currently active"
    }
}

void Fsm::tick(bool faultActive, FaultSource src, uint16_t faultId, uint32_t nowMs) {
    const OpState stateAtStart = state; // transitions made during this tick don't count as strikes
    if (state == OpState::DEGRADED || state == OpState::FAILSAFE) {
        lastUnsafeMs = nowMs;
    } else if (state == OpState::NORMAL && failsafeStrikes > 0 && (nowMs - lastUnsafeMs) >= STRIKE_RESET_MS) {
        failsafeStrikes = 0; // sustained calm operation forgives past FAILSAFE entries
    }

    switch (state) {

        case OpState::NORMAL: {
            if (faultActive) {
                OpState to;
                if (resolveTransition(state, FsmEvent::FAULT_RAISED, to))
                    applyTransition(to, FsmEvent::FAULT_RAISED, src, faultId, nowMs);
            }
            break;
        }

        case OpState::DEGRADED: {
            if (faultActive) {
                currentFaultSource = src;
                currentFaultId = faultId;
            }
            uint32_t dwell = nowMs - degradedEnteredMs;

            if (faultActive && dwell >= DEGRADED_ESCALATION_MS) {
                OpState to;
                if (resolveTransition(state, FsmEvent::ESCALATE, to))
                    applyTransition(to, FsmEvent::ESCALATE, src, faultId, nowMs);
            } else if (repeatLimitHit(nowMs)) {
                OpState to;
                if (resolveTransition(state, FsmEvent::ESCALATE, to))
                    applyTransition(to, FsmEvent::ESCALATE, src, faultId, nowMs);
            } else if (!faultActive && dwell >= DEGRADED_MIN_DWELL_MS) {
                OpState to;
                if (resolveTransition(state, FsmEvent::FAULT_CLEARED, to))
                    applyTransition(to, FsmEvent::FAULT_CLEARED, FaultSource::NONE, 0, nowMs);
            }
            break;
        }

        case OpState::FAILSAFE: {
            if (faultActive) {
                // Any live fault during either phase restarts that phase's
                // timer — we never count down toward recovery while
                // something is actively wrong.
                failsafePhaseStartMs = nowMs;
                currentFaultSource = src;
                currentFaultId = faultId;
                break;
            }

            if (failsafePhase == FailsafePhase::HOLD) {
                if (nowMs - failsafePhaseStartMs >= currentFailsafeHoldMs()) {
                    failsafePhase = FailsafePhase::VERIFY;
                    failsafePhaseStartMs = nowMs;
                }
            } else { // VERIFY
                if (nowMs - failsafePhaseStartMs >= VERIFY_MS) {
                    OpState to;
                    if (resolveTransition(state, FsmEvent::RECOVERED, to))
                        applyTransition(to, FsmEvent::RECOVERED, FaultSource::NONE, 0, nowMs);
                }
            }
            break;
        }

        case OpState::SHUTDOWN:
            // No automatic exit. Waits for an explicit reset() call.
            break;
    }

    // A failed recovery attempt = the fault REAPPEARS (rising edge) while we
    // were already in FAILSAFE and had seen it clear. Counting rising edges,
    // not ticks, means a fault that simply persists is one strike at most,
    // and entering FAILSAFE never counts as a strike by itself. Too many
    // consecutive failed attempts => give up on automatic recovery.
    if (stateAtStart == OpState::FAILSAFE && state == OpState::FAILSAFE) {
        if (faultActive && !prevFaultActive) {
            if (failsafeStrikes < 255) failsafeStrikes++;
            if (failsafeStrikes >= SHUTDOWN_STRIKE_LIMIT) {
                OpState to;
                if (resolveTransition(state, FsmEvent::GIVE_UP, to))
                    applyTransition(to, FsmEvent::GIVE_UP, src, faultId, nowMs);
            }
        }
    }
    prevFaultActive = faultActive;
}

void Fsm::reset(uint32_t nowMs) {
    if (state != OpState::SHUTDOWN) return; // the only state reset() acts on
    FsmLogEntry entry = { nowMs, state, OpState::NORMAL, FaultSource::NONE, 0 };
    logBuf[logHead] = entry;
    logHead = (logHead + 1) % FSM_LOG_CAPACITY;
    if (logCount < FSM_LOG_CAPACITY) logCount++;

    Serial.println(F("[FSM] Explicit reset from SHUTDOWN -> NORMAL"));
    state = OpState::NORMAL;
    stateEnteredMs = nowMs;
    currentFaultSource = FaultSource::NONE;
    currentFaultId = 0;
    failsafeStrikes = 0;
}

OpState     Fsm::getState()           const { return state; }
FaultSource Fsm::getFaultSource()     const { return currentFaultSource; }
uint16_t    Fsm::getFaultId()         const { return currentFaultId; }
uint8_t     Fsm::getFailsafeStrikes() const { return failsafeStrikes; }

uint8_t Fsm::getLogCount() const { return logCount; }

const FsmLogEntry& Fsm::getLogEntry(uint8_t indexFromNewest) const {
    uint8_t realIndex = (logHead + FSM_LOG_CAPACITY - 1 - indexFromNewest) % FSM_LOG_CAPACITY;
    return logBuf[realIndex];
}

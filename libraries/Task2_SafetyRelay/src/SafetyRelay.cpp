// ============================================================================
// SafetyRelay.cpp
// ============================================================================

#include "SafetyRelay.h"
#include <Arduino.h>

SafetyRelay::SafetyRelay()
    : state(RelayState::NORMAL), lastReason(TripReason::NONE),
      debounceStartMs(0), recoveryHoldStartMs(0), recoveryVerifyStartMs(0),
      logHead(0), logCount(0)
{
    for (uint8_t i = 0; i < LOG_CAPACITY; i++) {
        logBuf[i] = { 0, RelayState::NORMAL, RelayState::NORMAL, TripReason::NONE };
    }
}

void SafetyRelay::begin() {
    pinMode(RELAY_PIN, OUTPUT);
    setRelay(true); // start closed / normal
    state = RelayState::NORMAL;
    lastReason = TripReason::NONE;
}

void SafetyRelay::setRelay(bool closed) {
    // Adjust polarity here if your relay/MOSFET driver is active-low.
    digitalWrite(RELAY_PIN, closed ? HIGH : LOW);
}

bool SafetyRelay::conditionUnsafe(float cleanValue, SensorAnomaly anomaly, TripReason& outReason) const {
    if (anomaly == SensorAnomaly::FROZEN)        { outReason = TripReason::SENSOR_FROZEN;      return true; }
    if (anomaly == SensorAnomaly::UNREALISTIC_JUMP) { outReason = TripReason::SENSOR_JUMP;      return true; }
    if (anomaly == SensorAnomaly::OUT_OF_RANGE)  { outReason = TripReason::SENSOR_OUT_OF_RANGE; return true; }
    if (cleanValue > TRIP_CURRENT_A)             { outReason = TripReason::OVER_THRESHOLD;      return true; }
    outReason = TripReason::NONE;
    return false;
}

bool SafetyRelay::conditionSafeForReset(float cleanValue, SensorAnomaly anomaly) const {
    // Hysteresis: must drop below the (lower) reset threshold, not merely
    // below the trip threshold, and sensors must be reporting cleanly.
    return anomaly == SensorAnomaly::NONE && cleanValue < RESET_CURRENT_A;
}

void SafetyRelay::transitionTo(RelayState newState, TripReason reason, uint32_t nowMs) {
    RelayLogEntry entry;
    entry.timestampMs = nowMs;
    entry.prevState    = state;
    entry.newState      = newState;
    entry.reason         = reason;

    logBuf[logHead] = entry;
    logHead = (logHead + 1) % LOG_CAPACITY;
    if (logCount < LOG_CAPACITY) logCount++;

    // Structured, timestamped log line — machine-parseable and human-readable.
    Serial.print(F("[RELAY] t="));       Serial.print(nowMs);
    Serial.print(F("ms  prev="));         Serial.print((int)state);
    Serial.print(F(" -> new="));          Serial.print((int)newState);
    Serial.print(F("  reason="));         Serial.println((int)reason);

    state = newState;
    lastReason = reason;
}

void SafetyRelay::update(float cleanValue, SensorAnomaly anomaly, uint32_t nowMs) {
    TripReason candidateReason;
    bool unsafe = conditionUnsafe(cleanValue, anomaly, candidateReason);

    switch (state) {

        case RelayState::NORMAL: {
            if (unsafe) {
                debounceStartMs = nowMs;
                transitionTo(RelayState::FAULT_DEBOUNCE, candidateReason, nowMs);
            }
            break;
        }

        case RelayState::FAULT_DEBOUNCE: {
            if (!unsafe) {
                // Condition disappeared before debounce completed — a glitch,
                // not a real fault. Go back to NORMAL without ever tripping
                // the relay. This is the anti-chatter behaviour.
                transitionTo(RelayState::NORMAL, TripReason::NONE, nowMs);
            } else if (nowMs - debounceStartMs >= FAULT_DEBOUNCE_MS) {
                setRelay(false); // OPEN the relay — confirmed real fault
                transitionTo(RelayState::TRIPPED, candidateReason, nowMs);
            }
            // else: still within debounce window, keep waiting
            break;
        }

        case RelayState::TRIPPED: {
            if (conditionSafeForReset(cleanValue, anomaly)) {
                recoveryHoldStartMs = nowMs;
                transitionTo(RelayState::RECOVERY_HOLD, TripReason::NONE, nowMs);
            }
            // Relay remains open the whole time we're in TRIPPED.
            break;
        }

        case RelayState::RECOVERY_HOLD: {
            if (unsafe) {
                // Went unsafe again during cool-down — treat as still faulted.
                transitionTo(RelayState::TRIPPED, candidateReason, nowMs);
            } else if (nowMs - recoveryHoldStartMs >= RECOVERY_HOLD_MS) {
                recoveryVerifyStartMs = nowMs;
                transitionTo(RelayState::RECOVERY_VERIFY, TripReason::NONE, nowMs);
            }
            break;
        }

        case RelayState::RECOVERY_VERIFY: {
            if (unsafe || !conditionSafeForReset(cleanValue, anomaly)) {
                // Any excursion during verification sends us back to TRIPPED —
                // we do not re-close the relay on a lucky momentary reading.
                transitionTo(RelayState::TRIPPED, unsafe ? candidateReason : TripReason::OVER_THRESHOLD, nowMs);
            } else if (nowMs - recoveryVerifyStartMs >= RECOVERY_VERIFY_MS) {
                setRelay(true); // verified stable — safe to re-close
                transitionTo(RelayState::NORMAL, TripReason::NONE, nowMs);
            }
            break;
        }
    }
}

RelayState SafetyRelay::getState()      const { return state; }
bool       SafetyRelay::isRelayClosed() const { return state == RelayState::NORMAL || state == RelayState::FAULT_DEBOUNCE; }
TripReason SafetyRelay::getLastReason() const { return lastReason; }

uint8_t SafetyRelay::getLogCount() const { return logCount; }

const RelayLogEntry& SafetyRelay::getLogEntry(uint8_t indexFromNewest) const {
    // indexFromNewest = 0 -> most recent entry
    uint8_t realIndex = (logHead + LOG_CAPACITY - 1 - indexFromNewest) % LOG_CAPACITY;
    return logBuf[realIndex];
}

#pragma once
// ============================================================================
// FaultManager.h
// Task 4: aggregates fault detectors into the single (source, id, active)
// picture that Fsm::tick() needs.
//
// Detectors owned here:
//   - Frozen ADC: per-channel, identical raw counts for ADC_FROZEN_MS
//   - Relay mismatch: commanded vs. feedback pin disagree for RELAY_MISMATCH_MS
//     (ignoring the RELAY_SETTLE_MS window right after a commanded change)
//   - Comm timeout: no valid message seen for COMM_TIMEOUT_MS
//
// External faults (from Task 1's BMS imbalance check, etc.) are reported in
// via reportExternalFault() rather than this class depending on those
// classes directly.
//
// Priority when multiple faults are active: CELL > RELAY > COMM > ADC.
// ============================================================================

#include <stdint.h>
#include "FsmConfig.h"
#include "SystemTypes.h"

class FaultManager {
public:
    FaultManager();

    void begin(uint8_t adcChannelCount);

    void updateAdcChannel(uint8_t channel, uint16_t rawCounts, uint32_t nowMs);
    void updateRelayFeedback(bool commanded, bool feedbackPin, uint32_t nowMs);
    void noteCommMessageReceived(uint32_t nowMs);
    void checkCommTimeout(uint32_t nowMs);

    void reportExternalFault(FaultSource src, uint16_t id, bool active);

    bool hasActiveFault(FaultSource& outSrc, uint16_t& outId) const;

private:
    struct AdcChannelState {
        uint16_t lastCounts;
        uint32_t unchangedSinceMs;
        bool     haveLast;
        bool     frozen;
    };
    AdcChannelState adcStates[MAX_ADC_CHANNELS];
    uint8_t adcChannelCount;
    uint8_t frozenChannel;

    bool     relayCommanded;
    bool     relayFeedback;
    uint32_t relayLastCommandChangeMs;
    uint32_t relayMismatchSinceMs;
    bool     relayMismatchTimerRunning;
    bool     relayFaultActive;

    uint32_t lastValidMsgMs;
    bool     haveReceivedAnyMsg;
    bool     commFaultActive;

    bool     cellFaultActive;
    uint16_t cellFaultId;
    bool     relayExtFaultActive;
    uint16_t relayExtFaultId;
};

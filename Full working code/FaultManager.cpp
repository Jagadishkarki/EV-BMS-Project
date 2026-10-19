// ============================================================================
// FaultManager.cpp
// ============================================================================

#include "FaultManager.h"
#include <Arduino.h>

FaultManager::FaultManager()
    : adcChannelCount(0), frozenChannel(0),
      relayCommanded(true), relayFeedback(true),
      relayLastCommandChangeMs(0), relayMismatchSinceMs(0),
      relayMismatchTimerRunning(false), relayFaultActive(false),
      lastValidMsgMs(0), haveReceivedAnyMsg(false), commFaultActive(false),
      cellFaultActive(false), cellFaultId(0),
      relayExtFaultActive(false), relayExtFaultId(0)
{
    for (uint8_t i = 0; i < MAX_ADC_CHANNELS; i++) adcStates[i] = { 0, 0, false, false };
}

void FaultManager::begin(uint8_t channelCount) {
    adcChannelCount = (channelCount > MAX_ADC_CHANNELS) ? MAX_ADC_CHANNELS : channelCount;
    for (uint8_t i = 0; i < MAX_ADC_CHANNELS; i++) adcStates[i] = { 0, 0, false, false };
}

void FaultManager::updateAdcChannel(uint8_t channel, uint16_t rawCounts, uint32_t nowMs) {
    if (channel >= adcChannelCount) return;
    AdcChannelState& s = adcStates[channel];

    if (!s.haveLast) {
        s.lastCounts = rawCounts;
        s.unchangedSinceMs = nowMs;
        s.haveLast = true;
        s.frozen = false;
        return;
    }

    uint16_t diff = (rawCounts > s.lastCounts) ? (rawCounts - s.lastCounts) : (s.lastCounts - rawCounts);
    if (diff <= ADC_FROZEN_TOLERANCE) {
        if (nowMs - s.unchangedSinceMs >= ADC_FROZEN_MS) {
            s.frozen = true;
            frozenChannel = channel;
        }
    } else {
        s.unchangedSinceMs = nowMs;
        s.frozen = false;
        s.lastCounts = rawCounts;
    }
}

void FaultManager::updateRelayFeedback(bool commanded, bool feedback, uint32_t nowMs) {
    if (commanded != relayCommanded) {
        relayCommanded = commanded;
        relayLastCommandChangeMs = nowMs;
        relayMismatchTimerRunning = false;
    }
    relayFeedback = feedback;

    bool withinSettle = (nowMs - relayLastCommandChangeMs) < RELAY_SETTLE_MS;
    if (withinSettle) {
        relayMismatchTimerRunning = false;
        relayFaultActive = false;
        return;
    }

    if (relayCommanded != relayFeedback) {
        if (!relayMismatchTimerRunning) {
            relayMismatchTimerRunning = true;
            relayMismatchSinceMs = nowMs;
        } else if (nowMs - relayMismatchSinceMs >= RELAY_MISMATCH_MS) {
            relayFaultActive = true;
        }
    } else {
        relayMismatchTimerRunning = false;
        relayFaultActive = false;
    }
}

void FaultManager::noteCommMessageReceived(uint32_t nowMs) {
    lastValidMsgMs = nowMs;
    haveReceivedAnyMsg = true;
    commFaultActive = false;
}

void FaultManager::checkCommTimeout(uint32_t nowMs) {
    if (!haveReceivedAnyMsg) return;
    if (nowMs - lastValidMsgMs >= COMM_TIMEOUT_MS) commFaultActive = true;
}

void FaultManager::reportExternalFault(FaultSource src, uint16_t id, bool active) {
    if (src == FaultSource::CELL) {
        cellFaultActive = active;
        cellFaultId = id;
    } else if (src == FaultSource::RELAY) {
        relayExtFaultActive = active;
        relayExtFaultId = id;
    }
}

bool FaultManager::hasActiveFault(FaultSource& outSrc, uint16_t& outId) const {
    if (cellFaultActive)      { outSrc = FaultSource::CELL;  outId = cellFaultId;        return true; }
    if (relayFaultActive)     { outSrc = FaultSource::RELAY; outId = 200;                 return true; }
    if (relayExtFaultActive)  { outSrc = FaultSource::RELAY; outId = relayExtFaultId;      return true; }
    if (commFaultActive)      { outSrc = FaultSource::COMM;  outId = 300;                   return true; }
    for (uint8_t i = 0; i < adcChannelCount; i++) {
        if (adcStates[i].frozen) { outSrc = FaultSource::ADC; outId = 100 + i; return true; }
    }
    outSrc = FaultSource::NONE;
    outId = 0;
    return false;
}

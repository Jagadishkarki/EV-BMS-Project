#pragma once
// ============================================================================
// TelemetryEngine.h
// Task 5: event-driven telemetry.
//
// - Compares each tick's snapshot against the last-SENT snapshot. An event
//   is generated only when something crosses a "meaningful change"
//   threshold (TelemetryConfig.h), or the heartbeat interval elapses.
// - If Wi-Fi + Blynk are both up, the event is pushed live immediately.
// - If not, it's pushed into OfflineQueue (FIFO, fixed capacity).
// - Once connectivity returns, queued events are drained in order, a few
//   per tick, before the engine resumes sending fresh live events — so the
//   dashboard replays exactly what happened while it was offline, in the
//   order it happened, rather than silently skipping to "now".
// - VPIN_DATA_SOURCE tells the operator whether what they're looking at is
//   a live reading or a replayed queued one (and how many are left).
//
// This class takes plain data (TelemetrySnapshot) rather than depending on
// BMSEngine/SafetyRelay/Fsm classes directly, matching the project's
// established "clean interface" pattern.
// ============================================================================

#include <stdint.h>
#include "TelemetryConfig.h"
#include "OfflineQueue.h"
#include "WifiManager.h"
#include "SystemTypes.h"

struct TelemetrySnapshot {
    float       packV;
    float       weakestV;
    float       strongestV;
    uint8_t     weakestIndex;
    uint8_t     strongestIndex;
    float       soc;
    float       currentA;
    uint8_t     cellCount;
    const float* cellVoltages; // pointer to caller's array, length = cellCount
    bool        relayClosed;
    OpState     state;
    FaultSource faultSource;
    uint16_t    faultId;
};

class TelemetryEngine {
public:
    TelemetryEngine();

    void begin();

    // Call every loop() with the live snapshot, the Wi-Fi manager, and
    // whether Blynk itself reports connected (Blynk.connected()). Handles
    // change detection, queueing, draining, and live pin writes internally.
    void update(const TelemetrySnapshot& snap, const WifiManager& wifi, bool blynkConnected, uint32_t nowMs);

    uint8_t  getQueueDepth()   const;
    uint32_t getDroppedCount() const;
    bool     isDrainingQueue() const;

private:
    OfflineQueue queue;

    bool     haveLastSent;
    TelemetrySnapshot lastSent; // note: cellVoltages pointer not reused, cached array below
    float    lastSentCells[TELEMETRY_MAX_CELLS];

    uint32_t lastHeartbeatMs;
    bool     draining;

    bool detectMeaningfulChange(const TelemetrySnapshot& snap, TelemetryEventType& outType) const;
    TelemetryEvent buildEvent(const TelemetrySnapshot& snap, TelemetryEventType type, uint32_t nowMs) const;
    void sendLive(const TelemetryEvent& ev, const WifiManager& wifi, const char* dataSourceLabel);
    void rememberAsLastSent(const TelemetrySnapshot& snap);
};

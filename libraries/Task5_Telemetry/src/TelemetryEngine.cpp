// ============================================================================
// TelemetryEngine.cpp
// ============================================================================

#include "TelemetryEngine.h"
#include "PinMap.h"
#include <Blynk/BlynkApi.h> // pulled in transitively by the main sketch's Blynk include
#include <stdio.h>
#include <math.h>

TelemetryEngine::TelemetryEngine()
    : haveLastSent(false), lastHeartbeatMs(0), draining(false) {}

void TelemetryEngine::begin() {
    queue.begin();
    haveLastSent = false;
    lastHeartbeatMs = 0;
    draining = false;
}

bool TelemetryEngine::detectMeaningfulChange(const TelemetrySnapshot& snap, TelemetryEventType& outType) const {
    if (!haveLastSent) { outType = TelemetryEventType::HEARTBEAT; return true; } // first-ever reading

    if (snap.state != lastSent.state)                 { outType = TelemetryEventType::STATE_CHANGE; return true; }
    if (snap.relayClosed != lastSent.relayClosed)      { outType = TelemetryEventType::RELAY_CHANGE; return true; }
    if (snap.faultSource != lastSent.faultSource ||
        snap.faultId     != lastSent.faultId)          { outType = TelemetryEventType::FAULT_CHANGE; return true; }

    if (fabsf(snap.soc - lastSent.soc) >= SOC_DELTA_THRESHOLD)            { outType = TelemetryEventType::CELL_DELTA; return true; }
    if (fabsf(snap.currentA - lastSent.currentA) >= CURRENT_DELTA_THRESHOLD_A) { outType = TelemetryEventType::CELL_DELTA; return true; }

    uint8_t n = (snap.cellCount < TELEMETRY_MAX_CELLS) ? snap.cellCount : TELEMETRY_MAX_CELLS;
    for (uint8_t i = 0; i < n; i++) {
        if (fabsf(snap.cellVoltages[i] - lastSentCells[i]) >= CELL_DELTA_THRESHOLD_V) {
            outType = TelemetryEventType::CELL_DELTA;
            return true;
        }
    }
    return false;
}

TelemetryEvent TelemetryEngine::buildEvent(const TelemetrySnapshot& snap, TelemetryEventType type, uint32_t nowMs) const {
    TelemetryEvent ev;
    ev.timestampMs = nowMs;
    ev.type = type;
    ev.packV = snap.packV;
    ev.weakestV = snap.weakestV;
    ev.strongestV = snap.strongestV;
    ev.weakestIndex = snap.weakestIndex;
    ev.strongestIndex = snap.strongestIndex;
    ev.soc = snap.soc;
    ev.currentA = snap.currentA;
    ev.cellCount = (snap.cellCount < TELEMETRY_MAX_CELLS) ? snap.cellCount : TELEMETRY_MAX_CELLS;
    for (uint8_t i = 0; i < ev.cellCount; i++) ev.cellVoltages[i] = snap.cellVoltages[i];
    ev.relayClosed = snap.relayClosed;
    ev.state = snap.state;
    ev.faultSource = snap.faultSource;
    ev.faultId = snap.faultId;
    return ev;
}

void TelemetryEngine::rememberAsLastSent(const TelemetrySnapshot& snap) {
    lastSent = snap;
    uint8_t n = (snap.cellCount < TELEMETRY_MAX_CELLS) ? snap.cellCount : TELEMETRY_MAX_CELLS;
    for (uint8_t i = 0; i < n; i++) lastSentCells[i] = snap.cellVoltages[i];
    haveLastSent = true;
}

void TelemetryEngine::sendLive(const TelemetryEvent& ev, const WifiManager& wifi, const char* dataSourceLabel) {
    // Pin ownership: Task 5 owns V0-V2 and V7, V14-V18 (live/queued data).
    // Task 6 owns the rest. Neither writes the other's pins, so a replayed
    // queued event is never overwritten by a fresh analytics push.
    Blynk.virtualWrite(VPIN_PACK_VOLTAGE, ev.packV);
    Blynk.virtualWrite(VPIN_WEAKEST_V,    ev.weakestV);
    Blynk.virtualWrite(VPIN_STRONGEST_V,  ev.strongestV);
    Blynk.virtualWrite(VPIN_RELAY_STATUS, ev.relayClosed ? "CLOSED" : "OPEN");

    char stateMsg[48];
    snprintf(stateMsg, sizeof(stateMsg), "%s (src=%s id=%u)", opStateName(ev.state), faultSourceName(ev.faultSource), (unsigned)ev.faultId);
    Blynk.virtualWrite(VPIN_STATE_LABEL, stateMsg);

    char wifiMsg[40];
    if (wifi.isConnected()) {
        snprintf(wifiMsg, sizeof(wifiMsg), "CONNECTED %ddBm", (int)wifi.getRssi());
    } else if (wifi.getState() == WifiState::RETRY_WAIT || wifi.getState() == WifiState::CONNECTING) {
        snprintf(wifiMsg, sizeof(wifiMsg), "RECONNECTING (retry %u)", (unsigned)wifi.getRetryCount());
    } else {
        snprintf(wifiMsg, sizeof(wifiMsg), "OFFLINE");
    }
    Blynk.virtualWrite(VPIN_WIFI_HEALTH, wifiMsg);

    Blynk.virtualWrite(VPIN_QUEUE_DEPTH, queue.count());
    Blynk.virtualWrite(VPIN_DATA_SOURCE, dataSourceLabel);

    // Scalable cell listing: one Terminal line regardless of NUM_CELLS,
    // rather than one virtual pin per cell (consistent with Task 1's
    // scaling design — memory/bandwidth cost grows with cell count only
    // in this one string, not in the pin count).
    char line[160];
    int pos = snprintf(line, sizeof(line), "[%lums] ", (unsigned long)ev.timestampMs);
    for (uint8_t i = 0; i < ev.cellCount && pos < (int)sizeof(line) - 8; i++) {
        pos += snprintf(line + pos, sizeof(line) - pos, "C%u:%.2f ", (unsigned)i, (double)ev.cellVoltages[i]);
    }
    Blynk.virtualWrite(VPIN_CELL_LIST, line);
}

void TelemetryEngine::update(const TelemetrySnapshot& snap, const WifiManager& wifi, bool blynkConnected, uint32_t nowMs) {
    bool connected = wifi.isConnected() && blynkConnected;

    // ---- Drain any backlog first, a few events per tick, oldest first -----
    if (connected && !queue.isEmpty()) {
        draining = true;
        for (uint8_t i = 0; i < TELEMETRY_DRAIN_PER_TICK && !queue.isEmpty(); i++) {
            TelemetryEvent ev;
            if (queue.pop(ev)) {
                char label[24];
                snprintf(label, sizeof(label), "QUEUED(%u)", (unsigned)queue.count());
                sendLive(ev, wifi, label);
            }
        }
        if (queue.isEmpty()) draining = false;
        return; // finish draining before evaluating a fresh event this tick
    }
    draining = false;

    // ---- Decide if this tick is event-worthy -------------------------------
    TelemetryEventType type;
    bool heartbeatDue = (nowMs - lastHeartbeatMs) >= TELEMETRY_HEARTBEAT_MS;
    bool changed = detectMeaningfulChange(snap, type);

    if (!changed && !heartbeatDue) return; // nothing worth sending this tick

    if (!changed && heartbeatDue) type = TelemetryEventType::HEARTBEAT;
    lastHeartbeatMs = nowMs;

    TelemetryEvent ev = buildEvent(snap, type, nowMs);
    rememberAsLastSent(snap);

    if (connected) {
        sendLive(ev, wifi, "LIVE");
    } else {
        queue.push(ev); // FIFO; oldest dropped automatically if full
    }
}

uint8_t  TelemetryEngine::getQueueDepth()   const { return queue.count(); }
uint32_t TelemetryEngine::getDroppedCount() const { return queue.droppedCount(); }
bool     TelemetryEngine::isDrainingQueue() const { return draining; }

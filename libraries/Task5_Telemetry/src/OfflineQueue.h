#pragma once
// ============================================================================
// OfflineQueue.h
// Task 5: fixed-capacity FIFO queue of telemetry events, used while
// Wi-Fi/Blynk is unavailable. No dynamic allocation. Events are replayed in
// the order they were enqueued once the connection is restored.
// ============================================================================

#include <stdint.h>
#include "TelemetryConfig.h"
#include "SystemTypes.h" // FaultSource, OpState

enum class TelemetryEventType : uint8_t {
    HEARTBEAT = 0,
    CELL_DELTA,
    STATE_CHANGE,
    RELAY_CHANGE,
    FAULT_CHANGE
};

struct TelemetryEvent {
    uint32_t            timestampMs;
    TelemetryEventType  type;

    float    packV;
    float    weakestV;
    float    strongestV;
    uint8_t  weakestIndex;
    uint8_t  strongestIndex;
    float    soc;
    float    currentA;

    uint8_t  cellCount;
    float    cellVoltages[TELEMETRY_MAX_CELLS];

    bool        relayClosed;
    OpState     state;
    FaultSource faultSource;
    uint16_t    faultId;
};

class OfflineQueue {
public:
    OfflineQueue();

    void begin();

    // Returns false (and drops the oldest entry) if the queue was already
    // full — this is the documented overflow policy: prefer keeping the
    // newest data over growing unbounded or blocking.
    bool push(const TelemetryEvent& ev);

    bool pop(TelemetryEvent& out); // FIFO order
    bool isEmpty() const;
    bool isFull()  const;
    uint8_t count() const;
    uint32_t droppedCount() const;

private:
    TelemetryEvent buf[TELEMETRY_QUEUE_CAPACITY];
    uint8_t head; // next pop position
    uint8_t tail; // next push position
    uint8_t size;
    uint32_t dropped;
};

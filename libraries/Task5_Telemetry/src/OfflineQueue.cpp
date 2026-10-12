// ============================================================================
// OfflineQueue.cpp
// ============================================================================

#include "OfflineQueue.h"

OfflineQueue::OfflineQueue() : head(0), tail(0), size(0), dropped(0) {}

void OfflineQueue::begin() {
    head = 0; tail = 0; size = 0; dropped = 0;
}

bool OfflineQueue::push(const TelemetryEvent& ev) {
    if (size == TELEMETRY_QUEUE_CAPACITY) {
        // Drop the oldest to make room for the newest reading.
        head = (head + 1) % TELEMETRY_QUEUE_CAPACITY;
        size--;
        dropped++;
        buf[tail] = ev;
        tail = (tail + 1) % TELEMETRY_QUEUE_CAPACITY;
        size++;
        return false;
    }
    buf[tail] = ev;
    tail = (tail + 1) % TELEMETRY_QUEUE_CAPACITY;
    size++;
    return true;
}

bool OfflineQueue::pop(TelemetryEvent& out) {
    if (size == 0) return false;
    out = buf[head];
    head = (head + 1) % TELEMETRY_QUEUE_CAPACITY;
    size--;
    return true;
}

bool    OfflineQueue::isEmpty()      const { return size == 0; }
bool    OfflineQueue::isFull()       const { return size == TELEMETRY_QUEUE_CAPACITY; }
uint8_t OfflineQueue::count()        const { return size; }
uint32_t OfflineQueue::droppedCount() const { return dropped; }

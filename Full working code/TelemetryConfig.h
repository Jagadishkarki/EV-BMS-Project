#pragma once
// ============================================================================
// TelemetryConfig.h
// Task 5: Event-Driven Telemetry and Live Blynk Dashboard
// ============================================================================

#include <stdint.h>

// ---- "Meaningful change" thresholds -----------------------------------------
// An event is sent only when something crosses one of these, not on a fixed
// timer. Tighten/loosen to trade dashboard freshness against traffic.
constexpr float    CELL_DELTA_THRESHOLD_V = 0.010f; // any single cell moving this much triggers an event
constexpr float    SOC_DELTA_THRESHOLD    = 0.01f;   // 1% SoC change
constexpr float    CURRENT_DELTA_THRESHOLD_A = 0.5f;

// A heartbeat event is still sent on this cadence even with zero change, so
// the dashboard never looks dead/stale during a long quiet period.
constexpr uint32_t TELEMETRY_HEARTBEAT_MS = 30000;

// ---- Offline queue -----------------------------------------------------------
// Fixed capacity, no dynamic allocation. If it fills while offline, the
// OLDEST event is dropped to make room for the newest (documented policy,
// see Task 5 report) rather than growing unbounded or blocking.
constexpr uint8_t TELEMETRY_QUEUE_CAPACITY = 24;

// Cap how many queued events are drained to Blynk per loop() pass, so a
// large backlog doesn't flood the connection or block the loop.
constexpr uint8_t TELEMETRY_DRAIN_PER_TICK = 2;

// Max cells a TelemetryEvent snapshot can carry (independent of BMSConfig's
// NUM_CELLS so this library has no compile dependency on Task 1 — pass the
// actual cellCount at runtime, capped to this).
constexpr uint8_t TELEMETRY_MAX_CELLS = 16;

// ---- Wi-Fi reconnect state machine --------------------------------------------
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000; // give one connect attempt this long
constexpr uint32_t WIFI_RETRY_BASE_MS      = 2000;  // first retry wait
constexpr uint32_t WIFI_RETRY_MAX_MS       = 30000; // retry backoff caps here
constexpr uint32_t WIFI_RSSI_POLL_MS       = 5000;  // how often to sample RSSI while connected

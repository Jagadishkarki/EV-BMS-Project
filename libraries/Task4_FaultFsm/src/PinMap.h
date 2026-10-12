#pragma once
// ============================================================================
// PinMap.h
// Single source of truth for every Blynk virtual pin used anywhere in the
// project, so Task 5 (live telemetry) and Task 6 (analytics) never collide.
// Lives here because both Task 5 and Task 6 already depend on Task 4.
// ============================================================================

#include <stdint.h>

// ---- Task 6: analytics / historical trends -----------------------------------
constexpr uint8_t VPIN_PACK_VOLTAGE   = 0;
constexpr uint8_t VPIN_WEAKEST_V      = 1;
constexpr uint8_t VPIN_STRONGEST_V    = 2;
constexpr uint8_t VPIN_IMBALANCE_MV   = 3;
constexpr uint8_t VPIN_SOC_PCT        = 4;
constexpr uint8_t VPIN_CURRENT_A      = 5;
constexpr uint8_t VPIN_RISK_SCORE     = 6;
constexpr uint8_t VPIN_STATE_LABEL    = 7;
constexpr uint8_t VPIN_FAULT_COUNT    = 8;
constexpr uint8_t VPIN_UPTIME         = 9;
constexpr uint8_t VPIN_MAINTENANCE    = 10;
constexpr uint8_t VPIN_SEVERITY_LED   = 11;
constexpr uint8_t VPIN_FAULT_HISTORY  = 12; // Terminal: FSM transition replay
constexpr uint8_t VPIN_EXEC_SUMMARY   = 13;

// ---- Task 5: live/event-driven telemetry ---------------------------------------
constexpr uint8_t VPIN_RELAY_STATUS   = 14; // "CLOSED" / "OPEN"
constexpr uint8_t VPIN_WIFI_HEALTH    = 15; // "CONNECTED -54dBm" / "RECONNECTING" / "OFFLINE"
constexpr uint8_t VPIN_QUEUE_DEPTH    = 16; // offline queue entry count
constexpr uint8_t VPIN_DATA_SOURCE    = 17; // "LIVE" or "QUEUED(n)" while draining backlog
constexpr uint8_t VPIN_CELL_LIST      = 18; // Terminal: all N cell voltages, one line

// V19 reserved for future use.

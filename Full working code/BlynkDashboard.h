#pragma once
// ============================================================================
// BlynkDashboard.h
// Task 6: analytics dashboard. Pushes risk score, severity color, executive
// summary, maintenance suggestion, imbalance/SoC/current trends, and replays
// every Fsm transition to a Terminal widget exactly once.
//
// Pin ownership (see PinMap.h): this class writes V3-V6, V8-V13. It does NOT
// write the live-telemetry pins owned by Task 5 (V0-V2, V7, V14-V18).
// ============================================================================

#include <stdint.h>
#include "RiskEngine.h"
#include "Fsm.h" // Fsm log access (Task4_FaultFsm, declared dependency)

struct DashboardData {
    float        imbalanceV;
    float        adaptiveThresholdV;
    int8_t       imbalanceTrend;
    float        soc;
    float        currentA;
    OpState      state;
    FaultSource  faultSource;
    uint16_t     totalFaultCount;
    uint16_t     faultsInLastWindow;
    uint32_t     uptimeSeconds;
};

class BlynkDashboard {
public:
    BlynkDashboard();
    void begin();
    void update(const DashboardData& data, const Fsm& fsm, uint32_t nowMs);

private:
    RiskEngine riskEngine;
    uint32_t   lastPushMs;
    uint8_t    lastSentLogTotal; // total transitions ever logged that we've already replayed
    uint32_t   totalTransitionsSeen;

    const char* severityColor(Severity s) const;
    const char* severityIcon(Severity s) const;
    void pushValues(const DashboardData& d, const RiskResult& risk);
    void pushSeverity(const DashboardData& d, const RiskResult& risk);
    void pushSummary(const DashboardData& d, const RiskResult& risk);
    void replayFaultHistory(const Fsm& fsm);
};

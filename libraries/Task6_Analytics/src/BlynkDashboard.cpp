// ============================================================================
// BlynkDashboard.cpp
// ============================================================================

#include "BlynkDashboard.h"
#include "PinMap.h"
#include <Blynk/BlynkApi.h>
#include <stdio.h>

constexpr uint32_t DASHBOARD_PUSH_MS = 2000;

BlynkDashboard::BlynkDashboard()
    : lastPushMs(0), lastSentLogTotal(0), totalTransitionsSeen(0) {}

void BlynkDashboard::begin() {
    lastPushMs = 0;
    lastSentLogTotal = 0;
    totalTransitionsSeen = 0;
}

// Colors/icons deliberately track the Task 4 operating states:
//   NORMAL -> green, DEGRADED -> orange, FAILSAFE -> red, SHUTDOWN -> dark red.
const char* BlynkDashboard::severityColor(Severity s) const {
    switch (s) {
        case Severity::OK:       return "#23C48E";
        case Severity::WATCH:    return "#F7C948";
        case Severity::WARNING:  return "#F2994A";
        case Severity::CRITICAL: return "#EB5757";
        default:                 return "#A0A0A0";
    }
}

const char* BlynkDashboard::severityIcon(Severity s) const {
    switch (s) {
        case Severity::OK:       return "OK";
        case Severity::WATCH:    return "WATCH";
        case Severity::WARNING:  return "WARN";
        case Severity::CRITICAL: return "CRIT";
        default:                 return "?";
    }
}

void BlynkDashboard::pushValues(const DashboardData& d, const RiskResult& risk) {
    Blynk.virtualWrite(VPIN_IMBALANCE_MV, d.imbalanceV * 1000.0f);
    Blynk.virtualWrite(VPIN_SOC_PCT,      d.soc * 100.0f);
    Blynk.virtualWrite(VPIN_CURRENT_A,    d.currentA);
    Blynk.virtualWrite(VPIN_RISK_SCORE,   risk.score);
    Blynk.virtualWrite(VPIN_FAULT_COUNT,  d.totalFaultCount);

    unsigned long s = d.uptimeSeconds;
    char up[12];
    snprintf(up, sizeof(up), "%02lu:%02lu:%02lu", s / 3600UL, (s % 3600UL) / 60UL, s % 60UL);
    Blynk.virtualWrite(VPIN_UPTIME, up);
    Blynk.virtualWrite(VPIN_MAINTENANCE, risk.suggestion);
}

void BlynkDashboard::pushSeverity(const DashboardData& d, const RiskResult& risk) {
    (void)d;
    Blynk.setProperty(VPIN_SEVERITY_LED, "color", severityColor(risk.severity));
    Blynk.setProperty(VPIN_SEVERITY_LED, "label", severityIcon(risk.severity));
    Blynk.virtualWrite(VPIN_SEVERITY_LED, risk.severity == Severity::OK ? 0 : 255);
}

void BlynkDashboard::pushSummary(const DashboardData& d, const RiskResult& risk) {
    uint8_t health = 100 - risk.score;
    unsigned long s = d.uptimeSeconds;
    char summary[128];
    snprintf(summary, sizeof(summary),
             "Health: %u%% | State: %s | Faults: %u | Uptime: %02lu:%02lu:%02lu",
             (unsigned)health, opStateName(d.state), (unsigned)d.totalFaultCount,
             s / 3600UL, (s % 3600UL) / 60UL, s % 60UL);
    Blynk.virtualWrite(VPIN_EXEC_SUMMARY, summary);
}

// Replays only log entries not yet sent. Uses the FSM's ring buffer count;
// because the buffer holds FSM_LOG_CAPACITY entries and this runs every
// loop() pass, it keeps up with transitions long before the buffer wraps.
void BlynkDashboard::replayFaultHistory(const Fsm& fsm) {
    uint8_t buffered = fsm.getLogCount();
    if (buffered == 0) return;

    // newest entry is index 0. Count how many of the buffered entries are
    // newer than what we already sent by tracking a running total.
    // Simplest robust approach: compare buffered count growth, handle full-buffer
    // case by comparing newest entry timestamp+state to the last we sent.
    static uint32_t lastTs = 0;
    static bool     haveLast = false;

    // Walk from oldest to newest among buffered entries and send those
    // newer than lastTs (or all, the first time).
    for (int16_t i = (int16_t)buffered - 1; i >= 0; i--) {
        const FsmLogEntry& e = fsm.getLogEntry((uint8_t)i);
        if (haveLast && e.timestampMs <= lastTs) continue;
        char line[80];
        snprintf(line, sizeof(line), "[%lums] %s -> %s (src=%s id=%u)",
                 (unsigned long)e.timestampMs,
                 opStateName(e.prevState), opStateName(e.newState),
                 faultSourceName(e.faultSource), (unsigned)e.faultId);
        Blynk.virtualWrite(VPIN_FAULT_HISTORY, line);
        lastTs = e.timestampMs;
        haveLast = true;
        totalTransitionsSeen++;
    }
}

void BlynkDashboard::update(const DashboardData& d, const Fsm& fsm, uint32_t nowMs) {
    replayFaultHistory(fsm); // every call: no transition waits for the push timer

    if (nowMs - lastPushMs < DASHBOARD_PUSH_MS) return;
    lastPushMs = nowMs;

    RiskInputs in;
    in.imbalanceV = d.imbalanceV;
    in.adaptiveThresholdV = d.adaptiveThresholdV;
    in.imbalanceTrend = d.imbalanceTrend;
    in.soc = d.soc;
    in.faultsInLastWindow = d.faultsInLastWindow;
    in.state = d.state;
    in.faultSource = d.faultSource;

    RiskResult risk = riskEngine.evaluate(in);
    pushValues(d, risk);
    pushSeverity(d, risk);
    pushSummary(d, risk);
}

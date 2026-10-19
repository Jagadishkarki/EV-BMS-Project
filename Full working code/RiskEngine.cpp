// ============================================================================
// RiskEngine.cpp
// ============================================================================

#include "RiskEngine.h"
#include <math.h>
#include <stdio.h>

RiskEngine::RiskEngine() {}

float RiskEngine::imbalanceFactor(const RiskInputs& in) const {
    float ratio = (in.adaptiveThresholdV > 0.0f) ? (in.imbalanceV / in.adaptiveThresholdV) : 0.0f;
    if (ratio > 1.5f) ratio = 1.5f;
    float factor = ratio / 1.5f;
    if (in.imbalanceTrend > 0) factor += 0.15f;
    if (factor > 1.0f) factor = 1.0f;
    if (factor < 0.0f) factor = 0.0f;
    return factor;
}

float RiskEngine::faultFrequencyFactor(const RiskInputs& in) const {
    const uint16_t CAP = 5;
    float factor = (float)in.faultsInLastWindow / (float)CAP;
    if (factor > 1.0f) factor = 1.0f;
    return factor;
}

float RiskEngine::socFactor(const RiskInputs& in) const {
    float distanceFromMid = fabsf(in.soc - 0.50f);
    float factor = distanceFromMid / 0.50f;
    if (factor > 1.0f) factor = 1.0f;
    if (factor < 0.0f) factor = 0.0f;
    return factor;
}

Severity RiskEngine::scoreToSeverity(uint8_t score, OpState state) const {
    // FSM state is authoritative for anything already abnormal.
    if (state == OpState::SHUTDOWN) return Severity::CRITICAL;
    if (state == OpState::FAILSAFE) return Severity::CRITICAL;
    if (state == OpState::DEGRADED) return Severity::WARNING;
    if (score >= 70) return Severity::CRITICAL;
    if (score >= 40) return Severity::WARNING;
    if (score >= 15) return Severity::WATCH;
    return Severity::OK;
}

void RiskEngine::buildSuggestion(const RiskInputs& in, uint8_t score, Severity sev, char* out, uint8_t outLen) const {
    if (in.state == OpState::SHUTDOWN) {
        snprintf(out, outLen, "SYSTEM SHUTDOWN: automatic recovery exhausted. Manual inspection and reset required.");
        return;
    }
    if (sev == Severity::CRITICAL) {
        if (in.faultSource == FaultSource::CELL) {
            snprintf(out, outLen, "Stop operation: cell fault active. Inspect weak cell and balancing circuit before restart.");
        } else if (in.faultSource == FaultSource::RELAY) {
            snprintf(out, outLen, "Stop operation: relay fault. Check contactor wiring and feedback line.");
        } else if (in.faultSource == FaultSource::ADC) {
            snprintf(out, outLen, "Stop operation: sensor fault (frozen ADC). Check sensor wiring/connector.");
        } else if (in.faultSource == FaultSource::COMM) {
            snprintf(out, outLen, "Stop operation: communication lost. Check bus wiring and controller power.");
        } else {
            snprintf(out, outLen, "System in FAILSAFE. Hold operation until verified recovery completes.");
        }
    } else if (sev == Severity::WARNING) {
        if (in.imbalanceTrend > 0) {
            snprintf(out, outLen, "Cell imbalance widening (%.0f%% of limit). Schedule a balancing cycle soon.",
                     (double)(in.adaptiveThresholdV > 0 ? (in.imbalanceV / in.adaptiveThresholdV * 100.0f) : 0.0f));
        } else if (in.faultsInLastWindow >= 2) {
            snprintf(out, outLen, "Repeated faults in recent operation (%u). Inspect the flagged subsystem.",
                     (unsigned)in.faultsInLastWindow);
        } else {
            snprintf(out, outLen, "Operating near safe limits. Monitor closely, no action required yet.");
        }
    } else if (sev == Severity::WATCH) {
        if (in.soc > 0.90f) {
            snprintf(out, outLen, "SoC high (%.0f%%). Avoid prolonged full-charge dwell to protect cell life.", (double)(in.soc * 100.0f));
        } else if (in.soc < 0.15f) {
            snprintf(out, outLen, "SoC low (%.0f%%). Recharge soon to avoid deep-discharge stress.", (double)(in.soc * 100.0f));
        } else {
            snprintf(out, outLen, "Minor imbalance trend detected. No action needed, continue monitoring.");
        }
    } else {
        snprintf(out, outLen, "Pack healthy. No maintenance action required.");
    }
}

RiskResult RiskEngine::evaluate(const RiskInputs& in) const {
    float fImb  = imbalanceFactor(in);
    float fFreq = faultFrequencyFactor(in);
    float fSoc  = socFactor(in);

    const float W_IMBALANCE = 0.5f;
    const float W_FREQUENCY = 0.3f;
    const float W_SOC       = 0.2f;

    float raw = (fImb * W_IMBALANCE) + (fFreq * W_FREQUENCY) + (fSoc * W_SOC);
    uint8_t score = (uint8_t)(raw * 100.0f + 0.5f);
    if (score > 100) score = 100;

    RiskResult result;
    result.score = score;
    result.severity = scoreToSeverity(score, in.state);
    buildSuggestion(in, score, result.severity, result.suggestion, sizeof(result.suggestion));
    return result;
}

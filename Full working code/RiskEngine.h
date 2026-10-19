#pragma once
// ============================================================================
// RiskEngine.h
// Task 6: composite 0-100 risk score + human-readable maintenance suggestion.
// Pure data in / data out, no Blynk dependency.
// ============================================================================

#include <stdint.h>
#include "SystemTypes.h" // OpState, FaultSource (from Task4_FaultFsm, declared dependency)

struct RiskInputs {
    float        imbalanceV;
    float        adaptiveThresholdV;
    int8_t       imbalanceTrend;      // -1/0/+1
    float        soc;                 // 0.0-1.0
    uint16_t     faultsInLastWindow;
    OpState      state;
    FaultSource  faultSource;
};

enum class Severity : uint8_t { OK = 0, WATCH, WARNING, CRITICAL };

struct RiskResult {
    uint8_t  score;
    Severity severity;
    char     suggestion[96];
};

class RiskEngine {
public:
    RiskEngine();
    RiskResult evaluate(const RiskInputs& in) const;

private:
    float imbalanceFactor(const RiskInputs& in) const;
    float faultFrequencyFactor(const RiskInputs& in) const;
    float socFactor(const RiskInputs& in) const;
    Severity scoreToSeverity(uint8_t score, OpState state) const;
    void buildSuggestion(const RiskInputs& in, uint8_t score, Severity sev, char* out, uint8_t outLen) const;
};

// ============================================================================
// BMSEngine.cpp
// Implementation of Task 1's Modular Battery Management Engine.
// ============================================================================

#include "BMSEngine.h"
#include <math.h>

BMSEngine::BMSEngine()
    : historyHead(0), historyCount(0)
{
    status.weakestIndex   = 0;
    status.strongestIndex = 0;
    status.weakestV       = 0.0f;
    status.strongestV     = 0.0f;
    status.packVoltage    = 0.0f;
    status.averageV       = 0.0f;
    status.stateOfCharge  = 0.0f;
    status.dischargeRateA = 0.0f;
    status.lastUpdateMs   = 0;
    status.valid          = false;

    status.imbalance.imbalanceV        = 0.0f;
    status.imbalance.adaptiveThreshold = BASE_IMBALANCE_V;
    status.imbalance.overThreshold     = false;
    status.imbalance.trend             = 0;

    for (uint8_t i = 0; i < IMBALANCE_HISTORY_LEN; i++) {
        imbalanceHistory[i] = 0.0f;
    }
}

void BMSEngine::begin() {
    historyHead  = 0;
    historyCount = 0;
    status.valid = false;
}

// ---------------------------------------------------------------------------
// scanCells: single O(NUM_CELLS) pass to find weakest/strongest/pack totals.
// Works unchanged whether NUM_CELLS is 4 or 16 — only the loop bound changes.
// ---------------------------------------------------------------------------
void BMSEngine::scanCells(const float cellVoltages[NUM_CELLS]) {
    float minV = cellVoltages[0];
    float maxV = cellVoltages[0];
    uint8_t minIdx = 0;
    uint8_t maxIdx = 0;
    float sum = 0.0f;

    for (uint8_t i = 0; i < NUM_CELLS; i++) {
        float v = cellVoltages[i];
        status.cells[i].voltage = v;
        status.cells[i].index   = i;
        sum += v;

        if (v < minV) { minV = v; minIdx = i; }
        if (v > maxV) { maxV = v; maxIdx = i; }
    }

    status.weakestIndex   = minIdx;
    status.strongestIndex = maxIdx;
    status.weakestV       = minV;
    status.strongestV     = maxV;
    status.packVoltage    = sum;
    status.averageV       = sum / (float)NUM_CELLS;

    status.imbalance.imbalanceV = maxV - minV;
}

// ---------------------------------------------------------------------------
// computeAdaptiveThreshold:
// Instead of a fixed "trip at 50mV" rule, the allowed imbalance window
// breathes with pack condition:
//   - Near full charge or near empty, cells are most sensitive to damage
//     from imbalance, so the threshold TIGHTENS (socFactor < 1).
//   - Under heavy discharge current, IR-drop differences between cells
//     legitimately widen the *measured* imbalance without indicating a
//     real capacity mismatch, so the threshold RELAXES (currentFactor > 1)
//     to avoid false trips during genuine load transients.
// The two factors are combined and clamped to a sane absolute range.
// ---------------------------------------------------------------------------
void BMSEngine::computeAdaptiveThreshold() {
    float soc = status.stateOfCharge;

    float socFactor = 1.0f;
    if (soc > 0.90f) {
        // Tighten linearly from 1.0 at 90% SoC down to 0.7 at 100% SoC
        socFactor = 1.0f - (soc - 0.90f) * 3.0f;
    } else if (soc < 0.15f) {
        // Tighten linearly from 1.0 at 15% SoC down to 0.7 at 0% SoC
        socFactor = 1.0f - (0.15f - soc) * 2.0f;
    }
    if (socFactor < 0.6f) socFactor = 0.6f;
    if (socFactor > 1.0f) socFactor = 1.0f;

    float loadRatio = fabsf(status.dischargeRateA) / RATED_CURRENT_A;
    if (loadRatio > 1.0f) loadRatio = 1.0f;
    float currentFactor = 1.0f + (loadRatio * 0.5f); // up to +50% relaxation at rated current

    float threshold = BASE_IMBALANCE_V * socFactor * currentFactor;

    if (threshold < MIN_IMBALANCE_V) threshold = MIN_IMBALANCE_V;
    if (threshold > MAX_IMBALANCE_V) threshold = MAX_IMBALANCE_V;

    status.imbalance.adaptiveThreshold = threshold;
    status.imbalance.overThreshold     = status.imbalance.imbalanceV > threshold;
}

// ---------------------------------------------------------------------------
// pushHistoryAndComputeTrend:
// Fixed-size ring buffer (size independent of NUM_CELLS, so memory cost of
// trend tracking does not grow when the pack scales from 4S to 16S).
// Trend is classified by comparing the mean of the newer half of the buffer
// against the mean of the older half — cheap, noise-tolerant, O(1) extra
// state beyond the buffer itself.
// ---------------------------------------------------------------------------
void BMSEngine::pushHistoryAndComputeTrend() {
    imbalanceHistory[historyHead] = status.imbalance.imbalanceV;
    historyHead = (historyHead + 1) % IMBALANCE_HISTORY_LEN;
    if (historyCount < IMBALANCE_HISTORY_LEN) historyCount++;

    if (historyCount < IMBALANCE_HISTORY_LEN) {
        status.imbalance.trend = 0; // not enough data yet
        return;
    }

    float oldHalfSum = 0.0f, newHalfSum = 0.0f;
    const uint8_t half = IMBALANCE_HISTORY_LEN / 2;

    for (uint8_t i = 0; i < half; i++) {
        uint8_t oldIdx = (historyHead + i) % IMBALANCE_HISTORY_LEN;                 // oldest half
        uint8_t newIdx = (historyHead + half + i) % IMBALANCE_HISTORY_LEN;          // newest half
        oldHalfSum += imbalanceHistory[oldIdx];
        newHalfSum += imbalanceHistory[newIdx];
    }

    float oldAvg = oldHalfSum / half;
    float newAvg = newHalfSum / half;
    float delta  = newAvg - oldAvg;

    const float NOISE_FLOOR_V = 0.003f; // 3 mV — ignore sub-noise wobble
    if (delta > NOISE_FLOOR_V)      status.imbalance.trend = 1;   // widening
    else if (delta < -NOISE_FLOOR_V) status.imbalance.trend = -1; // narrowing
    else                              status.imbalance.trend = 0; // stable
}

void BMSEngine::update(const float cellVoltages[NUM_CELLS], float soc, float dischargeRateA, uint32_t nowMs) {
    status.stateOfCharge  = soc;
    status.dischargeRateA = dischargeRateA;
    status.lastUpdateMs   = nowMs;

    scanCells(cellVoltages);
    computeAdaptiveThreshold();
    pushHistoryAndComputeTrend();

    status.valid = true;
}

// ---- Interface -------------------------------------------------------
const PackStatus& BMSEngine::getStatus()       const { return status; }
uint8_t BMSEngine::getWeakestIndex()           const { return status.weakestIndex; }
uint8_t BMSEngine::getStrongestIndex()         const { return status.strongestIndex; }
float   BMSEngine::getWeakestVoltage()         const { return status.weakestV; }
float   BMSEngine::getStrongestVoltage()       const { return status.strongestV; }
float   BMSEngine::getImbalance()              const { return status.imbalance.imbalanceV; }
float   BMSEngine::getAdaptiveThreshold()      const { return status.imbalance.adaptiveThreshold; }
int8_t  BMSEngine::getImbalanceTrend()         const { return status.imbalance.trend; }
bool    BMSEngine::isImbalanceCritical()       const { return status.imbalance.overThreshold; }

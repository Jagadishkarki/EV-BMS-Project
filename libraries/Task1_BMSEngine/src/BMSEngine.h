#pragma once
// ============================================================================
// BMSEngine.h
// Task 1: Modular Battery Management Engine
//
// Responsibilities:
//   - Scan an array of NUM_CELLS voltages every update() call
//   - Track weakest / strongest cell and the resulting imbalance
//   - Maintain a short rolling history to classify the imbalance trend
//   - Compute an adaptive imbalance threshold from SoC + discharge rate
//   - Expose a single read-only PackStatus struct so Tasks 2-5 can pull
//     everything they need without re-implementing any of this logic.
//
// Design notes:
//   - No dynamic allocation anywhere (embedded-safe).
//   - All arrays are sized from NUM_CELLS in BMSConfig.h, so the whole
//     engine rescales by editing one constant.
// ============================================================================

#include <stdint.h>
#include "BMSConfig.h"

// A single cell's latest reading.
struct CellReading {
    float   voltage;   // volts
    uint8_t index;     // position in the pack, 0..NUM_CELLS-1
};

// Everything related to the imbalance calculation in one place.
struct ImbalanceSnapshot {
    float   imbalanceV;        // strongest - weakest, volts
    float   adaptiveThreshold; // current allowed imbalance limit, volts
    bool    overThreshold;     // imbalanceV > adaptiveThreshold
    int8_t  trend;             // -1 = narrowing, 0 = stable, +1 = widening
};

// The full, exposed snapshot of pack state. This is the "clean interface"
// other tasks (relay logic, LCD, fault manager, Blynk dashboard) read from.
struct PackStatus {
    CellReading         cells[NUM_CELLS];
    uint8_t              weakestIndex;
    uint8_t              strongestIndex;
    float                weakestV;
    float                strongestV;
    float                packVoltage;   // sum of all cells
    float                averageV;      // packVoltage / NUM_CELLS
    float                stateOfCharge; // 0.0 - 1.0, supplied by caller
    float                dischargeRateA;// amps, supplied by caller (+ = discharging)
    ImbalanceSnapshot     imbalance;
    uint32_t              lastUpdateMs;
    bool                  valid;         // false until first update() call
};

class BMSEngine {
public:
    BMSEngine();

    // Call once in setup().
    void begin();

    // Call every loop() with fresh readings. Cheap enough for tight loops:
    // O(NUM_CELLS) per call, no allocation, no blocking.
    //   cellVoltages : array of NUM_CELLS raw voltage readings
    //   soc          : 0.0-1.0 state of charge estimate (from Coulomb count / OCV table)
    //   dischargeRateA: signed pack current, positive while discharging
    //   nowMs        : millis() timestamp, injected for testability
    void update(const float cellVoltages[NUM_CELLS], float soc, float dischargeRateA, uint32_t nowMs);

    // ---- Clean read-only interface for other modules -----------------
    const PackStatus& getStatus() const;
    uint8_t  getWeakestIndex()      const;
    uint8_t  getStrongestIndex()    const;
    float    getWeakestVoltage()    const;
    float    getStrongestVoltage()  const;
    float    getImbalance()         const;
    float    getAdaptiveThreshold() const;
    int8_t   getImbalanceTrend()    const; // -1/0/+1
    bool     isImbalanceCritical()  const; // imbalance > adaptive threshold

private:
    PackStatus status;

    float   imbalanceHistory[IMBALANCE_HISTORY_LEN];
    uint8_t historyHead;   // next write position (ring buffer)
    uint8_t historyCount;  // number of valid samples so far

    void scanCells(const float cellVoltages[NUM_CELLS]);
    void computeAdaptiveThreshold();
    void pushHistoryAndComputeTrend();
};

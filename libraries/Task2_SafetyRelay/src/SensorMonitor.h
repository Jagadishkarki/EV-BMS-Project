#pragma once
// ============================================================================
// SensorMonitor.h
// Per-channel sensor watchdog. Wraps one raw analog/digital feed and:
//   - rejects impossible readings (out-of-range)
//   - detects a frozen sensor (unchanged output for too long)
//   - detects a physically unrealistic single-sample jump (hardware fault)
//   - separately, uses a rolling window (mean/std-dev) to tell a genuine
//     fast load change apart from a single noisy outlier sample, so noise
//     doesn't get treated as a value the rest of the system reacts to
// ============================================================================

#include <stdint.h>
#include "RelaySafetyConfig.h"

enum class SensorAnomaly : uint8_t {
    NONE = 0,
    FROZEN,
    UNREALISTIC_JUMP,
    OUT_OF_RANGE
};

class SensorMonitor {
public:
    SensorMonitor();

    // Feed one new raw reading. Returns the "clean" value to use downstream:
    //   - on OUT_OF_RANGE / UNREALISTIC_JUMP: returns the last known-good value (sample rejected)
    //   - on an unconfirmed statistical spike: returns the window mean (spike filtered out)
    //   - otherwise: returns rawValue itself
    // Call getLastAnomaly() after ingest() to see what, if anything, was flagged.
    float ingest(float rawValue, uint32_t nowMs);

    SensorAnomaly getLastAnomaly() const;
    bool hasFault() const; // true only for FROZEN / UNREALISTIC_JUMP / OUT_OF_RANGE

private:
    static constexpr uint8_t WINDOW_LEN = 10;
    float   window[WINDOW_LEN];
    uint8_t windowHead;
    uint8_t windowCount;

    float    lastGoodValue;
    uint32_t lastTimestampMs;
    bool     haveLast;

    uint32_t frozenSinceMs;
    bool     frozenTimerRunning;

    uint8_t  spikeConfirmCount;
    float    spikeCandidateValue;

    SensorAnomaly lastAnomaly;

    float windowMean() const;
    float windowStdDev(float mean) const;
    void  pushWindow(float v);
    void  resetWindowAround(float v);
};

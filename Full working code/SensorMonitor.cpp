// ============================================================================
// SensorMonitor.cpp
// ============================================================================

#include "SensorMonitor.h"
#include <math.h>

SensorMonitor::SensorMonitor()
    : windowHead(0), windowCount(0),
      lastGoodValue(0.0f), lastTimestampMs(0), haveLast(false),
      frozenSinceMs(0), frozenTimerRunning(false),
      spikeConfirmCount(0), spikeCandidateValue(0.0f),
      lastAnomaly(SensorAnomaly::NONE)
{
    for (uint8_t i = 0; i < WINDOW_LEN; i++) window[i] = 0.0f;
}

float SensorMonitor::windowMean() const {
    if (windowCount == 0) return lastGoodValue;
    float sum = 0.0f;
    for (uint8_t i = 0; i < windowCount; i++) sum += window[i];
    return sum / (float)windowCount;
}

float SensorMonitor::windowStdDev(float mean) const {
    if (windowCount < 2) return 0.0f;
    float sumSq = 0.0f;
    for (uint8_t i = 0; i < windowCount; i++) {
        float d = window[i] - mean;
        sumSq += d * d;
    }
    return sqrtf(sumSq / (float)windowCount);
}

void SensorMonitor::pushWindow(float v) {
    window[windowHead] = v;
    windowHead = (windowHead + 1) % WINDOW_LEN;
    if (windowCount < WINDOW_LEN) windowCount++;
}

void SensorMonitor::resetWindowAround(float v) {
    // A genuine rapid change was confirmed: rebase the statistical window
    // around the new operating point instead of dragging the old mean along.
    windowHead = 0;
    windowCount = 1;
    window[0] = v;
}

float SensorMonitor::ingest(float rawValue, uint32_t nowMs) {
    lastAnomaly = SensorAnomaly::NONE;

    // ---- 1. Out-of-range: physically impossible for the sensor's spec ----
    if (rawValue < SENSOR_MIN_VALID || rawValue > SENSOR_MAX_VALID) {
        lastAnomaly = SensorAnomaly::OUT_OF_RANGE;
        return haveLast ? lastGoodValue : 0.0f; // hold last good value, reject sample
    }

    // ---- 2. Frozen sensor: output not changing at all over time ----------
    if (haveLast) {
        if (fabsf(rawValue - lastGoodValue) < SENSOR_FROZEN_EPSILON) {
            if (!frozenTimerRunning) {
                frozenTimerRunning = true;
                frozenSinceMs = nowMs;
            } else if (nowMs - frozenSinceMs >= SENSOR_FROZEN_WINDOW_MS) {
                lastAnomaly = SensorAnomaly::FROZEN;
                return lastGoodValue; // still hold, sensor isn't giving new info
            }
        } else {
            frozenTimerRunning = false; // value moved, reset the frozen timer
        }
    }

    // ---- 3. Unrealistic single-sample jump: exceeds physical slew rate ---
    if (haveLast) {
        uint32_t dt = (nowMs > lastTimestampMs) ? (nowMs - lastTimestampMs) : 1;
        float maxAllowedDelta = SENSOR_MAX_SLEW_PER_MS * (float)dt;
        float delta = fabsf(rawValue - lastGoodValue);
        if (delta > maxAllowedDelta) {
            lastAnomaly = SensorAnomaly::UNREALISTIC_JUMP;
            return lastGoodValue; // physically impossible in this dt -> reject as sensor fault
        }
    }

    // ---- 4. Statistical spike vs genuine rapid load change ---------------
    // Within physical slew limits, but is it just noise or a real fast change?
    if (windowCount >= WINDOW_LEN / 2) {
        float mean = windowMean();
        float sd   = windowStdDev(mean);
        float dev  = fabsf(rawValue - mean);

        if (sd > 0.0f && dev > SPIKE_SIGMA * sd) {
            // Candidate spike. Does it repeat (real change) or vanish (noise)?
            if (spikeConfirmCount > 0 && fabsf(rawValue - spikeCandidateValue) < (SPIKE_SIGMA * sd * 0.5f)) {
                spikeConfirmCount++;
            } else {
                spikeConfirmCount = 1;
                spikeCandidateValue = rawValue;
            }

            if (spikeConfirmCount >= PERSIST_CONFIRM) {
                // Confirmed: this is a genuine rapid load change, not noise.
                resetWindowAround(rawValue);
                spikeConfirmCount = 0;
                lastGoodValue = rawValue;
                lastTimestampMs = nowMs;
                haveLast = true;
                return rawValue;
            }

            // Not yet confirmed: treat as noise, filter it out with the
            // window mean instead of letting the outlier through.
            lastTimestampMs = nowMs; // still track time progression
            return mean;
        } else {
            spikeConfirmCount = 0; // sample is consistent with recent window, no spike in progress
        }
    }

    // ---- Normal sample: accept it -----------------------------------------
    pushWindow(rawValue);
    lastGoodValue = rawValue;
    lastTimestampMs = nowMs;
    haveLast = true;
    return rawValue;
}

SensorAnomaly SensorMonitor::getLastAnomaly() const { return lastAnomaly; }

bool SensorMonitor::hasFault() const {
    return lastAnomaly == SensorAnomaly::FROZEN
        || lastAnomaly == SensorAnomaly::UNREALISTIC_JUMP
        || lastAnomaly == SensorAnomaly::OUT_OF_RANGE;
}

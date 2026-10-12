#pragma once
// ============================================================================
// BMSConfig.h
// Single source of truth for pack size and chemistry constants.
// Changing NUM_CELLS here is the ONLY edit needed to scale the whole engine
// from a 4S pack up to a 16S pack (or beyond, RAM permitting).
// ============================================================================

#include <stdint.h>

// ---- Pack scale -----------------------------------------------------------
constexpr uint8_t NUM_CELLS = 4;          // <-- change this single constant to rescale (e.g. 16)

// ---- Cell chemistry assumptions (typical Li-ion NMC/LFP-ish defaults) -----
constexpr float CELL_V_MIN        = 2.8f;   // volts, hard low limit
constexpr float CELL_V_MAX        = 4.2f;   // volts, hard high limit
constexpr float CELL_V_NOMINAL    = 3.7f;   // volts

// ---- Imbalance thresholds (base values, adapted at runtime) ---------------
constexpr float BASE_IMBALANCE_V  = 0.050f; // 50 mV base balancing threshold
constexpr float MIN_IMBALANCE_V   = 0.020f; // never tighten below this
constexpr float MAX_IMBALANCE_V   = 0.150f; // never relax above this

// ---- Pack electrical rating, used to normalize discharge-rate effects -----
constexpr float RATED_CURRENT_A   = 20.0f;  // amps, pack's rated continuous current

// ---- History buffer used for trend detection -------------------------------
constexpr uint8_t IMBALANCE_HISTORY_LEN = 8; // samples kept for trend slope

#pragma once
// ============================================================================
// LcdConfig.h
// Geometry and timing constants for the flicker-free LCD engine (Task 3).
// ============================================================================

#include <stdint.h>

// ---- Display geometry (20x4 I2C HD44780 assumed) ---------------------------
constexpr uint8_t LCD_I2C_ADDR = 0x27;   // common PCF8574 backpack address (0x3F on some)
constexpr uint8_t LCD_COLS     = 20;
constexpr uint8_t LCD_ROWS     = 4;

// ---- Timing (all evaluated against millis(), never delay()) ----------------
constexpr uint32_t LCD_REFRESH_MS = 250;   // how often page content is re-composed from live data
constexpr uint32_t LCD_PAGE_MS    = 3000;  // dwell time per page before auto-rotating
constexpr uint32_t LCD_BLINK_MS   = 500;   // fault-screen header blink half-period

// ---- Bus-time budget --------------------------------------------------------
// Each character written over I2C blocks for roughly a millisecond (estimate;
// measure on your hardware). Capping characters per update() call bounds how
// long one loop() pass can be held up by the display.
constexpr uint8_t LCD_MAX_CHARS_PER_TICK = 16;

// Layouts in LcdEngine.cpp are written for a 20x4 panel.
static_assert(LCD_COLS >= 20 && LCD_ROWS >= 4, "LcdEngine layouts need at least a 20x4 display");

#pragma once
// ============================================================================
// LcdEngine.h
// Task 3: Flicker-Free LCD Display Engine
//
// How it avoids flicker:
//   - Two character grids are kept in RAM: `back` (what we want on screen)
//     and `front` (what the LCD currently shows).
//   - Pages are composed into `back`. flush() then writes ONLY the cells
//     where back != front, so unchanged text is never touched.
//   - lcd.clear() is called exactly once, at startup. Page changes and
//     fault screens are also done by diffing, never by clearing.
//
// How it stays non-blocking:
//   - Page rotation, content refresh and fault blinking are millis() timers.
//   - flush() writes at most LCD_MAX_CHARS_PER_TICK characters per call and
//     resumes where it left off on the next call.
//   - Exception: when a critical fault is raised, the first flush is allowed
//     to write the whole screen so the override appears immediately.
//
// The engine takes plain data (LcdData) rather than depending on the BMS or
// relay classes, so it can be reused unchanged by later tasks.
// ============================================================================

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "LcdConfig.h"

// Everything the pages can show. The sketch fills this from BMSEngine's
// PackStatus (Task 1) and the relay/system state (Tasks 2 and 4).
struct LcdData {
    float       packV;
    float       weakestV;
    float       strongestV;
    float       imbalanceV;
    uint8_t     weakestIndex;
    uint8_t     strongestIndex;
    int8_t      trend;          // -1 narrowing, 0 stable, +1 widening
    float       soc;            // 0.0 - 1.0
    float       currentA;
    const char* stateName;      // e.g. "NORMAL", "TRIPPED"
    bool        relayClosed;
    uint16_t    faultCount;
    uint32_t    uptimeSeconds;
};

enum class LcdPage : uint8_t {
    BATTERY = 0,
    SYSTEM,
    TELEMETRY,
    COUNT
};

class LcdEngine {
public:
    explicit LcdEngine(LiquidCrystal_I2C& lcd);

    void begin();

    // Call every loop(). Cheap when nothing changed.
    void update(const LcdData& data, uint32_t nowMs);

    // Critical fault override. The fault screen replaces whatever page is
    // showing and stays until clearFault() is called explicitly.
    void raiseFault(uint8_t faultId, const char* source, const char* message);
    void clearFault();
    bool isFaultActive() const;

    LcdPage  getPage() const;

    // Diagnostics: lets you show how much bus traffic the diffing saved.
    uint32_t getCharsWritten() const;
    uint32_t getComposeCount() const;

private:
    LiquidCrystal_I2C& lcd;

    char back [LCD_ROWS][LCD_COLS];
    char front[LCD_ROWS][LCD_COLS];
    uint16_t scanPos;            // resume point for partial flushes

    LcdPage  page;
    uint32_t lastPageMs;
    uint32_t lastComposeMs;
    bool     firstFrame;

    bool     faultActive;
    bool     faultJustRaised;
    bool     faultJustCleared;
    uint8_t  faultId;
    char     faultSource[LCD_COLS + 1];
    char     faultMessage[LCD_COLS + 1];
    bool     blinkOn;
    uint32_t lastBlinkMs;

    uint32_t charsWritten;
    uint32_t composeCount;

    void setRow(uint8_t row, const char* fmt, ...);
    void composeBattery  (const LcdData& d);
    void composeSystem   (const LcdData& d);
    void composeTelemetry(const LcdData& d);
    void composeFault();
    void composeCurrentPage(const LcdData& d);
    uint16_t flush(uint16_t budget);
};

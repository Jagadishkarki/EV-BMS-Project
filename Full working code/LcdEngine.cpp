// ============================================================================
// LcdEngine.cpp
// ============================================================================

#include "LcdEngine.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

LcdEngine::LcdEngine(LiquidCrystal_I2C& l)
    : lcd(l), scanPos(0),
      page(LcdPage::BATTERY), lastPageMs(0), lastComposeMs(0), firstFrame(true),
      faultActive(false), faultJustRaised(false), faultJustCleared(false), faultId(0),
      blinkOn(true), lastBlinkMs(0),
      charsWritten(0), composeCount(0)
{
    faultSource[0]  = '\0';
    faultMessage[0] = '\0';
}

void LcdEngine::begin() {
    lcd.init();
    lcd.backlight();
    lcd.clear();   // the one and only clear(): afterwards the screen is known-blank

    for (uint8_t r = 0; r < LCD_ROWS; r++) {
        for (uint8_t c = 0; c < LCD_COLS; c++) {
            back[r][c]  = ' ';
            front[r][c] = ' ';
        }
    }
    scanPos = 0;
    firstFrame = true;
}

// ---------------------------------------------------------------------------
// setRow: format into a row of the back buffer, space-padded to full width.
// Padding matters: stale characters from a longer previous value are
// overwritten by the diff instead of lingering on screen.
// ---------------------------------------------------------------------------
void LcdEngine::setRow(uint8_t row, const char* fmt, ...) {
    char tmp[LCD_COLS + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    bool ended = false;
    for (uint8_t c = 0; c < LCD_COLS; c++) {
        if (tmp[c] == '\0') ended = true;
        back[row][c] = ended ? ' ' : tmp[c];
    }
}

// ---- Page layouts (20 columns x 4 rows) ------------------------------------

void LcdEngine::composeBattery(const LcdData& d) {
    char pack[10], lo[8], hi[8];
    dtostrf(d.packV,     5, 2, pack);
    dtostrf(d.weakestV,  4, 2, lo);
    dtostrf(d.strongestV,4, 2, hi);

    int socPct = (int)(d.soc * 100.0f + 0.5f);
    int imbMv  = (int)(d.imbalanceV * 1000.0f + 0.5f);
    const char* trend = (d.trend > 0) ? "UP  " : (d.trend < 0) ? "DOWN" : "FLAT";

    setRow(0, "BATTERY          1/3");
    setRow(1, "Pack%sV SoC %3d%%", pack, socPct);
    setRow(2, "L%u:%sV H%u:%sV", d.weakestIndex, lo, d.strongestIndex, hi);
    setRow(3, "Imb %2dmV Trend %s", imbMv, trend);
}

void LcdEngine::composeSystem(const LcdData& d) {
    setRow(0, "SYSTEM STATE      2/3");
    setRow(1, "State: %s", d.stateName ? d.stateName : "?");
    setRow(2, "Relay: %s", d.relayClosed ? "CLOSED" : "OPEN");
    setRow(3, "Faults: %u", (unsigned)d.faultCount);
}

void LcdEngine::composeTelemetry(const LcdData& d) {
    char cur[10];
    dtostrf(d.currentA, 6, 2, cur);
    int socPct = (int)(d.soc * 100.0f + 0.5f);
    int imbMv  = (int)(d.imbalanceV * 1000.0f + 0.5f);

    unsigned long s = d.uptimeSeconds;
    unsigned long hh = s / 3600UL;
    unsigned long mm = (s % 3600UL) / 60UL;
    unsigned long ss = s % 60UL;

    setRow(0, "TELEMETRY         3/3");
    setRow(1, "Current:%sA", cur);
    setRow(2, "SoC:%3d%% Imb:%2dmV", socPct, imbMv);
    setRow(3, "Uptime %02lu:%02lu:%02lu", hh, mm, ss);
}

void LcdEngine::composeFault() {
    setRow(0, blinkOn ? "!!! CRITICAL FAULT!!" : "                    ");
    setRow(1, "ID:%03u Src:%s", (unsigned)faultId, faultSource);
    setRow(2, "%s", faultMessage);
    setRow(3, "Awaiting fault clear");
}

void LcdEngine::composeCurrentPage(const LcdData& d) {
    switch (page) {
        case LcdPage::BATTERY:   composeBattery(d);   break;
        case LcdPage::SYSTEM:    composeSystem(d);    break;
        case LcdPage::TELEMETRY: composeTelemetry(d); break;
        default:                 composeBattery(d);   break;
    }
    composeCount++;
}

// ---------------------------------------------------------------------------
// flush: write only cells where back != front, at most `budget` characters.
// Consecutive differing cells in a row share one setCursor() call. scanPos
// remembers where we stopped, so a partially-written frame resumes on the
// next call instead of restarting from the top-left corner.
// ---------------------------------------------------------------------------
uint16_t LcdEngine::flush(uint16_t budget) {
    const uint16_t total = (uint16_t)LCD_ROWS * LCD_COLS;
    uint16_t written = 0;
    uint16_t scanned = 0;

    while (scanned < total && written < budget) {
        uint8_t r = scanPos / LCD_COLS;
        uint8_t c = scanPos % LCD_COLS;

        if (back[r][c] == front[r][c]) {
            scanPos = (scanPos + 1) % total;
            scanned++;
            continue;
        }

        lcd.setCursor(c, r);
        while (c < LCD_COLS && back[r][c] != front[r][c] && written < budget) {
            lcd.write((uint8_t)back[r][c]);
            front[r][c] = back[r][c];
            c++;
            written++;
            scanPos = (scanPos + 1) % total;
            scanned++;
        }
    }

    charsWritten += written;
    return written;
}

void LcdEngine::update(const LcdData& d, uint32_t nowMs) {
    const uint16_t fullScreen = (uint16_t)LCD_ROWS * LCD_COLS;

    // ---- Fault override: takes priority over everything else ---------------
    if (faultActive) {
        bool needCompose = false;
        bool urgent = false;

        if (faultJustRaised) {
            faultJustRaised = false;
            blinkOn = true;
            lastBlinkMs = nowMs;
            needCompose = true;
            urgent = true;               // show it NOW, not spread over ticks
        }
        if (nowMs - lastBlinkMs >= LCD_BLINK_MS) {
            lastBlinkMs = nowMs;
            blinkOn = !blinkOn;
            needCompose = true;
        }
        if (needCompose) composeFault();
        flush(urgent ? fullScreen : LCD_MAX_CHARS_PER_TICK);
        return;
    }

    // ---- Normal operation --------------------------------------------------
    bool needCompose = false;
    bool urgent = false;

    if (firstFrame) {
        firstFrame = false;
        lastPageMs = nowMs;
        needCompose = true;
    }
    if (faultJustCleared) {
        // Explicit clear: restart the rotation from the first page.
        faultJustCleared = false;
        page = LcdPage::BATTERY;
        lastPageMs = nowMs;
        needCompose = true;
        urgent = true;
    }
    if (nowMs - lastPageMs >= LCD_PAGE_MS) {
        page = (LcdPage)(((uint8_t)page + 1) % (uint8_t)LcdPage::COUNT);
        lastPageMs = nowMs;
        needCompose = true;
    }
    if (nowMs - lastComposeMs >= LCD_REFRESH_MS) {
        needCompose = true;
    }

    if (needCompose) {
        lastComposeMs = nowMs;
        composeCurrentPage(d);
    }
    flush(urgent ? fullScreen : LCD_MAX_CHARS_PER_TICK);
}

void LcdEngine::raiseFault(uint8_t id, const char* source, const char* message) {
    faultId = id;
    strncpy(faultSource,  source  ? source  : "?", LCD_COLS);
    strncpy(faultMessage, message ? message : "",  LCD_COLS);
    faultSource[LCD_COLS]  = '\0';
    faultMessage[LCD_COLS] = '\0';
    faultActive     = true;
    faultJustRaised = true;
}

void LcdEngine::clearFault() {
    if (!faultActive) return;
    faultActive      = false;
    faultJustCleared = true;
}

bool     LcdEngine::isFaultActive()   const { return faultActive; }
LcdPage  LcdEngine::getPage()         const { return page; }
uint32_t LcdEngine::getCharsWritten() const { return charsWritten; }
uint32_t LcdEngine::getComposeCount() const { return composeCount; }

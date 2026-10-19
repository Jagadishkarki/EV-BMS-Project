// ============================================================================
// MainProject.ino
// Integration sketch: Tasks 1-6 in one firmware.
//
//   Task 1  BMSEngine         cell analysis, adaptive imbalance threshold
//   Task 2  SafetyRelay       non-blocking protection relay + SensorMonitor
//   Task 3  LcdEngine         flicker-free LCD, page rotation, fault screen
//   Task 4  Fsm/FaultManager  NORMAL/DEGRADED/FAILSAFE/SHUTDOWN + fault isolation
//   Task 5  TelemetryEngine   event-driven telemetry, offline queue, Wi-Fi FSM
//   Task 6  BlynkDashboard    risk score, severity, history, recommendations
//
// Setup: copy the six folders in /libraries into your Arduino "libraries"
// directory (or use them via PlatformIO lib_extra_dirs), and make sure
// BlynkConfig.h in this folder has your real credentials.
//
// Serial commands (115200 baud) for demonstrations:
//   f  inject a CELL fault for ~4 s        n  toggle simulated network outage
//   a  freeze ADC channel 1 for ~5 s       r  reset out of SHUTDOWN
// ============================================================================
#define NO_GLOBAL_BLYNK

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define BLYNK_PRINT Serial
#include "BlynkConfig.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#include <BMSConfig.h>
#include <BMSEngine.h>
#include <RelaySafetyConfig.h>
#include <SensorMonitor.h>
#include <SafetyRelay.h>
#include <LcdConfig.h>
#include <LcdEngine.h>
#include <FsmConfig.h>
#include <SystemTypes.h>
#include <Fsm.h>
#include <FaultManager.h>
#include <TelemetryEngine.h>
#include <WifiManager.h>
#include <RiskEngine.h>
#include <BlynkDashboard.h>

LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
LcdEngine         display(lcd);
BMSEngine         bms;
SensorMonitor     currentSensor;
SafetyRelay       relay;
FaultManager      faultMgr;
Fsm               fsm;
WifiManager       wifiMgr;
TelemetryEngine   telemetry;
BlynkDashboard    dashboard;

WiFiClient blynkWifiClient;
BlynkEsp32Client blynkTransport(blynkWifiClient);
BlynkWifi Blynk(blynkTransport);

float    simVoltages[NUM_CELLS];
float    simSoC = 0.70f;
uint32_t bootMs;

// ---- Demo fault injection flags (driven from Serial) -------------------------
uint32_t cellFaultUntilMs = 0;
uint32_t adcFreezeUntilMs = 0;
bool     simulateOutage   = false;
uint16_t adcFrozenValue   = 2048;

// ---- Fault-frequency window for the risk engine -------------------------------
constexpr uint8_t  FREQ_WINDOW_LEN = 10;
constexpr uint32_t FREQ_WINDOW_MS  = 60000;
uint32_t faultTimestamps[FREQ_WINDOW_LEN];
uint8_t  faultTsHead = 0, faultTsCount = 0;
uint16_t totalDegradedEntries = 0;

uint16_t faultsInWindow(uint32_t nowMs) {
    uint16_t c = 0;
    for (uint8_t i = 0; i < faultTsCount; i++)
        if (nowMs - faultTimestamps[i] <= FREQ_WINDOW_MS) c++;
    return c;
}

void handleSerial(uint32_t now) {
    while (Serial.available()) {
        char ch = Serial.read();
        if (ch == 'f') { cellFaultUntilMs = now + 4000; Serial.println(F("[DEMO] cell fault injected")); }
        if (ch == 'a') { adcFreezeUntilMs = now + 5000; Serial.println(F("[DEMO] ADC ch1 frozen")); }
        if (ch == 'n') { simulateOutage = !simulateOutage;
                         if (simulateOutage) WiFi.disconnect(true);
                         Serial.println(simulateOutage ? F("[DEMO] outage ON") : F("[DEMO] outage OFF")); }
        if (ch == 'r') { fsm.reset(now); }
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    for (uint8_t i = 0; i < NUM_CELLS; i++) simVoltages[i] = 3.70f + i * 0.01f;

    bms.begin();
    relay.begin();
    display.begin();
    faultMgr.begin(2);
    telemetry.begin();
    dashboard.begin();
    bootMs = millis();
    fsm.begin(bootMs);

    // Blynk.config + our own non-blocking WifiManager: we do NOT call the
    // blocking Blynk.begin(ssid, pass). Blynk.connect() is attempted only
    // while Wi-Fi is up, with a short timeout, from loop().
    Blynk.config(BLYNK_AUTH_TOKEN);
    wifiMgr.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
    uint32_t now = millis();
    handleSerial(now);

    // ---- Network (Task 5): non-blocking ----------------------------------
    if (!simulateOutage) wifiMgr.update(now);
    bool online = wifiMgr.isConnected() && !simulateOutage;
    if (online) {
        if (!Blynk.connected()) {
            static uint32_t lastTry = 0;
            if (now - lastTry > 3000) { lastTry = now; Blynk.connect(1500); }
        } else {
            Blynk.run();
        }
    }
    bool blynkUp = online && Blynk.connected();

    // ---- Task 1: battery -----------------------------------------------------
    simVoltages[0] -= 0.0002f;
    simSoC -= 0.00003f; if (simSoC < 0.05f) simSoC = 0.05f;
    float simCurrentA = 6.0f + (random(-30, 30) / 100.0f);
    bms.update(simVoltages, simSoC, simCurrentA, now);
    const PackStatus& pack = bms.getStatus();

    // ---- Task 2: relay ---------------------------------------------------------
    float cleanCurrent = currentSensor.ingest(simCurrentA, now);
    relay.update(cleanCurrent, currentSensor.getLastAnomaly(), now);

    // ---- Task 4: faults + FSM ------------------------------------------------
    faultMgr.updateAdcChannel(0, 2048 + random(-50, 50), now);
    uint16_t ch1 = (now < adcFreezeUntilMs) ? adcFrozenValue : (uint16_t)(2048 + random(-50, 50));
    faultMgr.updateAdcChannel(1, ch1, now);
    faultMgr.updateRelayFeedback(relay.isRelayClosed(), relay.isRelayClosed(), now);
    faultMgr.noteCommMessageReceived(now); // simulated healthy bus
    faultMgr.checkCommTimeout(now);
    faultMgr.reportExternalFault(FaultSource::CELL, 401, (now < cellFaultUntilMs) || bms.isImbalanceCritical());

    FaultSource src; uint16_t fid;
    bool faultActive = faultMgr.hasActiveFault(src, fid);
    OpState prev = fsm.getState();
    fsm.tick(faultActive, src, fid, now);
    if (fsm.getState() == OpState::DEGRADED && prev != OpState::DEGRADED) {
        faultTimestamps[faultTsHead] = now;
        faultTsHead = (faultTsHead + 1) % FREQ_WINDOW_LEN;
        if (faultTsCount < FREQ_WINDOW_LEN) faultTsCount++;
        totalDegradedEntries++;
    }

    // ---- Task 3: LCD -------------------------------------------------------------
    LcdData ld;
    ld.packV = pack.packVoltage; ld.weakestV = pack.weakestV; ld.strongestV = pack.strongestV;
    ld.imbalanceV = pack.imbalance.imbalanceV; ld.weakestIndex = pack.weakestIndex;
    ld.strongestIndex = pack.strongestIndex; ld.trend = pack.imbalance.trend;
    ld.soc = pack.stateOfCharge; ld.currentA = pack.dischargeRateA;
    ld.stateName = opStateName(fsm.getState());
    ld.relayClosed = relay.isRelayClosed();
    ld.faultCount = totalDegradedEntries;
    ld.uptimeSeconds = (now - bootMs) / 1000UL;

    bool critical = (fsm.getState() == OpState::FAILSAFE || fsm.getState() == OpState::SHUTDOWN);
    if (critical && !display.isFaultActive())
        display.raiseFault((uint8_t)fsm.getFaultId(), faultSourceName(fsm.getFaultSource()), opStateName(fsm.getState()));
    else if (!critical && display.isFaultActive())
        display.clearFault();
    display.update(ld, now);

    // ---- Task 5: event-driven telemetry ------------------------------------
    TelemetrySnapshot ts;
    ts.packV = pack.packVoltage; ts.weakestV = pack.weakestV; ts.strongestV = pack.strongestV;
    ts.weakestIndex = pack.weakestIndex; ts.strongestIndex = pack.strongestIndex;
    ts.soc = pack.stateOfCharge; ts.currentA = pack.dischargeRateA;
    ts.cellCount = NUM_CELLS;
    float cells[NUM_CELLS];
    for (uint8_t i = 0; i < NUM_CELLS; i++) cells[i] = pack.cells[i].voltage;
    ts.cellVoltages = cells;
    ts.relayClosed = relay.isRelayClosed();
    ts.state = fsm.getState(); ts.faultSource = fsm.getFaultSource(); ts.faultId = fsm.getFaultId();
    telemetry.update(ts, wifiMgr, blynkUp, now);

    // ---- Task 6: analytics dashboard (only when Blynk is reachable) ----------
    if (blynkUp) {
        DashboardData dd;
        dd.imbalanceV = pack.imbalance.imbalanceV; dd.adaptiveThresholdV = pack.imbalance.adaptiveThreshold;
        dd.imbalanceTrend = pack.imbalance.trend; dd.soc = pack.stateOfCharge; dd.currentA = pack.dischargeRateA;
        dd.state = fsm.getState(); dd.faultSource = fsm.getFaultSource();
        dd.totalFaultCount = totalDegradedEntries; dd.faultsInLastWindow = faultsInWindow(now);
        dd.uptimeSeconds = (now - bootMs) / 1000UL;
        dashboard.update(dd, fsm, now);
    }
}

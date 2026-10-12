# Assumptions

## Platform and tooling
- Target MCU: ESP32 (Arduino framework). Task 5/6 need Wi-Fi; the logic modules also compile on AVR except where noted.
- Blynk IoT (Blynk 2.0 cloud, template/auth-token model), not legacy Blynk.
- LCD: 20x4 HD44780 with PCF8574 I2C backpack, address 0x27, `LiquidCrystal_I2C` library.
- Each task is packaged as an Arduino library so the repo can use a per-task folder structure; Arduino IDE cannot compile subfolders inside a single sketch folder.

## Inputs are simulated
- Cell voltages, SoC, current, ADC readings and relay feedback in `MainProject.ino` are **simulated**. Real hardware (cell-monitor IC, current sensor, relay feedback line) is not wired or modelled.
- SoC is supplied to the engine by the caller; no Coulomb counting or OCV table is implemented.

## Design assumptions
- Pack chemistry defaults to generic Li-ion (2.8–4.2 V/cell); base imbalance threshold 50 mV, adaptive range 20–150 mV. All values are starting points, untuned.
- Task 4 fault priority: CELL > RELAY > COMM > ADC (most to least directly threatening to the pack).
- SHUTDOWN is terminal; only an explicit `Fsm::reset()` (operator action) leaves it.
- A "failed recovery attempt" means a fault reappearing after it had cleared while in FAILSAFE.
- Offline queue overflow policy: drop the oldest event, keep the newest.
- "Meaningful change" thresholds for telemetry: 10 mV per cell, 1 % SoC, 0.5 A, any state/relay/fault change, plus a 30 s heartbeat.
- Dashboard "health %" is defined as 100 − risk score.
- Risk weights (imbalance 0.5, fault frequency 0.3, SoC 0.2) are engineering judgement, not derived from data.

## Documents not available to the author
- No assignment document was provided, so the exact required folder structure, screenshot filenames, and report template were **not** followed verbatim. The layout here is a reasonable default and must be reconciled with the real instructions.
- The assignment text mentions RTL, ModelSim and waveforms. This project is C++ firmware; those sections are marked N/A in the report (see below). If an HDL deliverable is actually required, it is **not** covered by this repository.

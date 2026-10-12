# Real-Time EV Battery Management System — Technical Report

*Author: ____________  |  Course: ____________  |  Date: ____________*

> **Reading guide.** This report describes what was actually built and what
> was actually verified. Where something was not done or not possible, it
> says so. Sections 3 and 6 explain why two mandated headings (RTL Design,
> Simulation Results) are handled differently for a firmware project.

---

## 1. Project Overview

The project is a modular battery-management firmware for an ESP32, built as
six cooperating modules that share one integration sketch:

| # | Module | Purpose |
|---|--------|---------|
| 1 | BMS engine | Weakest/strongest cell, imbalance, trend, SoC/load-adaptive threshold; scalable 4–16 cells from one constant |
| 2 | Protection relay | Non-blocking relay with hysteresis + debounce, sensor anomaly detection, timed recovery |
| 3 | LCD engine | Flicker-free diff rendering, three rotating pages, critical-fault override |
| 4 | Fault FSM | NORMAL / DEGRADED / FAILSAFE / SHUTDOWN, fault-source isolation, verified recovery, structured log |
| 5 | Telemetry | Event-driven sends, fixed-size offline queue with ordered replay, non-blocking Wi-Fi state machine, RSSI monitoring, live Blynk dashboard |
| 6 | Analytics | Composite risk score, severity colours/icons tied to FSM state, fault-history replay, maintenance suggestions, executive summary |

Design goals common to all modules: no `delay()`, no dynamic allocation, a
clean data-struct interface so later modules reuse earlier ones without
duplicating logic.

**Scope note.** Sensor inputs (cell voltages, current, ADC counts, relay
feedback) are simulated in the integration sketch. No hardware run was
performed in preparing this report.

## 2. Architecture

```
            +-----------------+      PackStatus       +------------------+
 cells ---> |  Task1 BMSEngine| --------------------> |  Task3 LcdEngine |
            +--------+--------+                       +------------------+
                     | isImbalanceCritical()
                     v
 current -> Task2 SensorMonitor -> SafetyRelay --(relay state)--+
                                                                |
 ADC / relay fb / comms --> Task4 FaultManager --(src,id)--> Task4 Fsm --(OpState, log)--+
                                                                |                       |
                                          TelemetrySnapshot     v                       v
                     Task5 TelemetryEngine <-- WifiManager   Task6 BlynkDashboard + RiskEngine
                          |   (live pins / OfflineQueue)           (analytics pins)
                          +-----------------> Blynk Cloud <--------+
```

Key architectural decisions:

- **Libraries per task.** Each task is an Arduino library (`library.properties` + `src/`), giving a clean per-task repository layout that still compiles in the Arduino IDE.
- **Data-in / data-out interfaces.** `TelemetryEngine` and `RiskEngine` accept plain structs, not other modules' classes, so they are unit-testable and have no hard coupling.
- **Shared vocabulary in one place.** `SystemTypes.h` (OpState, FaultSource) and `PinMap.h` (every Blynk pin) live in the Task 4 library, which Tasks 5 and 6 declare as a dependency.
- **Pin ownership.** Task 5 writes V0–V2, V7, V14–V18; Task 6 writes V3–V6, V8–V13. This prevents an analytics push from overwriting a replayed queued value.
- **Compile-time scaling (Task 1).** `NUM_CELLS` sizes every array.

### Scaling 4 → 16 cells (Task 1)

| Aspect | 4S | 16S | Note |
|---|---|---|---|
| `cells[]` array | 4 × 8 B ≈ 32 B | 16 × 8 B ≈ 128 B | Linear in `NUM_CELLS` (size is for a typical 32-bit target; AVR is smaller) |
| Trend history | 8 floats | 8 floats | Independent of cell count by design |
| `update()` time | one pass over 4 cells | one pass over 16 cells | O(n), single loop, no nested scans |
| Dynamic allocation | none | none | Fixed arrays throughout |

Execution time was reasoned about, not measured on hardware.

### Display refresh justification (Task 3)

Content is recomposed every 250 ms: character LCD pixels respond in the order of 100–300 ms (slower when cold), values that change faster than a few times per second cannot be read, and pack data (voltage, SoC, imbalance) changes over seconds. Because only changed characters are written, most refreshes cost near-zero bus time. Pages dwell 3 s (about the time to read four short lines). The fault screen bypasses both timers and appears within one loop pass. These are engineering estimates; response times were not measured.

## 3. RTL Design — Not Applicable

This project is **Arduino/C++ firmware for an ESP32**, not a Verilog/VHDL
design, so there is no RTL, no synthesizable hardware description, and no
ModelSim project. Fabricating an RTL section for software would be
misleading. The closest analogues in this project are:

- the **explicit transition table** in `Fsm.cpp` (the part of the design that would be a state-register + next-state logic in RTL),
- the **fixed-size, compile-time-scaled data structures** (analogous to parameterised hardware widths),
- the **timer/debounce counters** built from `millis()` comparisons (analogous to hardware counters).

**If the course genuinely requires an HDL implementation, this repository
does not provide one**; that would be a separate deliverable in a different
toolchain.

## 4. FSM Design

### States
`enum class OpState { NORMAL, DEGRADED, FAILSAFE, SHUTDOWN }`

| State | Meaning |
|-------|---------|
| NORMAL | Nominal operation |
| DEGRADED | A fault is active; cautious operation. Minimum dwell 1 s prevents cycling |
| FAILSAFE | DEGRADED persisted 10 s, or 3 DEGRADED entries occurred within 30 s |
| SHUTDOWN | Terminal: 4 failed recovery attempts. Exits only via explicit `reset()` |

### Transition table (the only legal moves)

| From | Event | To | Guard (evaluated in `tick()`) |
|------|-------|----|-------------------------------|
| NORMAL | FAULT_RAISED | DEGRADED | any detector active |
| DEGRADED | FAULT_CLEARED | NORMAL | fault gone **and** dwell ≥ 1 s |
| DEGRADED | ESCALATE | FAILSAFE | fault persists ≥ 10 s, or repeat limit hit |
| FAILSAFE | RECOVERED | NORMAL | hold elapsed **and** verification window clean |
| FAILSAFE | GIVE_UP | SHUTDOWN | failed-recovery strikes ≥ 4 |
| SHUTDOWN | — | NORMAL | only via `Fsm::reset()` |

### Verified recovery (FAILSAFE)
Recovery is never a single lucky reading. Inside FAILSAFE two timed phases run: **HOLD** (5 s, growing with consecutive failed attempts up to 4×) and **VERIFY** (4 s). Any live fault during either phase restarts that phase's timer. Only after both complete cleanly does the machine return to NORMAL. A *failed attempt* is a fault reappearing (rising edge) after it had cleared; consecutive failures escalate to SHUTDOWN, and a sustained 30 s calm period in NORMAL forgives past strikes.

### Deadlock freedom
Every state has an exit: NORMAL↔DEGRADED by fault state; DEGRADED→FAILSAFE by time; FAILSAFE→NORMAL by timers or →SHUTDOWN by strikes; SHUTDOWN→NORMAL by explicit reset. Illegal events are ignored by the table lookup. The one terminal state is deliberate and has a defined manual exit.

### Fault isolation and logging
`FaultManager` tags each fault with a source (CELL/RELAY/COMM/ADC) and ID (ADC channel = 100+n, relay mismatch = 200, comm = 300, cell = caller-supplied). Priority when several are active: CELL > RELAY > COMM > ADC. Every transition is written to a 32-entry ring buffer *and* Serial as: timestamp, previous state, new state, fault source, fault ID.

### Relay sub-machine (Task 2)
`NORMAL → FAULT_DEBOUNCE → TRIPPED → RECOVERY_HOLD → RECOVERY_VERIFY → NORMAL`, with a trip threshold (25 A) distinct from the reset threshold (21 A) for hysteresis, a 300 ms debounce, 5 s hold and 2 s verify. `SensorMonitor` flags out-of-range, frozen (4 s), and physically-impossible-slew readings, and separately uses a 10-sample mean/σ window to treat a lone >3σ outlier as noise (filtered) while accepting a sustained shift (3 confirming samples) as a genuine load change.

### Telemetry state machine (Task 5)
`WifiManager`: IDLE → CONNECTING (10 s timeout) → CONNECTED → RETRY_WAIT (exponential backoff 2 s → 30 s cap) → CONNECTING…, using only `WiFi.status()` polling against `millis()`. RSSI is sampled every 5 s while connected. `TelemetryEngine` emits an event only on a meaningful change (10 mV per cell, 1 % SoC, 0.5 A, any state/relay/fault change) or a 30 s heartbeat; offline events enter a 24-slot FIFO (oldest dropped on overflow) and are replayed oldest-first, two per loop pass, with `VPIN_DATA_SOURCE` showing `LIVE` or `QUEUED(n)`.

## 5. Verification Methodology

Because no hardware or HDL simulator was available, verification was limited to **host-side unit testing of the hardware-independent logic**, compiled with `g++` against a small Arduino stub (`tests/host/stub/Arduino.h`).

- **Method:** deterministic scenario tests with explicit simulated timestamps, checking state, fault source/ID, relay output level, and log contents after each step.
- **Included:** boundary cases (min dwell, hysteresis band, debounce shorter than threshold), negative cases (single-sample spike must *not* fault), regression tests for a defect found during development (Bug Log #1), and a 20,000-step alternating fault-injection stress run checking the relay never reaches an invalid state and always recovers when input is clean.
- **Not included (not verified):** LCD engine (needs `LiquidCrystal_I2C`), `WifiManager`, `TelemetryEngine`, `BlynkDashboard`, the integration sketch's compilation, real-time/timing behaviour, Blynk cloud behaviour, and anything on actual ESP32 hardware. Timing constants are untuned starting values.

Reproduce with `sh tests/host/run_tests.sh`.

## 6. Simulation Results

No ModelSim/waveform simulation exists for this project (see §3). The
results that do exist are the **host-side test results**, saved verbatim in
`tests/host/last_run_output.txt`:

| Suite | Scope | Result |
|-------|-------|--------|
| A | Task 1 BMS (7 checks), Task 4 FSM + FaultManager (24), Task 5 queue (4), Task 6 risk (5) | **40 passed, 0 failed** |
| B | Task 2 SensorMonitor (8) and SafetyRelay (13), including 20k-step stress | **21 passed, 0 failed** |

Notable behaviours demonstrated by these tests: fault source/ID retained through DEGRADED; clear before 1 s dwell does *not* return to NORMAL; after FAILSAFE hold the system is still FAILSAFE (verifying) rather than instantly NORMAL; a fault mid-verification prevents early recovery; repeated failed recoveries reach SHUTDOWN, which never self-exits but is left by `reset()`; queue replay is in insertion order and overflow drops the oldest; 23 A (between reset and trip thresholds) does not start relay recovery.

These are logic-level results only. They are **not** evidence that the firmware works on hardware or against the Blynk cloud.

## 7. Challenges Faced

1. **Incomplete/shifting requirements.** The first FSM was built from a truncated spec with the wrong state set; it was rebuilt to the real four-state specification, and task numbering (telemetry vs analytics) had to be corrected.
2. **A subtle recovery bug.** Unit tests exposed that strike counting fired every tick, doubling the first hold time and allowing instant SHUTDOWN. The fix was rising-edge counting (Bug Log #1).
3. **Distinguishing noise from real load changes.** Solved with two separate checks: a physical slew limit (impossible = fault) and a statistical window with persistence confirmation (improbable but possible = noise unless it persists).
4. **Genuinely non-blocking networking.** Wi-Fi association is non-blocking, but Blynk's C++ library connect call is not; this remains an open limitation (Bug Log A) with a known mitigation (separate FreeRTOS task).
5. **Dashboard pin collisions** between live and analytics modules, solved with explicit pin ownership.
6. **Verifying without hardware.** Handled by isolating logic from I/O and testing it on a PC — which bounds, but cannot replace, hardware validation.

## 8. Conclusion

The six modules form a coherent, modular design: a scalable BMS core, a non-blocking protective relay with careful anti-chatter and anomaly handling, a diff-based display engine, a deterministic four-state fault machine with verified recovery and a deliberate terminal state, event-driven telemetry with an ordered offline queue, and an analytics layer whose severity is tied to the same state machine. The core decision logic is covered by 61 passing host-side tests, and testing found and fixed one genuine defect.

What remains is the work that cannot be done off-hardware: compiling and running the integration on an ESP32, validating the LCD/Wi-Fi/Blynk paths against real services, measuring timing, tuning thresholds, and resolving the blocking-connect limitation. Until that is done the system should be regarded as a verified-logic, unverified-integration prototype.

## Appendix — files
See `README.md` for the repository layout, `docs/Bug_Log.md` for defects and limitations, and `docs/Assumptions.md` for assumptions.

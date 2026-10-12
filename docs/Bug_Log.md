# Bug Log

Honest record of defects found and fixed during development, plus known
open issues. "Found by" shows how each was discovered.

## Fixed

| # | Component | Defect | Found by | Fix |
|---|-----------|--------|----------|-----|
| 1 | Task 4 `Fsm.cpp` | FAILSAFE strike counter incremented on **every tick** a fault was active, including the tick that entered FAILSAFE. Effect: first-entry hold time was doubled, and a fault lasting only a few milliseconds could force SHUTDOWN. | Host unit test (`verification passed -> NORMAL` failed) | Strikes now count only a **rising edge** (fault returns after having cleared) while already in FAILSAFE; the entry tick never counts. Regression tests added. |
| 2 | Task 4 design | First implementation used six states (INIT/NORMAL/WARNING/FAULT/FAILSAFE/RECOVERY) and was built from a truncated spec. Real spec requires exactly NORMAL/DEGRADED/FAILSAFE/SHUTDOWN. | Review against full assignment text | Rebuilt as a 4-state `enum class OpState`; verification (hold + verify) kept as internal FAILSAFE phases; SHUTDOWN added with explicit `reset()` exit. |
| 3 | Task numbering | Telemetry module (real Task 5) was initially missing; the analytics dashboard had been labelled Task 5. | Review against full assignment text | Telemetry built as Task 5; analytics relabelled Task 6. |
| 4 | Tasks 5/6 | Both modules wrote the same Blynk pins (V0, V7), so a fresh analytics push could overwrite a replayed *queued* value. | Design review while integrating | Pin ownership split (documented in `PinMap.h` and README). |
| 5 | Test code | Wrong expected imbalance in a unit test (3.73 − 3.55 = 0.18 V, not 0.15 V). | Failing test | Test corrected; production code was right. |

## Known open issues / limitations (not fixed)

| # | Area | Issue | Impact / suggested mitigation |
|---|------|-------|-------------------------------|
| A | `MainProject.ino` | `Blynk.connect(1500)` blocks the loop for up to 1.5 s per attempt (rate-limited to one attempt per 3 s while Wi-Fi is up). Blynk's C++ library has no fully non-blocking connect. | The safety relay/FSM loop can stall during a reconnect attempt. Mitigation: run networking on a separate FreeRTOS task/core. Not implemented. |
| B | Task 3 LCD | `LiquidCrystal_I2C` calls are blocking; the engine only bounds per-call character count. | Worst-case loop delay ~tens of ms at 100 kHz I2C (estimate, unmeasured). `Wire.setClock(400000)` reduces it. |
| C | Task 6 `BlynkDashboard` | Fault-history replay de-duplicates by timestamp, so two transitions logged in the same millisecond would send only the first. | Unlikely given the FSM's dwell times; use a monotonic sequence number in `FsmLogEntry` to remove entirely. |
| D | Task 5 `TelemetryEngine` | Queue overflow drops the oldest event (documented policy); a very long outage loses early history. | Increase `TELEMETRY_QUEUE_CAPACITY` (RAM cost scales with `TELEMETRY_MAX_CELLS`). |
| E | Cross-library include | `#include <Blynk/BlynkApi.h>` in Task 5/6 .cpp files relies on the Blynk library's header layout. | Not compiled against the real library; may need adjusting to the installed Blynk version. |
| F | Integration | The simulated relay feedback in `MainProject.ino` echoes the command, so a real relay-mismatch fault never fires in the demo. | Wire a real feedback pin to exercise the detector (it is host-tested). |
| G | Everything hardware-facing | No on-device testing was performed. All timing constants are untuned starting values. | Tune on hardware. |

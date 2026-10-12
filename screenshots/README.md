# Screenshots

**No screenshots are included, on purpose.** They must be captured from a
real run on your own setup; generating or editing images to look like real
output would be fabricating evidence.

## Naming
The assignment document specifies the *exact* required filenames. They were
not available when this repository was prepared. The names below are
**placeholders** — rename each file to the exact name your assignment lists
before submitting.

## Suggested captures (from your own run)

| Placeholder name | What to capture |
|------------------|-----------------|
| `task1_bms_serial.png` | Serial Monitor: weakest/strongest cell, imbalance, adaptive threshold, trend |
| `task2_relay_fault_injection.png` | Serial log showing debounce → TRIPPED → RECOVERY_HOLD → RECOVERY_VERIFY → NORMAL |
| `task3_lcd_pages.png` | Photos of the three rotating LCD pages |
| `task3_lcd_fault_screen.png` | Photo of the fault override screen |
| `task4_fsm_transitions.png` | Serial log of FSM transitions with timestamp, prev/new state, fault source/id |
| `task4_failsafe_recovery.png` | FAILSAFE → verified recovery → NORMAL |
| `task5_outage_queue.png` | Blynk dashboard during an outage (queue depth rising, Wi-Fi health) |
| `task5_queue_replay.png` | Dashboard after reconnect: `QUEUED(n)` draining, then `LIVE` |
| `task6_dashboard_overview.png` | Risk score, severity LED, executive summary, maintenance suggestion |
| `task6_fault_history.png` | Terminal widget showing replayed state transitions |
| `host_tests_pass.png` | Terminal running `sh tests/host/run_tests.sh` (this one you can produce right now) |

ModelSim/waveform screenshots do not apply to this firmware project; see
`docs/Technical_Report.md`, sections 3 and 6.

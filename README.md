# Real-Time EV Battery Management System (ESP32 / Arduino C++)

A modular, non-blocking battery management firmware with fault isolation,
event-driven cloud telemetry, and an analytics dashboard. Six tasks, one
integrated project.

> **Status in one line:** the hardware-independent logic (Tasks 1, 2, 4, the
> Task 5 queue, the Task 6 risk engine) is covered by 61 passing host-side
> unit tests. The hardware-facing code (LCD, Wi-Fi, Blynk, GPIO timing) and
> the full integration sketch have **not** been compiled for or run on an
> ESP32. See *Verification status* below.

## Repository layout

```
EV-BMS-Project/
├── README.md
├── .gitignore                     (excludes real credentials)
├── config/
│   └── BlynkConfig.example.h      template; copy to firmware/MainProject/BlynkConfig.h
├── docs/
│   ├── Technical_Report.md
│   ├── Bug_Log.md
│   ├── Assumptions.md
│   └── Self_Attestation_Checklist.md
├── libraries/                     each task is a real Arduino library
│   ├── Task1_BMSEngine/           modular cell analysis, adaptive thresholds
│   ├── Task2_SafetyRelay/         non-blocking relay + SensorMonitor
│   ├── Task3_LcdEngine/           flicker-free diff-based LCD engine
│   ├── Task4_FaultFsm/            NORMAL/DEGRADED/FAILSAFE/SHUTDOWN FSM
│   │                              (also owns SystemTypes.h and PinMap.h shared by 5 & 6)
│   ├── Task5_Telemetry/           event-driven telemetry, offline queue, Wi-Fi FSM
│   └── Task6_Analytics/           risk engine + Blynk analytics dashboard
├── firmware/MainProject/
│   ├── MainProject.ino            integration sketch (Tasks 1-6)
│   └── BlynkConfig.h              YOUR credentials (git-ignored)
├── tests/host/                    PC-side logic tests + saved run output
├── screenshots/                   see screenshots/README.md
└── video/                         see video/README.md
```

## Build (ESP32)

1. Install the ESP32 board package, plus libraries **Blynk** and **LiquidCrystal_I2C**.
2. Copy the six folders in `libraries/` into your Arduino `libraries/` directory.
3. Copy `config/BlynkConfig.example.h` to `firmware/MainProject/BlynkConfig.h` and fill in
   your Blynk template ID / auth token and Wi-Fi credentials.
4. In the Blynk console, create widgets bound to the pins in
   `libraries/Task4_FaultFsm/src/PinMap.h`.
5. Open `firmware/MainProject/MainProject.ino`, select an ESP32 board, upload.

Scaling the pack: change `NUM_CELLS` in `libraries/Task1_BMSEngine/src/BMSConfig.h`
(4 → 16 needs no other edit; `TELEMETRY_MAX_CELLS` is already 16).

## Run the host-side tests (no hardware needed)

```
sh tests/host/run_tests.sh
```
Needs `g++` with C++17. Last saved output: `tests/host/last_run_output.txt`.

## Demo controls (Serial Monitor, 115200 baud)

| Key | Effect |
|-----|--------|
| `f` | inject a CELL fault for ~4 s |
| `a` | freeze ADC channel 1 for ~5 s |
| `n` | toggle a simulated network outage (exercises the offline queue) |
| `r` | operator reset out of SHUTDOWN |

## Blynk virtual pins

Defined once in `PinMap.h`. Task 5 owns V0–V2, V7, V14–V18 (live/queued data);
Task 6 owns V3–V6, V8–V13 (analytics). Neither writes the other's pins.

## Verification status

| Area | Status |
|------|--------|
| Task 1 BMS logic | host-tested (weakest/strongest, imbalance, adaptive threshold, trend) |
| Task 2 relay + sensor monitor | host-tested incl. 20k-step fault-injection stress |
| Task 4 FSM + fault detectors | host-tested incl. verified recovery, SHUTDOWN, manual reset |
| Task 5 offline queue | host-tested (FIFO order, overflow policy) |
| Task 6 risk engine | host-tested (score, severity, suggestions) |
| Task 3 LCD engine | **not tested** (needs LiquidCrystal_I2C / hardware) |
| Task 5 Wi-Fi manager, telemetry engine | **not compiled or tested** |
| Task 6 Blynk dashboard | **not compiled or tested** |
| `MainProject.ino` integration | **not compiled or tested** |
| Behaviour on real ESP32 hardware / real Blynk cloud | **not verified** |

See `docs/Bug_Log.md` for known limitations.

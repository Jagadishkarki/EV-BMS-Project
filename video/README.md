# Demonstration video

**No video is included.** It has to be recorded by you, showing your own
hardware (or the required simulator) running.

Check your assignment document for exact requirements (length, filename,
format, required content). A sensible structure for this project:

1. Project overview and repository tour (README, folder layout)
2. Architecture: the six libraries and how `MainProject.ino` ties them together
3. Live demo: normal operation, LCD page rotation
4. Fault injection (`f`, `a`): DEGRADED → FAILSAFE → verified recovery; relay trip and timed recovery
5. Network outage (`n`): offline queue filling, then replay in order on reconnect
6. Blynk dashboards: live data, queue depth, Wi-Fi health, risk score, fault history
7. Running `sh tests/host/run_tests.sh` and explaining what it does and does not cover
8. Challenges and limitations (see `docs/Bug_Log.md`)

Record at a resolution where Serial output and dashboard text are readable.

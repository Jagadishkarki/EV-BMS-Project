#!/bin/sh
# Builds and runs ALL host-side logic tests (Tasks 1, 2, 4, 5-queue, 6-risk).
# Requires g++ (C++17). Verifies LOGIC only - not Wi-Fi, Blynk, LCD, GPIO or
# real-time behaviour on the ESP32.
set -e
cd "$(dirname "$0")"
P=../../libraries
INC="-Istub -I$P/Task1_BMSEngine/src -I$P/Task2_SafetyRelay/src -I$P/Task4_FaultFsm/src -I$P/Task5_Telemetry/src -I$P/Task6_Analytics/src"

echo "######## Suite A: Tasks 1, 4, 5(queue), 6(risk) ########"
g++ -std=c++17 -Wall -Wextra $INC test_main.cpp $P/Task1_BMSEngine/src/BMSEngine.cpp \
  $P/Task4_FaultFsm/src/Fsm.cpp $P/Task4_FaultFsm/src/FaultManager.cpp \
  $P/Task5_Telemetry/src/OfflineQueue.cpp $P/Task6_Analytics/src/RiskEngine.cpp -o /tmp/bms_suite_a 2>/dev/null
/tmp/bms_suite_a | grep -v '^\[FSM\]'

echo
echo "######## Suite B: Task 2 (SensorMonitor, SafetyRelay) ########"
g++ -std=c++17 -Wall -Wextra $INC test_task2.cpp $P/Task2_SafetyRelay/src/SensorMonitor.cpp \
  $P/Task2_SafetyRelay/src/SafetyRelay.cpp -o /tmp/bms_suite_b 2>/dev/null
/tmp/bms_suite_b | grep -v '^\[RELAY\]'

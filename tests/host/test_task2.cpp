#include "Arduino.h"
FakeSerial Serial; int g_lastPinLevel=-1;
#include "SensorMonitor.h"
#include "SafetyRelay.h"
static int pass=0, fail=0;
#define CHECK(c,msg) do{ if(c){pass++;printf("  PASS: %s\n",msg);} else {fail++;printf("  FAIL: %s\n",msg);} }while(0)

// Feed N baseline samples at 10 ms spacing with tiny deterministic noise.
static uint32_t warm(SensorMonitor& s, uint32_t t, int n, float base){
  for(int i=0;i<n;i++){ t+=10; s.ingest(base+((i%3)-1)*0.05f,t);} return t; }

int main(){
  printf("\n[SensorMonitor]\n");
  { SensorMonitor s; uint32_t t=warm(s,0,12,5.0f);
    float c=s.ingest(999.0f,t+10);
    CHECK(s.getLastAnomaly()==SensorAnomaly::OUT_OF_RANGE,"out-of-range flagged");
    CHECK(c<10.0f,"out-of-range sample rejected (last good value returned)"); }
  { SensorMonitor s; uint32_t t=warm(s,0,12,5.0f);
    s.ingest(5.0f+60.0f*0.0f+30.0f,t+10); // +30 A in 10 ms = 3 A/ms >> 0.5 A/ms limit
    CHECK(s.getLastAnomaly()==SensorAnomaly::UNREALISTIC_JUMP,"physically impossible jump flagged"); }
  { SensorMonitor s; uint32_t t=0; bool frozen=false;
    for(int i=0;i<(int)(SENSOR_FROZEN_WINDOW_MS/10)+50;i++){ t+=10; s.ingest(7.0f,t); if(s.getLastAnomaly()==SensorAnomaly::FROZEN) frozen=true; }
    CHECK(frozen,"frozen sensor flagged after SENSOR_FROZEN_WINDOW_MS"); }
  { SensorMonitor s; uint32_t t=warm(s,0,12,5.0f);
    float c=s.ingest(8.0f,t+10); // within slew (3A/10ms=0.3A/ms) but a statistical outlier, single sample
    bool noFault=!s.hasFault();
    CHECK(noFault,"single outlier is NOT a hardware fault");
    CHECK(c<6.0f,"single outlier filtered (window mean used, not the spike)"); }
  { SensorMonitor s; uint32_t t=warm(s,0,12,5.0f); float last=0;
    for(int i=0;i<PERSIST_CONFIRM+2;i++){ t+=10; last=s.ingest(8.0f,t);} 
    CHECK(!s.hasFault(),"sustained step is not a fault");
    CHECK(last>7.5f,"sustained step accepted as genuine load change"); }

  printf("\n[SafetyRelay]\n");
  { SafetyRelay r; r.begin(); uint32_t t=1000;
    r.update(5.0f,SensorAnomaly::NONE,t);
    CHECK(r.getState()==RelayState::NORMAL&&r.isRelayClosed(),"starts NORMAL, relay closed");
    r.update(30.0f,SensorAnomaly::NONE,t+=10);
    CHECK(r.getState()==RelayState::FAULT_DEBOUNCE&&g_lastPinLevel==HIGH,"overcurrent -> debounce, relay still closed");
    r.update(5.0f,SensorAnomaly::NONE,t+=50);
    CHECK(r.getState()==RelayState::NORMAL,"glitch shorter than debounce never trips relay (anti-chatter)");
    r.update(30.0f,SensorAnomaly::NONE,t+=10);
    r.update(30.0f,SensorAnomaly::NONE,t+=FAULT_DEBOUNCE_MS+1);
    CHECK(r.getState()==RelayState::TRIPPED&&g_lastPinLevel==LOW,"sustained overcurrent -> TRIPPED, relay OPEN");
    r.update(23.0f,SensorAnomaly::NONE,t+=10);
    CHECK(r.getState()==RelayState::TRIPPED,"hysteresis: 23A (below trip, above reset) does NOT start recovery");
    r.update(5.0f,SensorAnomaly::NONE,t+=10);
    CHECK(r.getState()==RelayState::RECOVERY_HOLD&&g_lastPinLevel==LOW,"below reset -> RECOVERY_HOLD, relay still open");
    r.update(30.0f,SensorAnomaly::NONE,t+=100);
    r.update(5.0f,SensorAnomaly::NONE,t+=10); // not enough: expect fault during hold bumps back to TRIPPED
    r.update(5.0f,SensorAnomaly::NONE,t+=10);
    CHECK(r.getState()!=RelayState::NORMAL,"re-fault during recovery prevents return to NORMAL");
    // run a clean recovery start to finish
    uint32_t tt=t+10; r.update(5.0f,SensorAnomaly::NONE,tt);
    for(int i=0;i<10 && r.getState()!=RelayState::RECOVERY_HOLD;i++) r.update(5.0f,SensorAnomaly::NONE,tt+=10);
    r.update(5.0f,SensorAnomaly::NONE,tt+=RECOVERY_HOLD_MS+1);
    CHECK(r.getState()==RelayState::RECOVERY_VERIFY&&g_lastPinLevel==LOW,"hold elapsed -> RECOVERY_VERIFY, relay still open");
    r.update(5.0f,SensorAnomaly::NONE,tt+=RECOVERY_VERIFY_MS+1);
    CHECK(r.getState()==RelayState::NORMAL&&g_lastPinLevel==HIGH,"verify elapsed -> NORMAL, relay re-closed");
    CHECK(r.getLogCount()>=5,"transitions logged"); }
  { SafetyRelay r; r.begin(); uint32_t t=0;
    r.update(5.0f,SensorAnomaly::FROZEN,t+=10); r.update(5.0f,SensorAnomaly::FROZEN,t+=FAULT_DEBOUNCE_MS+1);
    CHECK(r.getState()==RelayState::TRIPPED&&r.getLastReason()==TripReason::SENSOR_FROZEN,"sensor anomaly trips relay with correct reason"); }
  { // stress: alternate fault/clear every few ms for a long time; machine must never wedge
    SafetyRelay r; r.begin(); uint32_t t=0; bool ok=true;
    for(int i=0;i<20000;i++){ t+=7; float v=((i/13)%2)?30.0f:5.0f; r.update(v,SensorAnomaly::NONE,t);
      RelayState s=r.getState(); if((int)s<0||(int)s>4) ok=false; }
    CHECK(ok,"20k-step fault-injection stress: state always valid");
    for(int i=0;i<3000;i++){ t+=10; r.update(5.0f,SensorAnomaly::NONE,t); }
    CHECK(r.getState()==RelayState::NORMAL,"after stress, clean input always returns to NORMAL (no deadlock)"); }
  printf("\n==== Task 2: %d passed, %d failed ====\n",pass,fail); return fail?1:0; }

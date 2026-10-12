// Host-side unit tests for the hardware-independent logic (Tasks 1, 4, 5-queue, 6-risk).
// These verify LOGIC only. They do not exercise Wi-Fi, Blynk, LCD, relay GPIO or timing on real hardware.
#include "Arduino.h"
FakeSerial Serial;
#include "Fsm.h"
#include "FaultManager.h"
#include "OfflineQueue.h"
#include "BMSEngine.h"
#include "RiskEngine.h"
#include <string.h>
static int pass=0, fail=0;
#define CHECK(c,msg) do{ if(c){pass++;printf("  PASS: %s\n",msg);} else {fail++;printf("  FAIL: %s\n",msg);} }while(0)

void testFsm(){
  printf("\n[FSM] NORMAL->DEGRADED->NORMAL with fault isolation\n");
  Fsm f; f.begin(0);
  f.tick(false,FaultSource::NONE,0,100);
  CHECK(f.getState()==OpState::NORMAL,"starts NORMAL");
  f.tick(true,FaultSource::ADC,101,200);
  CHECK(f.getState()==OpState::DEGRADED,"fault -> DEGRADED");
  CHECK(f.getFaultSource()==FaultSource::ADC && f.getFaultId()==101,"fault source/id isolated and retained");
  f.tick(false,FaultSource::NONE,0,500);
  CHECK(f.getState()==OpState::DEGRADED,"clear before min dwell stays DEGRADED (anti-cycling)");
  f.tick(false,FaultSource::NONE,0,1300);
  CHECK(f.getState()==OpState::NORMAL,"clear after min dwell -> NORMAL");
  CHECK(f.getFaultSource()==FaultSource::NONE,"fault source cleared on recovery");

  printf("\n[FSM] persistent fault escalates; FAILSAFE recovery is verified, not instant\n");
  Fsm g; g.begin(0);
  g.tick(true,FaultSource::CELL,401,1000);
  g.tick(true,FaultSource::CELL,401,1000+DEGRADED_ESCALATION_MS);
  CHECK(g.getState()==OpState::FAILSAFE,"persistent fault -> FAILSAFE");
  CHECK(g.getFailsafeStrikes()==0,"entering FAILSAFE is not itself a strike");
  uint32_t t=1000+DEGRADED_ESCALATION_MS;
  g.tick(false,FaultSource::NONE,0,t+1);
  g.tick(false,FaultSource::NONE,0,t+1+FAILSAFE_HOLD_MS+1);
  CHECK(g.getState()==OpState::FAILSAFE,"after hold still FAILSAFE (verifying, not instant NORMAL)");
  g.tick(false,FaultSource::NONE,0,t+1+FAILSAFE_HOLD_MS+1+VERIFY_MS+1);
  CHECK(g.getState()==OpState::NORMAL,"verification passed -> NORMAL");

  printf("\n[FSM] fault during verification restarts the timer (no premature recovery)\n");
  Fsm v; v.begin(0); uint32_t u=0;
  v.tick(true,FaultSource::CELL,401,u+=10); v.tick(true,FaultSource::CELL,401,u+=DEGRADED_ESCALATION_MS);
  v.tick(false,FaultSource::NONE,0,u+=10);
  v.tick(false,FaultSource::NONE,0,u+=FAILSAFE_HOLD_MS+5);   // enter VERIFY
  v.tick(true,FaultSource::CELL,401,u+=VERIFY_MS/2);          // fault returns mid-verify
  v.tick(false,FaultSource::NONE,0,u+=10);
  v.tick(false,FaultSource::NONE,0,u+=VERIFY_MS-100);         // less than a full hold since restart
  CHECK(v.getState()==OpState::FAILSAFE,"fault mid-verification prevents early NORMAL");

  printf("\n[FSM] repeated failed recoveries -> SHUTDOWN, only manual reset exits\n");
  Fsm h; h.begin(0); uint32_t n=0;
  h.tick(true,FaultSource::RELAY,200,n+=10);
  h.tick(true,FaultSource::RELAY,200,n+=DEGRADED_ESCALATION_MS);
  CHECK(h.getState()==OpState::FAILSAFE,"escalated to FAILSAFE");
  h.tick(true,FaultSource::RELAY,200,n+=100);
  CHECK(h.getState()==OpState::FAILSAFE && h.getFailsafeStrikes()==0,"fault that merely persists is NOT a strike");
  for(int i=0;i<SHUTDOWN_STRIKE_LIMIT+1 && h.getState()==OpState::FAILSAFE;i++){
    h.tick(false,FaultSource::NONE,0,n+=100);   // fault clears
    h.tick(true,FaultSource::RELAY,200,n+=100); // fault returns -> one failed recovery
  }
  CHECK(h.getState()==OpState::SHUTDOWN,"repeated failed recoveries -> SHUTDOWN");
  h.tick(false,FaultSource::NONE,0,n+=100000);
  CHECK(h.getState()==OpState::SHUTDOWN,"SHUTDOWN does not auto-exit");
  h.reset(n+=1);
  CHECK(h.getState()==OpState::NORMAL,"explicit reset() -> NORMAL (deadlock-free)");
  Fsm k; k.begin(0); k.reset(5);
  CHECK(k.getState()==OpState::NORMAL,"reset() outside SHUTDOWN is a no-op");
  const FsmLogEntry& e=h.getLogEntry(0);
  CHECK(e.prevState==OpState::SHUTDOWN && e.newState==OpState::NORMAL,"log records previous/new state");
  CHECK(h.getLogCount()>=3,"every transition logged");
}

void testFaultMgr(){
  printf("\n[FaultManager] frozen ADC / relay mismatch / comm timeout\n");
  FaultManager m; m.begin(2); FaultSource s; uint16_t id;
  for(uint32_t t=0;t<=ADC_FROZEN_MS+100;t+=100){ m.updateAdcChannel(0,1000+(t/100)%7,t); m.updateAdcChannel(1,2048,t); }
  CHECK(m.hasActiveFault(s,id)&&s==FaultSource::ADC&&id==101,"frozen ADC ch1 detected (id 101)");
  m.updateAdcChannel(1,2100,ADC_FROZEN_MS+200);
  CHECK(!m.hasActiveFault(s,id),"ADC fault clears when value moves");
  FaultManager r; r.begin(1);
  r.updateRelayFeedback(true,true,0);
  for(uint32_t t=10;t<=RELAY_SETTLE_MS+RELAY_MISMATCH_MS+400;t+=10) r.updateRelayFeedback(true,false,t);
  CHECK(r.hasActiveFault(s,id)&&s==FaultSource::RELAY,"relay mismatch detected");
  FaultManager c; c.begin(1); c.noteCommMessageReceived(0); c.checkCommTimeout(COMM_TIMEOUT_MS+1);
  CHECK(c.hasActiveFault(s,id)&&s==FaultSource::COMM,"comm timeout detected");
  FaultManager p; p.begin(1); p.reportExternalFault(FaultSource::CELL,401,true); p.noteCommMessageReceived(0); p.checkCommTimeout(COMM_TIMEOUT_MS+1);
  CHECK(p.hasActiveFault(s,id)&&s==FaultSource::CELL,"priority: CELL outranks COMM when both active");
}

void testQueue(){
  printf("\n[OfflineQueue] FIFO order + overflow policy\n");
  OfflineQueue q; q.begin(); TelemetryEvent e; memset(&e,0,sizeof e);
  for(uint8_t i=0;i<5;i++){ e.timestampMs=i; q.push(e);}
  TelemetryEvent o; bool ordered=true;
  for(uint8_t i=0;i<5;i++){ q.pop(o); if(o.timestampMs!=i) ordered=false; }
  CHECK(ordered && q.isEmpty(),"events dequeue in insertion order");
  for(uint32_t i=0;i<TELEMETRY_QUEUE_CAPACITY+6;i++){ e.timestampMs=i; q.push(e);}
  CHECK(q.count()==TELEMETRY_QUEUE_CAPACITY,"count capped at fixed capacity");
  CHECK(q.droppedCount()==6,"6 oldest dropped on overflow");
  q.pop(o);
  CHECK(o.timestampMs==6,"oldest surviving event is #6 (newest data kept)");
}

void testBms(){
  printf("\n[BMSEngine] weakest/strongest, imbalance, adaptive threshold, trend\n");
  BMSEngine b; b.begin(); float v[NUM_CELLS]; for(uint8_t i=0;i<NUM_CELLS;i++) v[i]=3.70f+i*0.01f;
  v[2]=3.55f;
  b.update(v,0.5f,0.0f,0);
  CHECK(b.getWeakestIndex()==2,"weakest cell identified");
  CHECK(b.getStrongestIndex()==NUM_CELLS-1,"strongest cell identified");
  CHECK(b.getImbalance()>0.17f && b.getImbalance()<0.19f,"imbalance = max-min (3.73-3.55)");
  float mid=b.getAdaptiveThreshold();
  b.update(v,0.98f,0.0f,1); float hiSoc=b.getAdaptiveThreshold();
  b.update(v,0.5f,20.0f,2); float hiLoad=b.getAdaptiveThreshold();
  CHECK(hiSoc<mid,"threshold tightens near full SoC");
  CHECK(hiLoad>mid,"threshold relaxes under heavy discharge current");
  BMSEngine t; t.begin(); float w[NUM_CELLS]; for(uint8_t i=0;i<NUM_CELLS;i++) w[i]=3.7f;
  for(int i=0;i<IMBALANCE_HISTORY_LEN;i++){ w[0]=3.7f-0.01f*i; t.update(w,0.5f,0,i);}
  CHECK(t.getImbalanceTrend()==1,"widening imbalance -> trend +1");
  BMSEngine d; d.begin();
  for(int i=0;i<IMBALANCE_HISTORY_LEN;i++){ w[0]=3.7f-0.01f*(IMBALANCE_HISTORY_LEN-i); d.update(w,0.5f,0,i);}
  CHECK(d.getImbalanceTrend()==-1,"narrowing imbalance -> trend -1");
}

void testRisk(){
  printf("\n[RiskEngine] score & severity track FSM state\n");
  RiskEngine r; RiskInputs in{0.01f,0.05f,0,0.5f,0,OpState::NORMAL,FaultSource::NONE};
  RiskResult a=r.evaluate(in); CHECK(a.severity==Severity::OK&&a.score<15,"healthy pack -> OK, low score");
  in.state=OpState::FAILSAFE; in.faultSource=FaultSource::CELL;
  RiskResult b=r.evaluate(in); CHECK(b.severity==Severity::CRITICAL&&strstr(b.suggestion,"cell"),"FAILSAFE+CELL -> CRITICAL, cell-specific suggestion");
  in.state=OpState::SHUTDOWN; RiskResult c=r.evaluate(in); CHECK(c.severity==Severity::CRITICAL&&strstr(c.suggestion,"SHUTDOWN"),"SHUTDOWN -> CRITICAL, shutdown suggestion");
  in.state=OpState::DEGRADED; RiskResult d=r.evaluate(in); CHECK(d.severity==Severity::WARNING,"DEGRADED -> WARNING");
  RiskInputs hi{0.09f,0.05f,1,0.97f,4,OpState::NORMAL,FaultSource::NONE};
  CHECK(r.evaluate(hi).score>a.score,"worse imbalance/faults/SoC -> higher score");
}
int main(){ testFsm(); testFaultMgr(); testQueue(); testBms(); testRisk();
  printf("\n==== %d passed, %d failed ====\n",pass,fail); return fail?1:0; }

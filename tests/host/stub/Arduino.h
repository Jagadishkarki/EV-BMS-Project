#pragma once
// Minimal host-side stand-in for Arduino.h so hardware-independent modules
// can be compiled and unit-tested on a PC. NOT used on the ESP32.
#include <stdint.h>
#include <stdio.h>
#define F(x) x
struct FakeSerial {
  void print(const char* s){printf("%s",s);} void print(unsigned long v){printf("%lu",v);}
  void print(int v){printf("%d",v);} void print(unsigned v){printf("%u",v);}
  void println(const char* s){printf("%s\n",s);} void println(unsigned v){printf("%u\n",v);}
  void println(int v){printf("%d\n",v);}
};
extern FakeSerial Serial;
// GPIO stubs (record last relay level so tests can assert the physical output)
#define OUTPUT 1
#define HIGH 1
#define LOW 0
extern int g_lastPinLevel;
inline void pinMode(int,int){}
inline void digitalWrite(int,int v){ g_lastPinLevel=v; }

#include "Core.h"
#include <cassert>
#include <iostream>
using namespace hud;
int main(){
  Telemetry t; CanFrame speed{0x7E8,4,{3,0x41,0x0D,88},100};assert(decodeObd(speed,t)&&t.speedKph==88);
  CanFrame rpm{0x7E8,5,{4,0x41,0x0C,0x1F,0x40},100};assert(decodeObd(rpm,t)&&t.rpm==2000);
  CanFrame temp{0x7E8,4,{3,0x41,0x05,100},100};assert(decodeObd(temp,t)&&t.coolantC==60);
  assert(obdDescription(speed)=="Speed 88 km/h");assert(obdDescription(rpm)=="RPM 2000");
  assert(frameMatches(speed,"7e8"));assert(frameMatches(speed,"41 0D"));assert(!frameMatches(speed,"7DF"));
  assert(validateWidget({"speed",0,0,100,60,true}));assert(!validateWidget({"bad",450,0,100,60,true}));
  TemporaryAccess g;g.arm(1000);assert(g.joinable(1001));assert(!g.claim(1002,0));assert(g.claim(1002,0x01020304));assert(g.authorized(1003,0x01020304));assert(!g.authorized(1003,0x05060708));assert(!g.authorized(1002+TemporaryAccess::SESSION_MS,0x01020304));g.revoke();assert(!g.active(1004));
  TemporaryAccess wrap;wrap.arm(0xfffffff0u);assert(wrap.joinable(5));
  std::cout<<"All core tests passed\n";
}

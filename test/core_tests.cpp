#include "Core.h"
#include "ObdSimulator.h"
#include <cassert>
#include <iostream>
using namespace hud;
int main(){
  Telemetry t; CanFrame speed{0x7E8,4,{3,0x41,0x0D,88},100};assert(decodeObd(speed,t)&&t.speedKph==88);
  CanFrame rpm{0x7E8,5,{4,0x41,0x0C,0x1F,0x40},100};assert(decodeObd(rpm,t)&&t.rpm==2000);
  CanFrame temp{0x7E8,4,{3,0x41,0x05,100},100};assert(decodeObd(temp,t)&&t.coolantC==60);
  assert(obdDescription(speed)=="Speed 88 km/h");assert(obdDescription(rpm)=="RPM 2000");
  Telemetry simulated;int decoded=0;for(uint8_t i=0;i<10;i++){auto f=simulatedFrame(1000+i*100,i);decoded+=decodeObd(f,simulated);}assert(decoded>=5&&simulated.speedKph>0&&simulated.rpm>0&&simulated.coolantC>0&&simulated.engineLoad>0);
  assert(frameMatches(speed,"7e8"));assert(frameMatches(speed,"41 0D"));assert(!frameMatches(speed,"7DF"));
  assert(frameTypeKey(temp)=="7E8:05");float value,realMin,realMax;const char*unit;assert(frameMetric(temp,value,realMin,realMax,unit));assert(value==60&&realMin==-40&&realMax==215&&std::string(unit)=="C");
  CanFrame volts{0x7E8,5,{4,0x41,0x42,0x30,0x39},200};assert(frameMetric(volts,value,realMin,realMax,unit));assert(value>12.34f&&value<12.36f&&std::string(unit)=="V");
  auto fuelTrim=findPid(0x06);uint8_t neutralTrim[]={128};assert(fuelTrim&&decodePidValue(*fuelTrim,neutralTrim,1,value)&&value==0);
  assert(validateWidget({"speed",0,0,100,60,true}));assert(!validateWidget({"bad",450,0,100,60,true}));
  TemporaryAccess g;g.arm(1000);assert(g.joinable(1001));assert(!g.claim(1002,0));assert(g.claim(1002,0x01020304));assert(g.authorized(1003,0x01020304));assert(!g.authorized(1003,0x05060708));assert(!g.authorized(1002+TemporaryAccess::SESSION_MS,0x01020304));g.revoke();assert(!g.active(1004));
  TemporaryAccess wrap;wrap.arm(0xfffffff0u);assert(wrap.joinable(5));
  std::cout<<"All core tests passed\n";
}

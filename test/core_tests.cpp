#include "Core.h"
#include "ObdSimulator.h"
#include "FrameAnalytics.h"
#include <cassert>
#include <iostream>
using namespace hud;
int main(){
  static_assert(FRAME_HISTORY_MS==120000);static_assert(FRAME_HISTORY_CAPACITY>=120000);
  Telemetry t; CanFrame speed{0x7E8,4,{3,0x41,0x0D,88},100};assert(decodeObd(speed,t)&&t.speedKph==88);
  CanFrame rpm{0x7E8,5,{4,0x41,0x0C,0x1F,0x40},100};assert(decodeObd(rpm,t)&&t.rpm==2000);
  CanFrame temp{0x7E8,4,{3,0x41,0x05,100},100};assert(decodeObd(temp,t)&&t.coolantC==60);
  assert(obdDescription(speed)=="Speed 88 km/h");assert(obdDescription(rpm)=="RPM 2000");
  Telemetry simulated;int decoded=0,rawFrames=0;for(uint32_t i=0;i<200;i++){auto f=simulatedFrame(1000+i,i);decoded+=decodeObd(f,simulated);rawFrames+=f.id!=0x7E8;}assert(decoded>=8&&rawFrames==190&&simulated.speedKph>0&&simulated.rpm>0&&simulated.coolantC>0&&simulated.engineLoad>0);assert(SIMULATED_CAN_FPS==100&&SIMULATED_CAN_PERIOD_US==10000);
  assert(frameMatches(speed,"7e8"));assert(frameMatches(speed,"41 0D"));assert(!frameMatches(speed,"7DF"));
  assert(frameTypeKey(temp)=="7E8:05");float value=0,realMin=0,realMax=0;const char*unit=nullptr;assert(frameMetric(temp,value,realMin,realMax,unit));assert(value==60&&realMin==-40&&realMax==215&&std::string(unit)=="C");
  CanFrame volts{0x7E8,5,{4,0x41,0x42,0x30,0x39},200};assert(frameMetric(volts,value,realMin,realMax,unit));assert(value>12.34f&&value<12.36f&&std::string(unit)=="V");
  FrameAnalytics analytics;analytics.record(speed);analytics.record(CanFrame{0x7E8,4,{3,0x41,0x0D,90},1100});auto speedStats=analytics.find("7E8:0D");assert(speedStats);auto summary=analytics.summarize(*speedStats,1100);assert(summary.count==2&&summary.metric&&summary.minimum==88&&summary.maximum==90&&summary.latest==90);analytics.record(CanFrame{0x7E8,4,{3,0x41,0x0D,91},FRAME_HISTORY_MS+2200});summary=analytics.summarize(*speedStats,FRAME_HISTORY_MS+2200);assert(summary.count==1&&summary.latest==91);
  assert(decodeObd(volts,t)&&t.controlVoltage>12.34f&&t.controlVoltage<12.36f);
  CanFrame throttle{0x7E8,4,{3,0x41,0x11,128},210};assert(decodeObd(throttle,t)&&t.throttlePct>50.1f&&t.throttlePct<50.3f);
  CanFrame intake{0x7E8,4,{3,0x41,0x0F,70},220};assert(decodeObd(intake,t)&&t.intakeTempC==30);
  CanFrame ambient{0x7E8,4,{3,0x41,0x46,65},230};assert(decodeObd(ambient,t)&&t.ambientTempC==25);
  CanFrame fuelRate{0x7E8,5,{4,0x41,0x5E,0,100},240};assert(decodeObd(fuelRate,t)&&t.fuelRateLph==5);
  auto fuelTrim=findPid(0x06);uint8_t neutralTrim[]={128};assert(fuelTrim&&decodePidValue(*fuelTrim,neutralTrim,1,value)&&value==0);
  assert(validateWidget({"speed",0,0,100,60,true}));assert(!validateWidget({"bad",450,0,100,60,true}));
  float widgetMin=0,widgetMax=0;assert(widgetRange("speed",widgetMin,widgetMax)&&widgetMin==0&&widgetMax==240);assert(widgetRange("coolant",widgetMin,widgetMax)&&widgetMin==-40&&widgetMax==215);assert(!widgetRange("date",widgetMin,widgetMax));
  Widget styled{"rpm",0,0,100,60,true};styled.visualMin=500;styled.visualMax=7000;styled.lightLow=2500;styled.lightHigh=4500;assert(widgetVisualRange(styled,widgetMin,widgetMax)&&widgetMin==500&&widgetMax==7000);float lightLow,lightHigh;widgetLightLimits(styled,widgetMin,widgetMax,lightLow,lightHigh);assert(lightLow==2500&&lightHigh==4500);styled.visualMin=styled.visualMax=styled.lightLow=styled.lightHigh=0;assert(widgetVisualRange(styled,widgetMin,widgetMax)&&widgetMax==8000);widgetLightLimits(styled,widgetMin,widgetMax,lightLow,lightHigh);assert(lightLow>2666&&lightLow<2667&&lightHigh>5333&&lightHigh<5334);
  TemporaryAccess g;g.arm(1000);assert(g.joinable(1001));assert(!g.claim(1002,0));assert(g.claim(1002,0x01020304));assert(g.authorized(1003,0x01020304));assert(!g.authorized(1003,0x05060708));assert(!g.authorized(1002+TemporaryAccess::SESSION_MS,0x01020304));g.revoke();assert(!g.active(1004));
  TemporaryAccess wrap;wrap.arm(0xfffffff0u);assert(wrap.joinable(5));
  std::cout<<"All core tests passed\n";
}

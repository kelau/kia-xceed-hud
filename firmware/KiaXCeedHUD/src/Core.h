#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <array>
#include "StandardPids.h"

namespace hud {
inline constexpr const char* FIRMWARE_VERSION="0.10.1";
struct CanFrame { uint32_t id=0; uint8_t dlc=0; std::array<uint8_t,8> data{}; uint32_t ms=0; };
struct Telemetry {
  float speedKph=0, rpm=0, coolantC=0, soc=0, engineLoad=0;
  float intakeTempC=0, throttlePct=0, controlVoltage=0, ambientTempC=0, fuelRateLph=0;
  float gpsSpeedKph=0, latitude=0, longitude=0, tripKm=0;
  float accelLong=0, accelLat=0; bool gpsFix=false; uint32_t lastCanMs=0;
};
enum WidgetVisualStyle:uint8_t { WIDGET_VALUE=0,WIDGET_BAR=1,WIDGET_GAUGE=2,WIDGET_LIGHT=3 };
struct Widget { std::string id; int16_t x=0,y=0,w=100,h=70; bool visible=true; char title[32]{}; uint8_t fontSize=18; uint32_t background=0x102a38,textColor=0xffffff; uint8_t backgroundOpacity=255; bool border=true; uint8_t decimals=255,valueAlign=1,timeFormat=0,visualStyle=WIDGET_VALUE; };

inline const char* widgetDefaultTitle(const std::string&id){if(id=="speed")return "Vehicle speed";if(id=="soc")return "Level / SOC";if(id=="power")return "Power / RPM";if(id=="trip")return "Trip distance";if(id=="coolant")return "Coolant";if(id=="status")return "System status";if(id=="rpm")return "Engine RPM";if(id=="load")return "Engine load";if(id=="gpsSpeed")return "GPS speed";if(id=="accel")return "Acceleration";if(id=="canAge")return "CAN age";if(id=="coordinates")return "Coordinates";if(id=="version")return "Firmware version";if(id=="time")return "Time";if(id=="date")return "Date";if(id=="gpsLock")return "GPS lock";if(id=="uptime")return "System uptime";if(id=="wifi")return "Wi-Fi address";if(id=="intakeTemp")return "Intake temperature";if(id=="throttle")return "Throttle position";if(id=="voltage")return "Control voltage";if(id=="ambientTemp")return "Ambient temperature";if(id=="fuelRate")return "Fuel rate";if(id=="wifiSignal")return "Wi-Fi signal";if(id=="mode")return "Data source";if(id=="webAccess")return "Web access";return id.c_str();}
inline uint8_t widgetDefaultDecimals(const std::string&id){if(id=="soc"||id=="trip"||id=="load"||id=="gpsSpeed"||id=="throttle"||id=="voltage"||id=="fuelRate")return 1;if(id=="accel")return 2;if(id=="coordinates")return 5;return 0;}
inline bool widgetRange(const std::string&id,float&minimum,float&maximum){if(id=="speed"||id=="gpsSpeed"){minimum=0;maximum=240;}else if(id=="rpm"||id=="power"){minimum=0;maximum=8000;}else if(id=="soc"||id=="load"||id=="throttle"){minimum=0;maximum=100;}else if(id=="coolant"){minimum=-40;maximum=215;}else if(id=="intakeTemp"||id=="ambientTemp"){minimum=-40;maximum=120;}else if(id=="voltage"){minimum=0;maximum=20;}else if(id=="fuelRate"){minimum=0;maximum=50;}else if(id=="wifiSignal"){minimum=-100;maximum=-30;}else if(id=="canAge"){minimum=0;maximum=5000;}else return false;return true;}

inline int hexNibble(char c) { return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; }
inline bool frameMatches(const CanFrame& f, const std::string& query) {
  if(query.empty()) return true;
  char id[9]; snprintf(id,sizeof(id),"%03X",(unsigned)f.id);
  std::string hay=id;
  char b[4]; for(unsigned i=0;i<f.dlc;i++){snprintf(b,sizeof(b)," %02X",f.data[i]);hay+=b;}
  std::string q=query; for(char& c:q) if(c>='a'&&c<='f') c-=32;
  return hay.find(q)!=std::string::npos;
}
inline bool decodeObd(const CanFrame& f, Telemetry& t) {
  if(f.id<0x7E8||f.id>0x7EF||f.dlc<4||f.data[1]!=0x41) return false;
  switch(f.data[2]) {
    case 0x04: t.engineLoad=f.data[3]*100.0f/255.0f; return true;
    case 0x05: t.coolantC=(float)f.data[3]-40; return true;
    case 0x0C: if(f.dlc<5)return false; t.rpm=((f.data[3]<<8)|f.data[4])/4.0f; return true;
    case 0x0D: t.speedKph=f.data[3]; return true;
    case 0x0F: t.intakeTempC=(float)f.data[3]-40; return true;
    case 0x11: t.throttlePct=f.data[3]*100.0f/255.0f; return true;
    case 0x2F: t.soc=f.data[3]*100.0f/255.0f; return true;
    case 0x42: if(f.dlc<5)return false; t.controlVoltage=((f.data[3]<<8)|f.data[4])/1000.0f; return true;
    case 0x46: t.ambientTempC=(float)f.data[3]-40; return true;
    case 0x5E: if(f.dlc<5)return false; t.fuelRateLph=((f.data[3]<<8)|f.data[4])/20.0f; return true;
    default: return false;
  }
}
inline std::string obdDescription(const CanFrame& f) {
  if(f.id<0x7E8||f.id>0x7EF||f.dlc<4||f.data[1]!=0x41)return {};
  char out[48];switch(f.data[2]){
    case 0x04:snprintf(out,sizeof(out),"Load %.1f%%",f.data[3]*100.0/255.0);break;
    case 0x05:snprintf(out,sizeof(out),"Coolant %d C",(int)f.data[3]-40);break;
    case 0x0C:if(f.dlc<5)return {};snprintf(out,sizeof(out),"RPM %.0f",((f.data[3]<<8)|f.data[4])/4.0);break;
    case 0x0D:snprintf(out,sizeof(out),"Speed %u km/h",f.data[3]);break;
    case 0x2F:snprintf(out,sizeof(out),"Fuel input %.1f%%",f.data[3]*100.0/255.0);break;
    default:snprintf(out,sizeof(out),"Mode 01 PID %02X",f.data[2]);
  }return out;
}
inline const char* obdPidName(uint8_t pid){auto d=findPid(pid);return d?d->name:"Unknown Mode 01 PID";}
inline std::string frameTypeKey(const CanFrame&f){char key[16];if(f.id>=0x7E8&&f.id<=0x7EF&&f.dlc>=3&&f.data[1]==0x41)snprintf(key,sizeof(key),"%03X:%02X",(unsigned)f.id,f.data[2]);else snprintf(key,sizeof(key),"%03X",(unsigned)f.id);return key;}
inline bool frameMetric(const CanFrame&f,float&value,float&realMin,float&realMax,const char*&unit){if(f.id<0x7E8||f.id>0x7EF||f.dlc<4||f.data[1]!=0x41)return false;auto d=findPid(f.data[2]);if(!d||!decodePidValue(*d,&f.data[3],f.dlc-3,value))return false;realMin=d->minimum;realMax=d->maximum;unit=d->unit;return true;}
inline bool validateWidget(const Widget& w) {
  return !w.id.empty()&&w.x>=0&&w.y>=0&&w.w>=40&&w.h>=30&&w.x+w.w<=480&&w.y+w.h<=480;
}

class TemporaryAccess {
 public:
  static constexpr uint32_t JOIN_MS=300000, SESSION_MS=900000;
  void arm(uint32_t now){joinUntil_=now+JOIN_MS;sessionUntil_=0;client_=0;}
  bool joinable(uint32_t now)const{return client_==0&&before(now,joinUntil_);}
  bool claim(uint32_t now,uint32_t client){if(!client||!joinable(now))return false;client_=client;sessionUntil_=now+SESSION_MS;joinUntil_=0;return true;}
  bool authorized(uint32_t now,uint32_t client)const{return client&&client==client_&&before(now,sessionUntil_);}
  bool active(uint32_t now)const{return joinable(now)||authorized(now,client_);}
  uint32_t client()const{return client_;}
  void revoke(){joinUntil_=sessionUntil_=client_=0;}
 private:
  static bool before(uint32_t a,uint32_t b){return (int32_t)(b-a)>0;}
  uint32_t joinUntil_=0,sessionUntil_=0,client_=0;
};
}

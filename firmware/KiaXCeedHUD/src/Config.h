#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <new>
#include <LittleFS.h>
#include <Preferences.h>
#include "Core.h"

namespace hud {
struct Config {
  String hostname="kia-hud",ssid="",password="",otaSsid="",otaPassword="";
  bool canListenOnly=true,gpsEnabled=true,imuEnabled=true,simulateObd=false;
  uint16_t canRate=500;
  uint8_t brightness=80;
  char mainDisplayName[32]="Driving";
  std::array<Widget,40> widgets{{
    {"speed",20,20,210,150,true},{"soc",250,20,210,70,true},
    {"power",250,100,210,70,true},{"trip",20,190,210,80,true},
    {"coolant",250,190,210,80,true},{"status",20,290,440,120,true},
    {"rpm",20,20,210,80,false},{"load",250,190,210,80,false},
    {"gpsSpeed",20,190,210,80,false},{"accel",250,290,210,80,false},
    {"canAge",20,390,210,60,false},{"coordinates",250,390,210,60,false},
    {"version",300,420,160,45,false},{"time",20,20,180,65,false},
    {"date",210,20,250,65,false},{"gpsLock",20,95,180,60,false},
    {"uptime",210,95,180,60,false},{"wifi",20,165,260,60,false},
    {"intakeTemp",20,235,180,60,false},{"throttle",210,235,180,60,false},
    {"voltage",20,305,180,60,false},{"ambientTemp",210,305,180,60,false},
    {"fuelRate",20,375,180,60,false},{"wifiSignal",210,375,180,60,false},
    {"mode",20,420,180,50,false}
  }};
  std::array<VirtualDisplay,4> virtualDisplays{};

  Config(){
    initDisplay(virtualDisplays[0],"splash","Splash",CONDITION_BOOT,"",0,2500,true);
    virtualDisplays[0].widgets[0]={"text",40,155,400,110,true,"KIA",96,0,0,0xffffff,0,false};
    virtualDisplays[0].widgets[1]={"text",100,280,280,55,true,"by Kenneth",30,0,0,0xffffff,0,false};
    initDisplay(virtualDisplays[1],"stationary","Stationary",CONDITION_LTE,"speed",0,0,false);
    initDisplay(virtualDisplays[2],"temperature-alert","Temperature alert",CONDITION_GT,"coolant",200,0,false);
    virtualDisplays[2].widgets[0]={"text",30,120,420,80,true,"HIGH TEMPERATURE",48,0,0,0xef4444,0,false};
    virtualDisplays[2].widgets[1]={"coolant",90,220,300,140,true};
    initDisplay(virtualDisplays[3],"battery","Battery power",CONDITION_BATTERY,"batteryPower",0,0,true);
    virtualDisplays[3].widgets[0]={"text",30,80,420,70,true,"CAR POWER OFF",42,0,0,0xfbbf24,0,false};
    virtualDisplays[3].widgets[1]={"battery",90,190,300,100,true};
    virtualDisplays[3].widgets[2]={"time",90,310,300,80,true};
  }

  static void initDisplay(VirtualDisplay&d,const char*id,const char*name,DisplayCondition condition,const char*metric,float threshold,uint32_t timeout,bool enabled){
    strlcpy(d.id,id,sizeof(d.id));strlcpy(d.name,name,sizeof(d.name));strlcpy(d.metric,metric,sizeof(d.metric));d.condition=condition;d.threshold=threshold;d.timeoutMs=timeout;d.enabled=enabled;d.returnPrevious=true;
  }

  bool load(){
    String s;if(LittleFS.begin(true)){File f=LittleFS.open("/config.json","r");if(f){s=f.readString();f.close();}}
    if(!s.length()){Preferences p;if(p.begin("hud",true)){s=p.getString("config","");p.end();}}
    return s.length()?fromJson(s):false;
  }
  bool save()const{
    String s;toJson(s,true);if(!LittleFS.begin(true))return false;File f=LittleFS.open("/config.tmp","w");if(!f)return false;size_t written=f.print(s);f.close();if(written!=s.length()){LittleFS.remove("/config.tmp");return false;}LittleFS.remove("/config.json");return LittleFS.rename("/config.tmp","/config.json");
  }

  static void widgetToJson(JsonObject o,const Widget&w){
    o["id"]=w.id;o["x"]=w.x;o["y"]=w.y;o["w"]=w.w;o["h"]=w.h;o["visible"]=w.visible;o["title"]=w.title;o["fontSize"]=w.fontSize;o["fontFamily"]=w.fontFamily;o["background"]=w.background;o["textColor"]=w.textColor;o["backgroundOpacity"]=w.backgroundOpacity;o["border"]=w.border;o["decimals"]=w.decimals==255?widgetDefaultDecimals(w.id):w.decimals;o["valueAlign"]=w.valueAlign;o["timeFormat"]=w.timeFormat;o["visualStyle"]=w.visualStyle;o["visualMin"]=w.visualMin;o["visualMax"]=w.visualMax;o["lightLow"]=w.lightLow;o["lightHigh"]=w.lightHigh;o["trackColor"]=w.trackColor;o["accentColor"]=w.accentColor;o["lowColor"]=w.lowColor;o["midColor"]=w.midColor;o["highColor"]=w.highColor;o["visualThickness"]=w.visualThickness;o["subdivisions"]=w.subdivisions;o["resource"]=w.resource;o["beepEnabled"]=w.beepEnabled;o["beepWhen"]=w.beepWhen;o["beepThreshold"]=w.beepThreshold;o["beepTimeoutMs"]=w.beepTimeoutMs;o["showUnit"]=w.showUnit;
  }
  static bool widgetFromJson(JsonObjectConst o,Widget&w){
    w.id=std::string((const char*)(o["id"]|""));if(w.id.empty())return false;w.w=constrain((int)(o["w"]|100),40,480);w.h=constrain((int)(o["h"]|70),30,480);w.x=constrain((int)(o["x"]|0),0,480-w.w);w.y=constrain((int)(o["y"]|0),0,480-w.h);w.visible=o["visible"]|true;strlcpy(w.title,o["title"]|"",sizeof(w.title));w.fontSize=(uint16_t)constrain((int)(o["fontSize"]|18),10,400);w.fontFamily=(uint8_t)constrain((int)(o["fontFamily"]|0),0,2);w.background=(uint32_t)(o["background"]|0x102a38);w.textColor=(uint32_t)(o["textColor"]|0xffffff);w.backgroundOpacity=(uint8_t)constrain((int)(o["backgroundOpacity"]|255),0,255);w.border=o["border"]|true;w.decimals=(uint8_t)constrain((int)(o["decimals"]|widgetDefaultDecimals(w.id)),0,5);w.valueAlign=(uint8_t)constrain((int)(o["valueAlign"]|1),0,2);w.timeFormat=(uint8_t)constrain((int)(o["timeFormat"]|0),0,3);w.visualStyle=(uint8_t)constrain((int)(o["visualStyle"]|WIDGET_VALUE),0,3);w.visualMin=o["visualMin"]|0.0f;w.visualMax=o["visualMax"]|0.0f;w.lightLow=o["lightLow"]|0.0f;w.lightHigh=o["lightHigh"]|0.0f;w.trackColor=(uint32_t)(o["trackColor"]|0x1f3b4d);w.accentColor=(uint32_t)(o["accentColor"]|0x2dd4bf);w.lowColor=(uint32_t)(o["lowColor"]|0xef4444);w.midColor=(uint32_t)(o["midColor"]|0xf59e0b);w.highColor=(uint32_t)(o["highColor"]|0x22c55e);w.visualThickness=(uint8_t)constrain((int)(o["visualThickness"]|8),2,128);w.subdivisions=(uint8_t)constrain((int)(o["subdivisions"]|0),0,20);strlcpy(w.resource,o["resource"]|"",sizeof(w.resource));w.beepEnabled=o["beepEnabled"]|false;w.beepWhen=(uint8_t)constrain((int)(o["beepWhen"]|0),0,2);w.beepThreshold=o["beepThreshold"]|0.0f;w.beepTimeoutMs=(uint32_t)constrain((int)(o["beepTimeoutMs"]|5000),250,600000);w.showUnit=o["showUnit"]|true;return true;
  }
  static void widgetsToJson(JsonArray a,const std::array<Widget,40>&widgets){for(auto&w:widgets){if(w.id.empty())continue;widgetToJson(a.add<JsonObject>(),w);}}
  static bool widgetsFromJson(JsonArrayConst a,std::array<Widget,40>&widgets){for(auto&w:widgets){w.~Widget();new(&w)Widget();}int i=0;for(JsonObjectConst item:a){if(i>=40)break;if(!widgetFromJson(item,widgets[i]))return false;i++;}return true;}

  void toJson(String&out,bool includeSecrets=false)const{
    JsonDocument d;d["schemaVersion"]=CONFIG_SCHEMA_VERSION;d["hostname"]=hostname;d["ssid"]=ssid;d["otaSsid"]=otaSsid;d["otaPasswordSet"]=otaPassword.length()>0;if(includeSecrets){d["password"]=password;d["otaPassword"]=otaPassword;}d["canListenOnly"]=canListenOnly;d["simulateObd"]=simulateObd;d["gpsEnabled"]=gpsEnabled;d["imuEnabled"]=imuEnabled;d["brightness"]=brightness;d["mainDisplayName"]=mainDisplayName;widgetsToJson(d["widgets"].to<JsonArray>(),widgets);auto displays=d["virtualDisplays"].to<JsonArray>();for(auto&v:virtualDisplays){auto o=displays.add<JsonObject>();o["id"]=v.id;o["name"]=v.name;o["metric"]=v.metric;o["condition"]=v.condition;o["threshold"]=v.threshold;o["timeoutMs"]=v.timeoutMs;o["enabled"]=v.enabled;o["returnPrevious"]=v.returnPrevious;widgetsToJson(o["widgets"].to<JsonArray>(),v.widgets);}serializeJson(d,out);
  }
  bool fromJson(const String&s){
    JsonDocument d;if(deserializeJson(d,s))return false;hostname=d["hostname"]|hostname;ssid=d["ssid"]|ssid;const char*p=d["password"]|"";if(*p)password=p;otaSsid=d["otaSsid"]|otaSsid;const char*op=d["otaPassword"]|"";if(*op)otaPassword=op;canListenOnly=d["canListenOnly"]|true;simulateObd=d["simulateObd"]|false;gpsEnabled=d["gpsEnabled"]|true;imuEnabled=d["imuEnabled"]|true;brightness=constrain((int)(d["brightness"]|80),5,100);strlcpy(mainDisplayName,d["mainDisplayName"]|"Driving",sizeof(mainDisplayName));if(!widgetsFromJson(d["widgets"].as<JsonArrayConst>(),widgets))return false;
    JsonArrayConst displays=d["virtualDisplays"].as<JsonArrayConst>();int i=0;for(JsonObjectConst source:displays){if(i>=virtualDisplays.size())break;auto&v=virtualDisplays[i++];strlcpy(v.id,source["id"]|v.id,sizeof(v.id));strlcpy(v.name,source["name"]|v.name,sizeof(v.name));strlcpy(v.metric,source["metric"]|v.metric,sizeof(v.metric));v.condition=(DisplayCondition)constrain((int)(source["condition"]|(int)v.condition),0,7);v.threshold=source["threshold"]|v.threshold;v.timeoutMs=source["timeoutMs"]|v.timeoutMs;v.enabled=source["enabled"]|v.enabled;v.returnPrevious=source["returnPrevious"]|true;if(!source["widgets"].isNull()&&!widgetsFromJson(source["widgets"].as<JsonArrayConst>(),v.widgets))return false;}
    return true;
  }
};
}

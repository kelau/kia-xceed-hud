#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Core.h"
namespace hud {
struct Config {
  String hostname="kia-hud",ssid="",password="",otaSsid="",otaPassword="";
  bool canListenOnly=true,gpsEnabled=true,imuEnabled=true,simulateObd=false; uint16_t canRate=500;
  uint8_t brightness=80; std::array<Widget,26> widgets{{
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
    {"mode",20,420,180,50,false},{"webAccess",210,420,250,50,false,"",18,0x102a38,0xffffff,0,false}}};
  bool load(){String s;if(LittleFS.begin(true)){File f=LittleFS.open("/config.json","r");if(f){s=f.readString();f.close();}}if(!s.length()){Preferences p;if(p.begin("hud",true)){s=p.getString("config","");p.end();}}return s.length()?fromJson(s):false;}
  bool save()const{String s;toJson(s,true);if(!LittleFS.begin(true))return false;File f=LittleFS.open("/config.tmp","w");if(!f)return false;size_t written=f.print(s);f.close();if(written!=s.length()){LittleFS.remove("/config.tmp");return false;}LittleFS.remove("/config.json");return LittleFS.rename("/config.tmp","/config.json");}
  void toJson(String& out,bool includeSecrets=false)const{JsonDocument d;d["hostname"]=hostname;d["ssid"]=ssid;d["otaSsid"]=otaSsid;d["otaPasswordSet"]=otaPassword.length()>0;if(includeSecrets){d["password"]=password;d["otaPassword"]=otaPassword;}d["canListenOnly"]=canListenOnly;d["simulateObd"]=simulateObd;d["gpsEnabled"]=gpsEnabled;d["imuEnabled"]=imuEnabled;d["brightness"]=brightness;auto a=d["widgets"].to<JsonArray>();for(auto&w:widgets){auto o=a.add<JsonObject>();o["id"]=w.id;o["x"]=w.x;o["y"]=w.y;o["w"]=w.w;o["h"]=w.h;o["visible"]=w.visible;o["title"]=w.title;o["fontSize"]=w.fontSize;o["background"]=w.background;o["textColor"]=w.textColor;o["backgroundOpacity"]=w.backgroundOpacity;o["border"]=w.border;o["decimals"]=w.decimals==255?widgetDefaultDecimals(w.id):w.decimals;o["valueAlign"]=w.valueAlign;o["timeFormat"]=w.timeFormat;o["visualStyle"]=w.visualStyle;}serializeJson(d,out);}
  bool fromJson(const String&s){JsonDocument d;if(deserializeJson(d,s))return false;hostname=d["hostname"]|hostname;ssid=d["ssid"]|ssid;const char*p=d["password"]|"";if(*p)password=p;otaSsid=d["otaSsid"]|otaSsid;const char*op=d["otaPassword"]|"";if(*op)otaPassword=op;canListenOnly=d["canListenOnly"]|true;simulateObd=d["simulateObd"]|false;gpsEnabled=d["gpsEnabled"]|true;imuEnabled=d["imuEnabled"]|true;brightness=constrain((int)(d["brightness"]|80),5,100);int i=0;for(JsonObject o:d["widgets"].as<JsonArray>()){if(i>=26)break;Widget w;w.id=std::string((const char*)(o["id"]|""));if(w.id.empty())return false;w.w=constrain((int)(o["w"]|100),40,480);w.h=constrain((int)(o["h"]|70),30,480);w.x=constrain((int)(o["x"]|0),0,480-w.w);w.y=constrain((int)(o["y"]|0),0,480-w.h);w.visible=o["visible"]|true;strlcpy(w.title,o["title"]|"",sizeof(w.title));w.fontSize=(uint8_t)constrain((int)(o["fontSize"]|18),10,36);w.background=(uint32_t)(o["background"]|0x102a38);w.textColor=(uint32_t)(o["textColor"]|0xffffff);w.backgroundOpacity=(uint8_t)constrain((int)(o["backgroundOpacity"]|255),0,255);w.border=o["border"]|true;w.decimals=(uint8_t)constrain((int)(o["decimals"]|widgetDefaultDecimals(w.id)),0,5);w.valueAlign=(uint8_t)constrain((int)(o["valueAlign"]|1),0,2);w.timeFormat=(uint8_t)constrain((int)(o["timeFormat"]|0),0,3);w.visualStyle=(uint8_t)constrain((int)(o["visualStyle"]|WIDGET_VALUE),0,3);widgets[i++]=w;}return i>0;}
};}

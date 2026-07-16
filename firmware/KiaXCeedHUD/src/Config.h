#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "Core.h"
namespace hud {
struct Config {
  String hostname="kia-hud",ssid="",password="",otaSsid="",otaPassword="";
  bool canListenOnly=true,gpsEnabled=true,imuEnabled=true,simulateObd=false; uint16_t canRate=500;
  uint8_t brightness=80; std::array<Widget,12> widgets{{
    {"speed",20,20,210,150,true},{"soc",250,20,210,70,true},
    {"power",250,100,210,70,true},{"trip",20,190,210,80,true},
    {"coolant",250,190,210,80,true},{"status",20,290,440,120,true},
    {"rpm",20,20,210,80,false},{"load",250,190,210,80,false},
    {"gpsSpeed",20,190,210,80,false},{"accel",250,290,210,80,false},
    {"canAge",20,390,210,60,false},{"coordinates",250,390,210,60,false}}};
  bool load(){Preferences p;if(!p.begin("hud",true))return false;String s=p.getString("config","");p.end();return s.length()?fromJson(s):false;}
  bool save()const{String s;toJson(s,true);Preferences p;if(!p.begin("hud",false))return false;bool ok=p.putString("config",s)>0;p.end();return ok;}
  void toJson(String& out,bool includeSecrets=false)const{JsonDocument d;d["hostname"]=hostname;d["ssid"]=ssid;d["otaSsid"]=otaSsid;d["otaPasswordSet"]=otaPassword.length()>0;if(includeSecrets){d["password"]=password;d["otaPassword"]=otaPassword;}d["canListenOnly"]=canListenOnly;d["simulateObd"]=simulateObd;d["gpsEnabled"]=gpsEnabled;d["imuEnabled"]=imuEnabled;d["brightness"]=brightness;auto a=d["widgets"].to<JsonArray>();for(auto&w:widgets){auto o=a.add<JsonObject>();o["id"]=w.id;o["x"]=w.x;o["y"]=w.y;o["w"]=w.w;o["h"]=w.h;o["visible"]=w.visible;o["title"]=w.title;o["fontSize"]=w.fontSize;o["background"]=w.background;o["textColor"]=w.textColor;}serializeJson(d,out);}
  bool fromJson(const String&s){JsonDocument d;if(deserializeJson(d,s))return false;hostname=d["hostname"]|hostname;ssid=d["ssid"]|ssid;const char*p=d["password"]|"";if(*p)password=p;otaSsid=d["otaSsid"]|otaSsid;const char*op=d["otaPassword"]|"";if(*op)otaPassword=op;canListenOnly=d["canListenOnly"]|true;simulateObd=d["simulateObd"]|false;gpsEnabled=d["gpsEnabled"]|true;imuEnabled=d["imuEnabled"]|true;brightness=constrain((int)(d["brightness"]|80),5,100);int i=0;for(JsonObject o:d["widgets"].as<JsonArray>()){if(i>=12)break;Widget w;w.id=std::string((const char*)(o["id"]|""));w.x=o["x"]|0;w.y=o["y"]|0;w.w=o["w"]|100;w.h=o["h"]|70;w.visible=o["visible"]|true;strlcpy(w.title,o["title"]|"",sizeof(w.title));w.fontSize=(uint8_t)constrain((int)(o["fontSize"]|18),10,36);w.background=(uint32_t)(o["background"]|0x102a38);w.textColor=(uint32_t)(o["textColor"]|0xffffff);if(validateWidget(w))widgets[i++]=w;}return true;}
};}

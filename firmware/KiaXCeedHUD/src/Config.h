#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "Core.h"
namespace hud {
struct Config {
  String hostname="kia-hud",ssid="",password="";
  bool canListenOnly=true,gpsEnabled=true,imuEnabled=true; uint16_t canRate=500;
  uint8_t brightness=80; std::array<Widget,6> widgets{{
    {"speed",20,20,210,150,true},{"soc",250,20,210,70,true},
    {"power",250,100,210,70,true},{"trip",20,190,210,80,true},
    {"coolant",250,190,210,80,true},{"status",20,290,440,120,true}}};
  bool load(){Preferences p;if(!p.begin("hud",true))return false;String s=p.getString("config","");p.end();return s.length()?fromJson(s):false;}
  bool save()const{String s;toJson(s);Preferences p;if(!p.begin("hud",false))return false;bool ok=p.putString("config",s)>0;p.end();return ok;}
  void toJson(String& out)const{JsonDocument d;d["hostname"]=hostname;d["ssid"]=ssid;d["canListenOnly"]=canListenOnly;d["gpsEnabled"]=gpsEnabled;d["imuEnabled"]=imuEnabled;d["brightness"]=brightness;auto a=d["widgets"].to<JsonArray>();for(auto&w:widgets){auto o=a.add<JsonObject>();o["id"]=w.id;o["x"]=w.x;o["y"]=w.y;o["w"]=w.w;o["h"]=w.h;o["visible"]=w.visible;}serializeJson(d,out);}
  bool fromJson(const String&s){JsonDocument d;if(deserializeJson(d,s))return false;hostname=d["hostname"]|hostname;ssid=d["ssid"]|ssid;password=d["password"]|password;canListenOnly=d["canListenOnly"]|true;gpsEnabled=d["gpsEnabled"]|true;imuEnabled=d["imuEnabled"]|true;brightness=constrain((int)(d["brightness"]|80),5,100);int i=0;for(JsonObject o:d["widgets"].as<JsonArray>()){if(i>=6)break;Widget w{std::string((const char*)(o["id"]|"")),o["x"]|0,o["y"]|0,o["w"]|100,o["h"]|70,o["visible"]|true};if(validateWidget(w))widgets[i++]=w;}return true;}
};}

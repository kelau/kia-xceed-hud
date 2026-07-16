#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <lvgl.h>
#include "src/Core.h"
#include "src/Config.h"
#include "src/VehicleBus.h"
#include "src/WebUi.h"
#include "src/Display.h"

using namespace hud;
Config cfg; VehicleBus bus; Telemetry telemetry; PinGate gate;
AsyncWebServer server(80); DisplayPort display;
std::array<CanFrame,256> frames; size_t frameHead=0, frameCount=0;
lv_obj_t *speedLabel,*socLabel,*pinLabel,*armButton;

static uint32_t bearer(AsyncWebServerRequest*r){if(!r->hasHeader("Authorization"))return 0;String s=r->getHeader("Authorization")->value();if(!s.startsWith("Bearer "))return 0;return strtoul(s.c_str()+7,nullptr,10);}
static bool auth(AsyncWebServerRequest*r){if(gate.authorized(millis(),bearer(r)))return true;r->send(401,"application/json","{\"error\":\"unauthorized\"}");return false;}
static void sendJson(AsyncWebServerRequest*r,JsonDocument&d){String s;serializeJson(d,s);r->send(200,"application/json",s);}

static void armWeb(lv_event_t*){uint32_t pin=esp_random()%900000+100000;gate.arm(millis(),pin);lv_label_set_text_fmt(pinLabel,"Web PIN  %06lu\nExpires in 2 minutes",(unsigned long)gate.pin());lv_obj_clear_flag(pinLabel,LV_OBJ_FLAG_HIDDEN);}
static void initDisplay(){
  if(!display.begin()){Serial.println("Display/touch initialization failed");return;}
  lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0x071018),0);
  speedLabel=lv_label_create(lv_scr_act());lv_obj_set_style_text_color(speedLabel,lv_color_white(),0);lv_label_set_text(speedLabel,"0 km/h");lv_obj_align(speedLabel,LV_ALIGN_TOP_MID,0,35);
  socLabel=lv_label_create(lv_scr_act());lv_obj_set_style_text_color(socLabel,lv_color_hex(0x2dd4bf),0);lv_label_set_text(socLabel,"PHEV SOC --%  |  CAN waiting");lv_obj_align(socLabel,LV_ALIGN_TOP_MID,0,110);
  armButton=lv_btn_create(lv_scr_act());lv_obj_set_size(armButton,210,65);lv_obj_align(armButton,LV_ALIGN_BOTTOM_MID,0,-30);lv_obj_add_event_cb(armButton,armWeb,LV_EVENT_CLICKED,nullptr);auto l=lv_label_create(armButton);lv_label_set_text(l,"Web access");lv_obj_center(l);
  pinLabel=lv_label_create(lv_scr_act());lv_obj_set_style_text_align(pinLabel,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(pinLabel,lv_color_hex(0xffcb67),0);lv_obj_align(pinLabel,LV_ALIGN_CENTER,0,40);lv_obj_add_flag(pinLabel,LV_OBJ_FLAG_HIDDEN);
}
static void initWifi(){WiFi.setHostname(cfg.hostname.c_str());if(cfg.ssid.length()){WiFi.mode(WIFI_STA);WiFi.begin(cfg.ssid,cfg.password);for(int i=0;i<30&&WiFi.status()!=WL_CONNECTED;i++)delay(250);}if(WiFi.status()!=WL_CONNECTED){WiFi.mode(WIFI_AP);WiFi.softAP("Kia-HUD-Setup",cfg.apPassword.c_str());}}
static void routes(){
  server.on("/",HTTP_GET,[](AsyncWebServerRequest*r){if(!gate.canShowLogin(millis())){r->send(403,"text/plain","Touch Web access on the HUD first.");return;}r->send_P(200,"text/html",WEB_UI);});
  server.on("/api/login",HTTP_POST,[](AsyncWebServerRequest*r){},nullptr,[](AsyncWebServerRequest*r,uint8_t*d,size_t n,size_t,size_t){JsonDocument j;if(deserializeJson(j,d,n)||!gate.login(millis(),j["pin"]|0)){r->send(401,"application/json","{\"error\":\"invalid\"}");return;}r->send(200,"application/json",String("{\"token\":")+gate.token()+"}");});
  server.on("/api/status",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;JsonDocument j;j["uptime"]=millis()/1000;j["heap"]=ESP.getFreeHeap();j["rssi"]=WiFi.RSSI();j["canAge"]=millis()-telemetry.lastCanMs;j["speed"]=telemetry.speedKph;j["rpm"]=telemetry.rpm;j["soc"]=telemetry.soc;j["coolant"]=telemetry.coolantC;j["trip"]=telemetry.tripKm;j["gps"]=telemetry.gpsFix;sendJson(r,j);});
  server.on("/api/config",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;String s;cfg.toJson(s);r->send(200,"application/json",s);});
  server.on("/api/config",HTTP_PUT,[](AsyncWebServerRequest*r){},nullptr,[](AsyncWebServerRequest*r,uint8_t*d,size_t n,size_t,size_t){if(!auth(r))return;String s((char*)d,n);if(!cfg.fromJson(s)||!cfg.save()){r->send(400,"application/json","{\"error\":\"invalid config\"}");return;}r->send(200,"application/json","{\"saved\":true}");});
  server.on("/api/frames",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;String q=r->hasParam("q")?r->getParam("q")->value():"";JsonDocument j;auto a=j.to<JsonArray>();for(size_t k=0;k<frameCount;k++){auto&f=frames[(frameHead+256-frameCount+k)%256];if(!frameMatches(f,q.c_str()))continue;auto o=a.add<JsonObject>();char id[9],bytes[25]={};snprintf(id,sizeof(id),"%03X",(unsigned)f.id);for(int i=0;i<f.dlc;i++)snprintf(bytes+strlen(bytes),sizeof(bytes)-strlen(bytes),"%02X%s",f.data[i],i+1<f.dlc?" ":"");o["ms"]=f.ms;o["id"]=id;o["data"]=bytes;std::string decoded=obdDescription(f);if(!decoded.empty())o["decode"]=decoded;if(a.size()>=100)break;}sendJson(r,j);});
  server.on("/api/logout",HTTP_POST,[](AsyncWebServerRequest*r){gate.revoke();r->send(204);});server.onNotFound([](AsyncWebServerRequest*r){r->send(404,"application/json","{\"error\":\"not found\"}");});server.begin();
}
void setup(){Serial.begin(115200);cfg.load();initDisplay();initWifi();bus.begin(cfg.canListenOnly,cfg.canRate);routes();}
void loop() {
  CanFrame f;
  while(bus.receive(f)) {
    frames[frameHead]=f; frameHead=(frameHead+1)%frames.size();
    frameCount=min(frameCount+1,frames.size()); telemetry.lastCanMs=f.ms;
    decodeObd(f,telemetry);
  }
  static uint32_t lastPid=0; static uint8_t pidIndex=0;
  if(!cfg.canListenOnly && millis()-lastPid>=250) {
    static const uint8_t safePids[]={0x0D,0x0C,0x05,0x04,0x2F};
    lastPid=millis(); bus.requestPid(safePids[pidIndex++%5]);
  }
  static uint32_t last=0;
  if(millis()-last>250) {
    last=millis();
    lv_label_set_text_fmt(speedLabel,"%.0f km/h",telemetry.speedKph);
    lv_label_set_text_fmt(socLabel,"PHEV SOC %.0f%%  |  CAN %lums",telemetry.soc,(unsigned long)(millis()-telemetry.lastCanMs));
    if(!gate.canShowLogin(millis())) lv_obj_add_flag(pinLabel,LV_OBJ_FLAG_HIDDEN);
  }
  display.update(); delay(2);
}

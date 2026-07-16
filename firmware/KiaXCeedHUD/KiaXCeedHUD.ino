#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <qrcode.h>
#include <esp_system.h>
#include <lvgl.h>
#include "src/Core.h"
#include "src/Config.h"
#include "src/VehicleBus.h"
#include "src/WebUi.h"
#include "src/Display.h"

using namespace hud;
Config cfg; VehicleBus bus; Telemetry telemetry; TemporaryAccess accessGate;
AsyncWebServer server(80); DNSServer dns; DisplayPort display;
std::array<CanFrame,256> frames; size_t frameHead=0, frameCount=0;
lv_obj_t *speedLabel,*socLabel,*accessLabel,*armButton,*qrCanvas;
lv_color_t* qrPixels=nullptr; String apSsid,apPassword;
volatile bool stopWebRequested=false;

static uint32_t clientId(AsyncWebServerRequest*r){if(r->client()->localIP()!=WiFi.softAPIP())return 0;return (uint32_t)r->client()->remoteIP();}
static bool auth(AsyncWebServerRequest*r){if(accessGate.authorized(millis(),clientId(r)))return true;r->send(401,"application/json","{\"error\":\"unauthorized\"}");return false;}
static void sendJson(AsyncWebServerRequest*r,JsonDocument&d){String s;serializeJson(d,s);r->send(200,"application/json",s);}

static String randomSecret(size_t length){static const char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";String s;s.reserve(length);for(size_t i=0;i<length;i++)s+=alphabet[esp_random()%(sizeof(alphabet)-1)];return s;}
static void renderQr(esp_qrcode_handle_t qr){const int scale=4,quiet=4,size=esp_qrcode_get_size(qr),side=(size+quiet*2)*scale;if(!qrPixels)qrPixels=(lv_color_t*)heap_caps_malloc(180*180*sizeof(lv_color_t),MALLOC_CAP_SPIRAM);lv_canvas_set_buffer(qrCanvas,qrPixels,180,180,LV_IMG_CF_TRUE_COLOR);lv_canvas_fill_bg(qrCanvas,lv_color_white(),LV_OPA_COVER);for(int y=0;y<size;y++)for(int x=0;x<size;x++)if(esp_qrcode_get_module(qr,x,y))for(int py=0;py<scale;py++)for(int px=0;px<scale;px++)lv_canvas_set_px_color(qrCanvas,(x+quiet)*scale+px,(y+quiet)*scale+py,lv_color_black());lv_obj_set_size(qrCanvas,side,side);lv_obj_clear_flag(qrCanvas,LV_OBJ_FLAG_HIDDEN);}
static void showQr(const String& text){esp_qrcode_config_t q={renderQr,5,ESP_QRCODE_ECC_LOW};esp_qrcode_generate(&q,text.c_str());}
static void stopWeb(){dns.stop();WiFi.softAPdisconnect(true);accessGate.revoke();apPassword="";apSsid="";lv_obj_add_flag(qrCanvas,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);if(cfg.ssid.length())WiFi.mode(WIFI_STA);else WiFi.mode(WIFI_OFF);}
static void armWeb(lv_event_t*){stopWeb();accessGate.arm(millis());apSsid="Kia-HUD-"+randomSecret(4);apPassword=randomSecret(20);WiFi.mode(cfg.ssid.length()?WIFI_AP_STA:WIFI_AP);WiFi.softAP(apSsid.c_str(),apPassword.c_str(),1,0,1);dns.start(53,"*",WiFi.softAPIP());showQr("WIFI:T:WPA;S:"+apSsid+";P:"+apPassword+";;");lv_label_set_text_fmt(accessLabel,"Scan to join %s\nOne client - expires in 5 minutes",apSsid.c_str());lv_obj_clear_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);}
static void initDisplay(){
  if(!display.begin()){Serial.println("Display/touch initialization failed");return;}
  lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0x071018),0);
  speedLabel=lv_label_create(lv_scr_act());lv_obj_set_style_text_color(speedLabel,lv_color_white(),0);lv_label_set_text(speedLabel,"0 km/h");lv_obj_align(speedLabel,LV_ALIGN_TOP_MID,0,35);
  socLabel=lv_label_create(lv_scr_act());lv_obj_set_style_text_color(socLabel,lv_color_hex(0x2dd4bf),0);lv_label_set_text(socLabel,"PHEV SOC --%  |  CAN waiting");lv_obj_align(socLabel,LV_ALIGN_TOP_MID,0,110);
  armButton=lv_btn_create(lv_scr_act());lv_obj_set_size(armButton,210,65);lv_obj_align(armButton,LV_ALIGN_BOTTOM_MID,0,-30);lv_obj_add_event_cb(armButton,armWeb,LV_EVENT_CLICKED,nullptr);auto l=lv_label_create(armButton);lv_label_set_text(l,"Web access");lv_obj_center(l);
  qrCanvas=lv_canvas_create(lv_scr_act());lv_obj_align(qrCanvas,LV_ALIGN_CENTER,0,-5);lv_obj_add_flag(qrCanvas,LV_OBJ_FLAG_HIDDEN);
  accessLabel=lv_label_create(lv_scr_act());lv_obj_set_style_text_align(accessLabel,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(accessLabel,lv_color_hex(0xffcb67),0);lv_obj_align(accessLabel,LV_ALIGN_BOTTOM_MID,0,-105);lv_obj_add_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);
}
static void initWifi(){WiFi.setHostname(cfg.hostname.c_str());if(cfg.ssid.length()){WiFi.mode(WIFI_STA);WiFi.begin(cfg.ssid,cfg.password);}else WiFi.mode(WIFI_OFF);}
static void routes(){
  server.on("/",HTTP_GET,[](AsyncWebServerRequest*r){uint32_t id=clientId(r);if(!accessGate.authorized(millis(),id)&&!accessGate.claim(millis(),id)){r->send(403,"text/plain","Touch Web access on the HUD and scan its QR code.");return;}r->send_P(200,"text/html",WEB_UI);});
  server.on("/generate_204",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});server.on("/hotspot-detect.html",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});server.on("/connecttest.txt",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});server.on("/ncsi.txt",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});
  server.on("/api/status",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;JsonDocument j;j["uptime"]=millis()/1000;j["heap"]=ESP.getFreeHeap();j["client"]=r->client()->remoteIP().toString();j["canAge"]=millis()-telemetry.lastCanMs;j["speed"]=telemetry.speedKph;j["rpm"]=telemetry.rpm;j["soc"]=telemetry.soc;j["coolant"]=telemetry.coolantC;j["trip"]=telemetry.tripKm;j["gps"]=telemetry.gpsFix;sendJson(r,j);});
  server.on("/api/config",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;String s;cfg.toJson(s);r->send(200,"application/json",s);});
  server.on("/api/config",HTTP_PUT,[](AsyncWebServerRequest*r){},nullptr,[](AsyncWebServerRequest*r,uint8_t*d,size_t n,size_t,size_t){if(!auth(r))return;String s((char*)d,n);if(!cfg.fromJson(s)||!cfg.save()){r->send(400,"application/json","{\"error\":\"invalid config\"}");return;}r->send(200,"application/json","{\"saved\":true}");});
  server.on("/api/frames",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;String q=r->hasParam("q")?r->getParam("q")->value():"";JsonDocument j;auto a=j.to<JsonArray>();for(size_t k=0;k<frameCount;k++){auto&f=frames[(frameHead+256-frameCount+k)%256];if(!frameMatches(f,q.c_str()))continue;auto o=a.add<JsonObject>();char id[9],bytes[25]={};snprintf(id,sizeof(id),"%03X",(unsigned)f.id);for(int i=0;i<f.dlc;i++)snprintf(bytes+strlen(bytes),sizeof(bytes)-strlen(bytes),"%02X%s",f.data[i],i+1<f.dlc?" ":"");o["ms"]=f.ms;o["id"]=id;o["data"]=bytes;std::string decoded=obdDescription(f);if(!decoded.empty())o["decode"]=decoded;if(a.size()>=100)break;}sendJson(r,j);});
  server.on("/api/logout",HTTP_POST,[](AsyncWebServerRequest*r){if(!auth(r))return;r->send(204);stopWebRequested=true;});server.onNotFound([](AsyncWebServerRequest*r){if(accessGate.joinable(millis())||accessGate.authorized(millis(),clientId(r)))r->redirect("/");else r->send(404,"text/plain","Web access inactive");});server.begin();
}
void setup(){Serial.begin(115200);cfg.load();initDisplay();initWifi();bus.begin(cfg.canListenOnly,cfg.canRate);routes();}
void loop() {
  dns.processNextRequest();
  if(stopWebRequested){stopWebRequested=false;stopWeb();}
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
  if(accessGate.active(millis())&&max(telemetry.speedKph,telemetry.gpsSpeedKph)>=5.0f)stopWeb();
  if(!accessGate.active(millis())&&apPassword.length())stopWeb();
  static uint32_t last=0;
  if(millis()-last>250) {
    last=millis();
    lv_label_set_text_fmt(speedLabel,"%.0f km/h",telemetry.speedKph);
    lv_label_set_text_fmt(socLabel,"PHEV SOC %.0f%%  |  CAN %lums",telemetry.soc,(unsigned long)(millis()-telemetry.lastCanMs));
  }
  display.update(); delay(2);
}

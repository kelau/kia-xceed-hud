#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <qrcode.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <esp_system.h>
#include <lvgl.h>
#include "src/Core.h"
#include "src/Config.h"
#include "src/VehicleBus.h"
#include "src/WebUi.h"
#include "src/Display.h"
#include "src/TestMode.h"
#include "src/OtaUi.h"
#include "src/Dashboard.h"
#include "src/ObdSimulator.h"

using namespace hud;
Config cfg; VehicleBus bus; Telemetry telemetry; TemporaryAccess accessGate;
AsyncWebServer server(80); DNSServer dns; DisplayPort display; Dashboard dashboard;
std::array<CanFrame,256> frames; size_t frameHead=0, frameCount=0;
lv_obj_t *accessLabel,*armButton,*otaButton,*qrCanvas;
lv_color_t* qrPixels=nullptr; String apSsid,apPassword;
volatile bool stopWebRequested=false;
bool mdnsStarted=false;
bool otaConnecting=false,otaStarted=false;uint32_t otaDeadline=0;String otaUploadPassword;
volatile bool dashboardRebuildRequested=false;
static void initWifi();

static bool fromTestLan(AsyncWebServerRequest*r){return test_mode::enabled&&WiFi.status()==WL_CONNECTED&&r->client()->localIP()==WiFi.localIP();}
static uint32_t clientId(AsyncWebServerRequest*r){if(r->client()->localIP()!=WiFi.softAPIP())return 0;return (uint32_t)r->client()->remoteIP();}
static bool auth(AsyncWebServerRequest*r){if(fromTestLan(r)||accessGate.authorized(millis(),clientId(r)))return true;r->send(401,"application/json","{\"error\":\"unauthorized\"}");return false;}
static void sendJson(AsyncWebServerRequest*r,JsonDocument&d){String s;serializeJson(d,s);r->send(200,"application/json",s);}

static String randomSecret(size_t length){static const char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";String s;s.reserve(length);for(size_t i=0;i<length;i++)s+=alphabet[esp_random()%(sizeof(alphabet)-1)];return s;}
static void startOta(lv_event_t*){if(!cfg.otaSsid.length()||!cfg.otaPassword.length()){lv_label_set_text(accessLabel,"Configure hotspot credentials in the web UI first");lv_obj_clear_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);return;}if(max(telemetry.speedKph,telemetry.gpsSpeedKph)>=1.0f){lv_label_set_text(accessLabel,"OTA refused while vehicle is moving");lv_obj_clear_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);return;}otaUploadPassword=randomSecret(20);otaDeadline=millis()+600000;otaConnecting=true;otaStarted=false;MDNS.end();mdnsStarted=false;WiFi.mode(WIFI_STA);WiFi.begin(cfg.otaSsid,cfg.otaPassword);lv_label_set_text_fmt(accessLabel,"OTA: connecting to %s...",cfg.otaSsid.c_str());lv_obj_clear_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);}
static void stopOta(const char* reason){if(otaStarted)ArduinoOTA.end();otaStarted=otaConnecting=false;otaDeadline=0;otaUploadPassword="";lv_label_set_text(accessLabel,reason);lv_obj_clear_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);initWifi();}
static void updateOta(){if(!otaConnecting&&!otaStarted)return;if(max(telemetry.speedKph,telemetry.gpsSpeedKph)>=1.0f){stopOta("OTA cancelled: vehicle moving");return;}if((int32_t)(millis()-otaDeadline)>=0){stopOta("OTA window expired");return;}if(otaConnecting&&WiFi.status()==WL_CONNECTED){ArduinoOTA.setHostname("kia-hud-ota");ArduinoOTA.setPassword(otaUploadPassword.c_str());ArduinoOTA.onStart([](){lv_label_set_text(accessLabel,"OTA update started - do not remove power");});ArduinoOTA.onProgress([](unsigned int done,unsigned int total){lv_label_set_text_fmt(accessLabel,"OTA updating: %u%%",total?done*100/total:0);});ArduinoOTA.onEnd([](){lv_label_set_text(accessLabel,"OTA complete - rebooting");});ArduinoOTA.onError([](ota_error_t error){lv_label_set_text_fmt(accessLabel,"OTA error %u",error);});ArduinoOTA.begin();otaConnecting=false;otaStarted=true;lv_label_set_text_fmt(accessLabel,"OTA READY (10 minutes)\nHost: kia-hud-ota.local\nIP: %s\nPassword: %s",WiFi.localIP().toString().c_str(),otaUploadPassword.c_str());}if(otaStarted)ArduinoOTA.handle();}
static void renderQr(esp_qrcode_handle_t qr){const int scale=4,quiet=4,size=esp_qrcode_get_size(qr),side=(size+quiet*2)*scale;if(!qrPixels)qrPixels=(lv_color_t*)heap_caps_malloc(180*180*sizeof(lv_color_t),MALLOC_CAP_SPIRAM);lv_canvas_set_buffer(qrCanvas,qrPixels,180,180,LV_IMG_CF_TRUE_COLOR);lv_canvas_fill_bg(qrCanvas,lv_color_white(),LV_OPA_COVER);for(int y=0;y<size;y++)for(int x=0;x<size;x++)if(esp_qrcode_get_module(qr,x,y))for(int py=0;py<scale;py++)for(int px=0;px<scale;px++)lv_canvas_set_px_color(qrCanvas,(x+quiet)*scale+px,(y+quiet)*scale+py,lv_color_black());lv_obj_set_size(qrCanvas,side,side);lv_obj_clear_flag(qrCanvas,LV_OBJ_FLAG_HIDDEN);}
static void showQr(const String& text){esp_qrcode_config_t q={renderQr,5,ESP_QRCODE_ECC_LOW};esp_qrcode_generate(&q,text.c_str());}
static void stopWeb(){dns.stop();WiFi.softAPdisconnect(true);accessGate.revoke();apPassword="";apSsid="";lv_obj_add_flag(qrCanvas,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);WiFi.mode(WIFI_STA);if(test_mode::enabled)WiFi.begin(test_mode::ssid,test_mode::password);else if(!cfg.ssid.length())WiFi.disconnect(false,false);}
static void armWeb(lv_event_t*){stopWeb();accessGate.arm(millis());apSsid="Kia-HUD-"+randomSecret(4);apPassword=randomSecret(20);WiFi.mode(cfg.ssid.length()?WIFI_AP_STA:WIFI_AP);WiFi.softAP(apSsid.c_str(),apPassword.c_str(),1,0,1);dns.start(53,"*",WiFi.softAPIP());showQr("WIFI:T:WPA;S:"+apSsid+";P:"+apPassword+";;");lv_label_set_text_fmt(accessLabel,"Scan to join %s\nOne client - expires in 5 minutes",apSsid.c_str());lv_obj_clear_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);}
static void initDisplay(){
  if(!display.begin()){Serial.println("Display/touch initialization failed");return;}
  lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0x071018),0);dashboard.begin(cfg);
  armButton=lv_btn_create(lv_scr_act());lv_obj_set_size(armButton,210,65);lv_obj_align(armButton,LV_ALIGN_BOTTOM_MID,0,-30);lv_obj_add_event_cb(armButton,armWeb,LV_EVENT_CLICKED,nullptr);auto l=lv_label_create(armButton);lv_label_set_text(l,"Web access");lv_obj_center(l);
  if(test_mode::enabled)lv_obj_add_flag(armButton,LV_OBJ_FLAG_HIDDEN);
  otaButton=lv_btn_create(lv_scr_act());lv_obj_set_size(otaButton,150,55);lv_obj_align(otaButton,LV_ALIGN_BOTTOM_RIGHT,-10,-10);lv_obj_add_event_cb(otaButton,startOta,LV_EVENT_CLICKED,nullptr);auto otaText=lv_label_create(otaButton);lv_label_set_text(otaText,"OTA update");lv_obj_center(otaText);
  qrCanvas=lv_canvas_create(lv_scr_act());lv_obj_align(qrCanvas,LV_ALIGN_CENTER,0,-5);lv_obj_add_flag(qrCanvas,LV_OBJ_FLAG_HIDDEN);
  accessLabel=lv_label_create(lv_scr_act());lv_obj_set_style_text_align(accessLabel,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(accessLabel,lv_color_hex(0xffcb67),0);lv_obj_align(accessLabel,LV_ALIGN_BOTTOM_MID,0,-105);lv_obj_add_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);
  dashboard.sendToBack();
}
static void initWifi(){WiFi.mode(WIFI_STA);WiFi.setHostname(test_mode::enabled?test_mode::mdnsName:cfg.hostname.c_str());if(test_mode::enabled)WiFi.begin(test_mode::ssid,test_mode::password);else if(cfg.ssid.length())WiFi.begin(cfg.ssid,cfg.password);else WiFi.disconnect(false,false);}
static void updateTestNetwork(){if(otaConnecting||otaStarted||!test_mode::enabled||WiFi.status()!=WL_CONNECTED)return;if(!mdnsStarted){mdnsStarted=MDNS.begin(test_mode::mdnsName);if(mdnsStarted)MDNS.addService("http","tcp",80);}static IPAddress shown;if(shown!=WiFi.localIP()||lv_obj_has_flag(accessLabel,LV_OBJ_FLAG_HIDDEN)){shown=WiFi.localIP();lv_label_set_text_fmt(accessLabel,"TEST MODE - LAN access\nhttp://%s.local\nhttp://%s",test_mode::mdnsName,shown.toString().c_str());lv_obj_clear_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);}}
static void routes(){
  server.on("/",HTTP_GET,[](AsyncWebServerRequest*r){uint32_t id=clientId(r);if(!fromTestLan(r)&&!accessGate.authorized(millis(),id)&&!accessGate.claim(millis(),id)){r->send(403,"text/plain","Touch Web access on the HUD and scan its QR code.");return;}r->send_P(200,"text/html",WEB_UI);});
  server.on("/generate_204",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});server.on("/hotspot-detect.html",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});server.on("/connecttest.txt",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});server.on("/ncsi.txt",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});
  server.on("/api/status",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;JsonDocument j;j["version"]="0.4.0";j["uptime"]=millis()/1000;j["heap"]=ESP.getFreeHeap();j["minHeap"]=ESP.getMinFreeHeap();j["psramFree"]=ESP.getFreePsram();j["flashSize"]=ESP.getFlashChipSize();j["cpuMHz"]=ESP.getCpuFreqMHz();j["sdk"]=ESP.getSdkVersion();j["client"]=r->client()->remoteIP().toString();j["ip"]=WiFi.localIP().toString();j["rssi"]=WiFi.RSSI();j["canAge"]=millis()-telemetry.lastCanMs;j["frameCount"]=frameCount;j["simulateObd"]=cfg.simulateObd;j["listenOnly"]=cfg.canListenOnly;j["speed"]=telemetry.speedKph;j["rpm"]=telemetry.rpm;j["soc"]=telemetry.soc;j["coolant"]=telemetry.coolantC;j["load"]=telemetry.engineLoad;j["trip"]=telemetry.tripKm;j["gps"]=telemetry.gpsFix;sendJson(r,j);});
  server.on("/ota-config",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;r->send_P(200,"text/html",OTA_CONFIG_UI);});
  server.on("/ota-config",HTTP_POST,[](AsyncWebServerRequest*r){if(!auth(r))return;if(!r->hasParam("ssid",true)){r->send(400,"text/plain","SSID required");return;}String ssid=r->getParam("ssid",true)->value();String password=r->hasParam("password",true)?r->getParam("password",true)->value():"";if(ssid.length()>32||(!password.isEmpty()&&(password.length()<8||password.length()>63))){r->send(400,"text/plain","Invalid SSID or password length");return;}cfg.otaSsid=ssid;if(password.length())cfg.otaPassword=password;if(!cfg.save()){r->send(500,"text/plain","Save failed");return;}r->send(200,"text/html","<h2>Hotspot saved</h2><p>Press OTA update on the stationary HUD.</p><a href='/'>Back</a>");});
  server.on("/api/config",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;String s;cfg.toJson(s);r->send(200,"application/json",s);});
  server.on("/api/config",HTTP_PUT,[](AsyncWebServerRequest*r){},nullptr,[](AsyncWebServerRequest*r,uint8_t*d,size_t n,size_t,size_t){if(!auth(r))return;String s((char*)d,n);if(!cfg.fromJson(s)||!cfg.save()){r->send(400,"application/json","{\"error\":\"invalid config\"}");return;}dashboardRebuildRequested=true;r->send(200,"application/json","{\"saved\":true}");});
  server.on("/api/frames",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;String q=r->hasParam("q")?r->getParam("q")->value():"";JsonDocument j;auto a=j.to<JsonArray>();for(size_t k=0;k<frameCount;k++){auto&f=frames[(frameHead+256-frameCount+k)%256];std::string decoded=obdDescription(f);String decodedText=decoded.c_str();if(!frameMatches(f,q.c_str())&&!decodedText.equalsIgnoreCase(q)&&decodedText.indexOf(q)<0)continue;auto o=a.add<JsonObject>();char id[9],bytes[25]={};snprintf(id,sizeof(id),"%03X",(unsigned)f.id);for(int i=0;i<f.dlc;i++)snprintf(bytes+strlen(bytes),sizeof(bytes)-strlen(bytes),"%02X%s",f.data[i],i+1<f.dlc?" ":"");o["ms"]=f.ms;o["id"]=id;o["data"]=bytes;if(!decoded.empty())o["decode"]=decoded;if(a.size()>=100)break;}sendJson(r,j);});
  server.on("/api/logout",HTTP_POST,[](AsyncWebServerRequest*r){if(!auth(r))return;r->send(204);if(!fromTestLan(r))stopWebRequested=true;});server.onNotFound([](AsyncWebServerRequest*r){if(fromTestLan(r)||accessGate.joinable(millis())||accessGate.authorized(millis(),clientId(r)))r->redirect("/");else r->send(404,"text/plain","Web access inactive");});server.begin();
}
void setup(){
  Serial.begin(115200);delay(100);Serial.println("Kia XCeed HUD v0.2.1 booting");
  cfg.load();Serial.println("Config loaded");
  initDisplay();Serial.println("Display and touch initialized");
  initWifi();Serial.println("Wi-Fi stack initialized; temporary AP inactive");
  Serial.printf("CAN %s\n",bus.begin(cfg.canListenOnly,cfg.canRate)?"initialized":"initialization failed");
  routes();Serial.println("Web service ready; touch Web access to arm");
}
void loop() {
  dns.processNextRequest();
  updateOta();
  if(dashboardRebuildRequested){dashboardRebuildRequested=false;dashboard.rebuild(cfg);dashboard.sendToBack();}
  updateTestNetwork();
  if(stopWebRequested){stopWebRequested=false;stopWeb();}
  CanFrame f;
  while(bus.receive(f)) {
    if(cfg.simulateObd)continue;
    frames[frameHead]=f; frameHead=(frameHead+1)%frames.size();
    frameCount=min(frameCount+1,frames.size()); telemetry.lastCanMs=f.ms;
    decodeObd(f,telemetry);
  }
  static uint32_t lastSim=0;static uint8_t simSequence=0;
  if(cfg.simulateObd&&millis()-lastSim>=100){lastSim=millis();f=simulatedFrame(lastSim,simSequence++);frames[frameHead]=f;frameHead=(frameHead+1)%frames.size();frameCount=min(frameCount+1,frames.size());telemetry.lastCanMs=f.ms;decodeObd(f,telemetry);}
  static uint32_t lastPid=0; static uint8_t pidIndex=0;
  if(!cfg.simulateObd&&!cfg.canListenOnly && millis()-lastPid>=250) {
    static const uint8_t safePids[]={0x0D,0x0C,0x05,0x04,0x2F};
    lastPid=millis(); bus.requestPid(safePids[pidIndex++%5]);
  }
  if(accessGate.active(millis())&&max(telemetry.speedKph,telemetry.gpsSpeedKph)>=5.0f)stopWeb();
  if(!accessGate.active(millis())&&apPassword.length())stopWeb();
  static uint32_t last=0;
  if(millis()-last>250) {
    last=millis();
    dashboard.update(cfg,telemetry,millis());
  }
  display.update(); delay(2);
}

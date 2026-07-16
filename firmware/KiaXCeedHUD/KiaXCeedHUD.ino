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
#include <time.h>
#include "src/Core.h"
#include "src/Config.h"
#include "src/VehicleBus.h"
#include "src/WebUi.h"
#include "src/Display.h"
#include "src/TestMode.h"
#include "src/OtaUi.h"
#include "src/Dashboard.h"
#include "src/ObdSimulator.h"
#include "src/AnalysisUi.h"

using namespace hud;
Config cfg; VehicleBus bus; Telemetry telemetry; TemporaryAccess accessGate;
AsyncWebServer server(80); DNSServer dns; DisplayPort display; Dashboard dashboard;
std::array<CanFrame,256> frames; size_t frameHead=0, frameCount=0;
lv_obj_t *accessLabel,*armButton,*qrCanvas;
lv_color_t* qrPixels=nullptr; String apSsid,apPassword;
volatile bool stopWebRequested=false;
bool mdnsStarted=false;
bool displayStarted=false;
bool otaConnecting=false,otaStarted=false;uint32_t otaDeadline=0;String otaUploadPassword;
volatile bool dashboardRebuildRequested=false,otaStartRequested=false;
SemaphoreHandle_t configMutex=nullptr;
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
  qrCanvas=lv_canvas_create(lv_scr_act());lv_obj_align(qrCanvas,LV_ALIGN_CENTER,0,-5);lv_obj_add_flag(qrCanvas,LV_OBJ_FLAG_HIDDEN);
  accessLabel=lv_label_create(lv_scr_act());lv_obj_set_style_text_align(accessLabel,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(accessLabel,lv_color_hex(0xffcb67),0);lv_obj_align(accessLabel,LV_ALIGN_BOTTOM_MID,0,-105);lv_obj_add_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);
  dashboard.sendToBack();
}
static void initWifi(){WiFi.mode(WIFI_STA);WiFi.setHostname(test_mode::enabled?test_mode::mdnsName:cfg.hostname.c_str());setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();configTime(0,0,"pool.ntp.org","time.cloudflare.com");if(test_mode::enabled)WiFi.begin(test_mode::ssid,test_mode::password);else if(cfg.ssid.length())WiFi.begin(cfg.ssid,cfg.password);else WiFi.disconnect(false,false);}
static void updateTestNetwork(){if(!displayStarted||!accessLabel||otaConnecting||otaStarted||!test_mode::enabled||WiFi.status()!=WL_CONNECTED)return;if(!mdnsStarted){mdnsStarted=MDNS.begin(test_mode::mdnsName);if(mdnsStarted)MDNS.addService("http","tcp",80);}static IPAddress shown;if(shown!=WiFi.localIP()||lv_obj_has_flag(accessLabel,LV_OBJ_FLAG_HIDDEN)){shown=WiFi.localIP();lv_label_set_text_fmt(accessLabel,"TEST MODE - LAN access\nhttp://%s.local\nhttp://%s",test_mode::mdnsName,shown.toString().c_str());lv_obj_clear_flag(accessLabel,LV_OBJ_FLAG_HIDDEN);}}
static void analysisRoutes(){
  server.on("/api/pids",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;JsonDocument j;auto a=j.to<JsonArray>();for(const auto&d:SAE_PIDS){auto o=a.add<JsonObject>();char pid[3];snprintf(pid,sizeof(pid),"%02X",d.pid);o["pid"]=pid;o["name"]=d.name;o["unit"]=d.unit;o["min"]=d.minimum;o["max"]=d.maximum;o["bytes"]=d.bytes;bool seen=false,supported=false;for(size_t n=0;n<frameCount;n++){auto&f=frames[(frameHead+256-frameCount+n)%256];if(f.id<0x7E8||f.id>0x7EF||f.dlc<4||f.data[1]!=0x41)continue;if(f.data[2]==d.pid)seen=true;if((f.data[2]%0x20)==0&&f.dlc>=7&&d.pid>f.data[2]&&d.pid<=f.data[2]+32){uint8_t bit=d.pid-f.data[2]-1;supported|=(f.data[3+bit/8]&(0x80>>(bit%8)))!=0;}}o["seen"]=seen;o["supported"]=supported;}sendJson(r,j);});
  server.on("/api/ecus",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;JsonDocument j;auto a=j.to<JsonArray>();for(uint32_t id=0x7E8;id<=0x7EF;id++){uint32_t count=0,last=0;bool pids[256]={};for(size_t n=0;n<frameCount;n++){auto&f=frames[(frameHead+256-frameCount+n)%256];if(f.id!=id)continue;count++;last=f.ms;if(f.dlc>=3&&f.data[1]==0x41)pids[f.data[2]]=true;}if(!count)continue;auto o=a.add<JsonObject>();char name[8];snprintf(name,sizeof(name),"ECU %03X",(unsigned)id);o["id"]=id;o["name"]=name;o["frames"]=count;o["lastMs"]=last;auto pa=o["pids"].to<JsonArray>();for(int p=0;p<256;p++)if(pids[p])pa.add(p);}sendJson(r,j);});
  server.on("/api/isotp",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;JsonDocument j;auto a=j.to<JsonArray>();uint8_t payload[256];for(size_t n=0;n<frameCount;n++){auto&f=frames[(frameHead+256-frameCount+n)%256];if(f.dlc<2||(f.data[0]>>4)!=1)continue;uint16_t total=((f.data[0]&15)<<8)|f.data[1],used=0;for(int i=2;i<f.dlc&&used<total;i++)payload[used++]=f.data[i];uint8_t seq=1;for(size_t m=n+1;m<frameCount&&used<total;m++){auto&c=frames[(frameHead+256-frameCount+m)%256];if(c.id!=f.id||c.dlc<2||(c.data[0]>>4)!=2||(c.data[0]&15)!=(seq&15))continue;for(int i=1;i<c.dlc&&used<total;i++)payload[used++]=c.data[i];seq++;}auto o=a.add<JsonObject>();char id[9],raw[513]={};snprintf(id,sizeof(id),"%03X",(unsigned)f.id);for(int i=0;i<used;i++)snprintf(raw+strlen(raw),sizeof(raw)-strlen(raw),"%02X",payload[i]);o["id"]=id;o["expected"]=total;o["received"]=used;o["complete"]=used==total;o["payload"]=raw;}sendJson(r,j);});
  server.on("/api/diagnostics",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;JsonDocument j;auto a=j.to<JsonArray>();for(size_t n=0;n<frameCount;n++){auto&f=frames[(frameHead+256-frameCount+n)%256];if(f.id<0x7E8||f.id>0x7EF||f.dlc<4)continue;uint8_t mode=f.data[1];if(mode!=0x42&&mode!=0x43&&mode!=0x47&&mode!=0x4A)continue;for(int i=2;i+1<f.dlc;i+=2){if(!f.data[i]&&!f.data[i+1])continue;static const char family[]="PCBU";char dtc[8];snprintf(dtc,sizeof(dtc),"%c%01X%02X",family[f.data[i]>>6],(f.data[i]>>4)&3,((f.data[i]&15)<<8)|f.data[i+1]);auto o=a.add<JsonObject>();o["dtc"]=dtc;o["ecu"]=f.id;o["kind"]=mode==0x42?"freeze-frame":mode==0x43?"stored":mode==0x47?"pending":"permanent";o["ms"]=f.ms;}}sendJson(r,j);});
  server.on("/api/export.csv",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;String out="milliseconds,id,dlc,data,decode\r\n";for(size_t n=0;n<frameCount;n++){auto&f=frames[(frameHead+256-frameCount+n)%256];char row[160],raw[25]={};for(int i=0;i<f.dlc;i++)snprintf(raw+strlen(raw),sizeof(raw)-strlen(raw),"%02X%s",f.data[i],i+1<f.dlc?" ":"");auto d=obdDescription(f);snprintf(row,sizeof(row),"%u,%03X,%u,\"%s\",\"%s\"\r\n",f.ms,(unsigned)f.id,f.dlc,raw,d.c_str());out+=row;}r->send(200,"text/csv",out);});
}
static void routes(){
  analysisRoutes();
  server.on("/",HTTP_GET,[](AsyncWebServerRequest*r){uint32_t id=clientId(r);if(!fromTestLan(r)&&!accessGate.authorized(millis(),id)&&!accessGate.claim(millis(),id)){r->send(403,"text/plain","Touch Web access on the HUD and scan its QR code.");return;}r->send_P(200,"text/html",WEB_UI);});
  server.on("/analysis.js",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;r->send_P(200,"application/javascript",ANALYSIS_JS);});
  server.on("/generate_204",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});server.on("/hotspot-detect.html",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});server.on("/connecttest.txt",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});server.on("/ncsi.txt",HTTP_GET,[](AsyncWebServerRequest*r){r->redirect("/");});
  server.on("/api/status",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;JsonDocument j;j["version"]=FIRMWARE_VERSION;time_t now=time(nullptr);char date[40]="Not synchronized";if(now>1700000000){struct tm local;localtime_r(&now,&local);strftime(date,sizeof(date),"%Y-%m-%d %H:%M:%S %Z",&local);}j["date"]=date;j["uptime"]=millis()/1000;j["heap"]=ESP.getFreeHeap();j["minHeap"]=ESP.getMinFreeHeap();j["psramFree"]=ESP.getFreePsram();j["flashSize"]=ESP.getFlashChipSize();j["cpuMHz"]=ESP.getCpuFreqMHz();j["sdk"]=ESP.getSdkVersion();j["client"]=r->client()->remoteIP().toString();j["ip"]=WiFi.localIP().toString();j["rssi"]=WiFi.RSSI();j["canAge"]=millis()-telemetry.lastCanMs;j["frameCount"]=frameCount;j["simulateObd"]=cfg.simulateObd;j["listenOnly"]=cfg.canListenOnly;j["speed"]=telemetry.speedKph;j["rpm"]=telemetry.rpm;j["soc"]=telemetry.soc;j["coolant"]=telemetry.coolantC;j["load"]=telemetry.engineLoad;j["trip"]=telemetry.tripKm;j["gps"]=telemetry.gpsFix;sendJson(r,j);});
  server.on("/ota-config",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;r->send_P(200,"text/html",OTA_CONFIG_UI);});
  server.on("/api/ota-start",HTTP_POST,[](AsyncWebServerRequest*r){if(!auth(r))return;if(otaConnecting||otaStarted){r->send(409,"application/json","{\"error\":\"OTA already active\"}");return;}otaStartRequested=true;r->send(202,"application/json","{\"starting\":true}");});
  server.on("/ota-config",HTTP_POST,[](AsyncWebServerRequest*r){if(!auth(r))return;if(!r->hasParam("ssid",true)){r->send(400,"text/plain","SSID required");return;}String ssid=r->getParam("ssid",true)->value();String password=r->hasParam("password",true)?r->getParam("password",true)->value():"";if(ssid.length()>32||(!password.isEmpty()&&(password.length()<8||password.length()>63))){r->send(400,"text/plain","Invalid SSID or password length");return;}cfg.otaSsid=ssid;if(password.length())cfg.otaPassword=password;if(!cfg.save()){r->send(500,"text/plain","Save failed");return;}r->send(200,"text/html","<h2>Hotspot saved</h2><p>Return to Configuration and select Start OTA update.</p><a href='/'>Back</a>");});
  server.on("/api/config",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;String s;cfg.toJson(s);r->send(200,"application/json",s);});
  server.on("/api/config",HTTP_PUT,[](AsyncWebServerRequest*r){},nullptr,[](AsyncWebServerRequest*r,uint8_t*d,size_t n,size_t index,size_t total){if(index==0){if(!auth(r))return;r->_tempObject=new String();((String*)r->_tempObject)->reserve(total);}auto body=(String*)r->_tempObject;if(!body)return;body->concat((const char*)d,n);if(index+n<total)return;xSemaphoreTake(configMutex,portMAX_DELAY);Config candidate=cfg;xSemaphoreGive(configMutex);bool ok=candidate.fromJson(*body)&&candidate.save();if(ok){xSemaphoreTake(configMutex,portMAX_DELAY);cfg=candidate;xSemaphoreGive(configMutex);}delete body;r->_tempObject=nullptr;if(!ok){r->send(400,"application/json","{\"error\":\"invalid config\"}");return;}dashboardRebuildRequested=true;r->send(200,"application/json","{\"saved\":true}");});
  server.on("/api/frames",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;String q=r->hasParam("q")?r->getParam("q")->value():"";JsonDocument j;auto a=j.to<JsonArray>();for(size_t k=0;k<frameCount;k++){auto&f=frames[(frameHead+256-frameCount+k)%256];std::string decoded=obdDescription(f);String decodedText=decoded.c_str();if(!frameMatches(f,q.c_str())&&!decodedText.equalsIgnoreCase(q)&&decodedText.indexOf(q)<0)continue;auto o=a.add<JsonObject>();char id[9],bytes[25]={};snprintf(id,sizeof(id),"%03X",(unsigned)f.id);for(int i=0;i<f.dlc;i++)snprintf(bytes+strlen(bytes),sizeof(bytes)-strlen(bytes),"%02X%s",f.data[i],i+1<f.dlc?" ":"");o["ms"]=f.ms;o["id"]=id;o["data"]=bytes;if(!decoded.empty())o["decode"]=decoded;if(a.size()>=100)break;}sendJson(r,j);});
  server.on("/api/frame-types",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;JsonDocument j;auto out=j.to<JsonArray>();for(size_t k=0;k<frameCount&&out.size()<40;k++){auto&sample=frames[(frameHead+256-frameCount+k)%256];std::string key=frameTypeKey(sample);bool exists=false;for(auto v:out)if(v["key"].as<std::string>()==key){exists=true;break;}if(exists)continue;uint32_t count=0,first=0,last=0;float lo=0,hi=0,rlo=0,rhi=0;bool metric=false;const char*unit="";CanFrame latest=sample;for(size_t n=0;n<frameCount;n++){auto&f=frames[(frameHead+256-frameCount+n)%256];if(frameTypeKey(f)!=key)continue;float value,a,b;const char*u;if(!count)first=f.ms;last=f.ms;latest=f;count++;if(frameMetric(f,value,a,b,u)){if(!metric){lo=hi=value;rlo=a;rhi=b;unit=u;}else{lo=min(lo,value);hi=max(hi,value);}metric=true;}}auto o=out.add<JsonObject>();o["key"]=key;o["name"]=(latest.id>=0x7E8&&latest.id<=0x7EF&&latest.dlc>=3)?obdPidName(latest.data[2]):"CAN frame";o["count"]=count;o["frequency"]=(count>1&&last>first)?(count-1)*1000.0f/(last-first):0;o["min"]=metric?lo:0;o["max"]=metric?hi:0;o["realMin"]=rlo;o["realMax"]=rhi;o["unit"]=unit;o["metric"]=metric;char raw[25]={};for(int i=0;i<latest.dlc;i++)snprintf(raw+strlen(raw),sizeof(raw)-strlen(raw),"%02X%s",latest.data[i],i+1<latest.dlc?" ":"");o["raw"]=raw;}sendJson(r,j);});
  server.on("/api/frame-detail",HTTP_GET,[](AsyncWebServerRequest*r){if(!auth(r))return;if(!r->hasParam("key")){r->send(400,"application/json","{\"error\":\"key required\"}");return;}std::string key=r->getParam("key")->value().c_str();JsonDocument j;auto history=j["history"].to<JsonArray>();uint32_t count=0,first=0,last=0;float lo=0,hi=0,rlo=0,rhi=0;bool metric=false;const char*unit="";CanFrame latest;for(size_t n=0;n<frameCount;n++){auto&f=frames[(frameHead+256-frameCount+n)%256];if(frameTypeKey(f)!=key)continue;float value,a,b;const char*u;if(!count)first=f.ms;last=f.ms;latest=f;count++;if(frameMetric(f,value,a,b,u)){if(!metric){lo=hi=value;rlo=a;rhi=b;unit=u;}else{lo=min(lo,value);hi=max(hi,value);}metric=true;auto p=history.add<JsonObject>();p["ms"]=f.ms;p["value"]=value;}}j["key"]=key;j["name"]=(latest.id>=0x7E8&&latest.id<=0x7EF&&latest.dlc>=3)?obdPidName(latest.data[2]):"CAN frame";j["description"]=obdDescription(latest);j["count"]=count;j["frequency"]=(count>1&&last>first)?(count-1)*1000.0f/(last-first):0;j["min"]=metric?lo:0;j["max"]=metric?hi:0;j["realMin"]=rlo;j["realMax"]=rhi;j["unit"]=unit;j["metric"]=metric;char raw[25]={};for(int i=0;i<latest.dlc;i++)snprintf(raw+strlen(raw),sizeof(raw)-strlen(raw),"%02X%s",latest.data[i],i+1<latest.dlc?" ":"");j["raw"]=raw;sendJson(r,j);});
  server.on("/api/logout",HTTP_POST,[](AsyncWebServerRequest*r){if(!auth(r))return;r->send(204);if(!fromTestLan(r))stopWebRequested=true;});server.onNotFound([](AsyncWebServerRequest*r){if(fromTestLan(r)||accessGate.joinable(millis())||accessGate.authorized(millis(),clientId(r)))r->redirect("/");else r->send(404,"text/plain","Web access inactive");});server.begin();
}
void setup(){
  Serial.begin(115200);delay(100);Serial.printf("Kia XCeed HUD v%s booting\n",FIRMWARE_VERSION);
  configMutex=xSemaphoreCreateMutex();
  cfg.load();Serial.println("Config loaded");
  displayStarted=true;initDisplay();Serial.println("Display and touch initialized");
  initWifi();Serial.println("Wi-Fi stack initialized; temporary AP inactive");
  routes();Serial.println("Web service ready; touch Web access to arm");
  Serial.printf("CAN %s\n",bus.begin(cfg.canListenOnly,cfg.canRate)?"initialized":"initialization failed");
}
void loop() {
  dns.processNextRequest();
  if(otaStartRequested){otaStartRequested=false;startOta(nullptr);}
  updateOta();
  if(dashboardRebuildRequested){dashboardRebuildRequested=false;xSemaphoreTake(configMutex,portMAX_DELAY);dashboard.rebuild(cfg);dashboard.sendToBack();xSemaphoreGive(configMutex);}
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
    static const uint8_t safePids[]={0x00,0x20,0x40,0x60,0x80,0xA0,0x0D,0x0C,0x05,0x04,0x2F,0x0F,0x10,0x11,0x1F,0x42,0x46,0x5C,0x5E};
    lastPid=millis(); bus.requestPid(safePids[pidIndex++%(sizeof(safePids)/sizeof(safePids[0]))]);
  }
  if(accessGate.active(millis())&&max(telemetry.speedKph,telemetry.gpsSpeedKph)>=5.0f)stopWeb();
  if(!accessGate.active(millis())&&apPassword.length())stopWeb();
  static uint32_t last=0;
  if(millis()-last>250) {
    last=millis();
    xSemaphoreTake(configMutex,portMAX_DELAY);dashboard.update(cfg,telemetry,millis());xSemaphoreGive(configMutex);
  }
  if(displayStarted)display.update(); delay(2);
}

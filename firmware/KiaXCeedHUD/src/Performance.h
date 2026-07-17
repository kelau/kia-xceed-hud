#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <freertos/task.h>
namespace hud {
class PerformanceMonitor {
 public:
  struct Timing { float last=0,average=0,peak=0; };
  void begin(){sampleMs_=millis();readIdle(idleSample_);}
  void tick(uint32_t now){uint32_t elapsed=now-sampleMs_;if(elapsed<1000)return;uint32_t idle[2]={};readIdle(idle);float possible=elapsed*1000.0f;for(int c=0;c<2;c++){uint32_t idleDelta=idle[c]-idleSample_[c];float v=possible?100.0f*(1.0f-min(1.0f,idleDelta/possible)):0;cpu_[c].last=v;cpu_[c].average=samples_?cpu_[c].average*.8f+v*.2f:v;cpu_[c].peak=max(cpu_[c].peak,v);idleSample_[c]=idle[c];}samples_++;sampleMs_=now;}
  void recordLoop(uint32_t us){record(loop_,us);}void recordLcd(uint32_t us){record(lcd_,us);}void recordCan(uint32_t us){record(can_,us);}
  void noteDisplayService(uint32_t now){if(lastDisplayUs_){uint32_t gap=now-lastDisplayUs_;if(gap>DEADLINE_US)missed_+=(gap-1)/DEADLINE_US;}lastDisplayUs_=now;}
  void noteCanDropped(uint32_t n=1){dropped_+=n;}void noteCanCoalesced(uint32_t n=1){coalesced_+=n;}
  void webBegin(AsyncWebServerRequest*){portENTER_CRITICAL(&mux_);webStarted_=micros();portEXIT_CRITICAL(&mux_);}
  void webEnd(AsyncWebServerRequest*){uint32_t start;portENTER_CRITICAL(&mux_);start=webStarted_;webStarted_=0;portEXIT_CRITICAL(&mux_);if(start)record(webTiming_,micros()-start);}
  void toJson(JsonDocument&j)const{auto a=j["cores"].to<JsonArray>();for(int c=0;c<2;c++){auto o=a.add<JsonObject>();o["core"]=c;o["utilization"]=cpu_[c].last;o["average"]=cpu_[c].average;o["peak"]=cpu_[c].peak;}timing(j["loop"].to<JsonObject>(),loop_);timing(j["lcdRender"].to<JsonObject>(),lcd_);timing(j["canProcessing"].to<JsonObject>(),can_);timing(j["webRequest"].to<JsonObject>(),webTiming_);j["missedDisplayDeadlines"]=missed_;j["droppedCanFrames"]=dropped_;j["coalescedCanFrames"]=coalesced_;j["displayDeadlineUs"]=DEADLINE_US;}
 private:
  struct WebSlot{AsyncWebServerRequest*request=nullptr;uint32_t started=0;};static constexpr uint32_t DEADLINE_US=20000;static void readIdle(uint32_t idle[2]){for(int c=0;c<2;c++){TaskStatus_t status={};vTaskGetInfo(xTaskGetIdleTaskHandleForCore(c),&status,pdFALSE,eInvalid);idle[c]=status.ulRunTimeCounter;}}static void record(Timing&t,uint32_t us){float v=us;t.last=v;t.average=t.average?t.average*.9f+v*.1f:v;t.peak=max(t.peak,v);}static void timing(JsonObject o,const Timing&t){o["lastUs"]=t.last;o["averageUs"]=t.average;o["peakUs"]=t.peak;}
  uint32_t idleSample_[2]={},sampleMs_=0,samples_=0,lastDisplayUs_=0;Timing cpu_[2],loop_,lcd_,can_,webTiming_;volatile uint32_t missed_=0,dropped_=0,coalesced_=0,webStarted_=0;WebSlot web_[8];portMUX_TYPE mux_=portMUX_INITIALIZER_UNLOCKED;
};
}

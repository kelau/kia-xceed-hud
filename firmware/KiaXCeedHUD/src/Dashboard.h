#pragma once
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>
#include "Config.h"
namespace hud {
class Dashboard {
 public:
  void begin(const Config& c){root_=createRoot();populate(root_,c);}
  void rebuild(const Config& c){
    auto oldCards=cards_;auto oldTitles=titles_;auto oldValues=values_;auto oldIds=ids_;cards_.fill(nullptr);titles_.fill(nullptr);values_.fill(nullptr);for(auto&id:ids_)id.clear();
    for(size_t i=0;i<c.widgets.size();i++){auto&w=c.widgets[i];if(!w.visible)continue;size_t found=oldCards.size();for(size_t j=0;j<oldCards.size();j++)if(oldCards[j]&&oldIds[j]==w.id){found=j;break;}if(found<oldCards.size()){cards_[i]=oldCards[found];titles_[i]=oldTitles[found];values_[i]=oldValues[found];oldCards[found]=nullptr;}else{cards_[i]=lv_obj_create(root_);titles_[i]=lv_label_create(cards_[i]);values_[i]=lv_label_create(cards_[i]);}ids_[i]=w.id;configure(i,w);}
    for(auto card:oldCards)if(card)lv_obj_del(card);
  }
  void populate(lv_obj_t* root,const Config& c){
    for(size_t i=0;i<c.widgets.size();i++){
      auto&w=c.widgets[i];if(!w.visible)continue;
      auto card=lv_obj_create(root);cards_[i]=card;titles_[i]=lv_label_create(card);values_[i]=lv_label_create(card);ids_[i]=w.id;configure(i,w);
    }
  }
  void update(const Config& c,const Telemetry&t,uint32_t now){
    for(size_t i=0;i<c.widgets.size();i++){
      if(!cards_[i]||!values_[i])continue;String v;auto&w=c.widgets[i];auto&id=w.id;uint8_t decimals=w.decimals==255?widgetDefaultDecimals(id):w.decimals;
      if(id=="speed")v=String(t.speedKph,(unsigned int)decimals)+" km/h";else if(id=="soc")v=String(t.soc,(unsigned int)decimals)+" %";
      else if(id=="power"||id=="rpm")v=String(t.rpm,(unsigned int)decimals)+" rpm";else if(id=="trip")v=String(t.tripKm,(unsigned int)decimals)+" km";
      else if(id=="coolant")v=String(t.coolantC,(unsigned int)decimals)+" C";else if(id=="load")v=String(t.engineLoad,(unsigned int)decimals)+" %";
      else if(id=="gpsSpeed")v=String(t.gpsSpeedKph,(unsigned int)decimals)+" km/h";else if(id=="accel")v=String(t.accelLong,(unsigned int)decimals)+" / "+String(t.accelLat,(unsigned int)decimals)+" g";
      else if(id=="canAge")v=String(now-t.lastCanMs)+" ms";else if(id=="coordinates")v=String(t.latitude,5)+", "+String(t.longitude,5);
      else if(id=="status")v=String(t.gpsFix?"GPS fix | ":"GPS -- | ")+String(now-t.lastCanMs)+"ms";
      else if(id=="version")v=FIRMWARE_VERSION;else if(id=="gpsLock")v=t.gpsFix?"LOCKED":"NO FIX";
      else if(id=="uptime")v=String(now/1000)+" s";else if(id=="wifi")v=WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():"offline";
      else if(id=="time"||id=="date"){time_t epoch=time(nullptr);if(epoch>1700000000){struct tm local;localtime_r(&epoch,&local);char text[24];const char*formats[]={"%H:%M:%S","%H:%M","%I:%M:%S %p","%I:%M %p"};strftime(text,sizeof(text),id=="time"?formats[min((uint8_t)3,w.timeFormat)]:"%Y-%m-%d",&local);v=text;}else v="--";}else v="--";
      lv_label_set_text(values_[i],v.c_str());
    }
  }
  void sendToBack(){if(root_)lv_obj_move_background(root_);}
 private:
  void configure(size_t i,const Widget&w){auto card=cards_[i];lv_obj_set_pos(card,w.x,w.y);lv_obj_set_size(card,w.w,w.h);lv_obj_set_style_bg_color(card,lv_color_hex(w.background),0);lv_obj_set_style_bg_opa(card,w.backgroundOpacity,0);lv_obj_set_style_border_color(card,lv_color_hex(0x2dd4bf),0);lv_obj_set_style_border_width(card,w.border?2:0,0);lv_obj_set_style_radius(card,10,0);lv_obj_clear_flag(card,LV_OBJ_FLAG_SCROLLABLE);auto title=titles_[i];lv_label_set_text(title,w.title[0]?w.title:widgetDefaultTitle(w.id));lv_obj_set_style_text_color(title,lv_color_hex(w.textColor),0);lv_obj_align(title,LV_ALIGN_TOP_LEFT,0,0);auto value=values_[i];lv_obj_set_style_text_color(value,lv_color_hex(w.textColor),0);lv_obj_set_style_text_font(value,&lv_font_montserrat_14,0);lv_obj_align(value,w.valueAlign==0?LV_ALIGN_LEFT_MID:w.valueAlign==2?LV_ALIGN_RIGHT_MID:LV_ALIGN_CENTER,0,8);lv_obj_move_foreground(card);}
  static lv_obj_t* createRoot(){auto root=lv_obj_create(lv_scr_act());lv_obj_set_size(root,480,480);lv_obj_set_pos(root,0,0);lv_obj_set_style_bg_color(root,lv_color_hex(0x071018),0);lv_obj_set_style_border_width(root,0,0);lv_obj_set_style_pad_all(root,0,0);lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);return root;}
  lv_obj_t*root_=nullptr;std::array<lv_obj_t*,18>cards_{};std::array<lv_obj_t*,18>titles_{};std::array<lv_obj_t*,18>values_{};std::array<std::string,18>ids_{};
};}

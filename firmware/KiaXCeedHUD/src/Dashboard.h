#pragma once
#include <lvgl.h>
#include "Config.h"
namespace hud {
class Dashboard {
 public:
  void begin(const Config& c){
    root_=lv_obj_create(lv_scr_act());lv_obj_set_size(root_,480,480);lv_obj_set_pos(root_,0,0);
    lv_obj_set_style_bg_color(root_,lv_color_hex(0x071018),0);lv_obj_set_style_border_width(root_,0,0);
    lv_obj_set_style_pad_all(root_,0,0);
    lv_obj_clear_flag(root_,LV_OBJ_FLAG_SCROLLABLE);rebuild(c);
  }
  void rebuild(const Config& c){
    for(auto&o:cards_)if(o)lv_obj_del(o);cards_.fill(nullptr);values_.fill(nullptr);
    for(size_t i=0;i<c.widgets.size();i++){
      auto&w=c.widgets[i];if(!w.visible)continue;auto card=lv_obj_create(root_);cards_[i]=card;
      lv_obj_set_pos(card,w.x,w.y);lv_obj_set_size(card,w.w,w.h);lv_obj_set_style_bg_color(card,lv_color_hex(w.background),0);
      lv_obj_set_style_border_color(card,lv_color_hex(0x2dd4bf),0);lv_obj_set_style_radius(card,10,0);lv_obj_clear_flag(card,LV_OBJ_FLAG_SCROLLABLE);
      auto title=lv_label_create(card);lv_label_set_text(title,w.title[0]?w.title:w.id.c_str());lv_obj_set_style_text_color(title,lv_color_hex(w.textColor),0);lv_obj_align(title,LV_ALIGN_TOP_LEFT,0,0);
      auto value=lv_label_create(card);values_[i]=value;lv_obj_set_style_text_color(value,lv_color_hex(w.textColor),0);lv_obj_align(value,LV_ALIGN_CENTER,0,8);
    }
  }
  void update(const Config& c,const Telemetry&t,uint32_t now){
    for(size_t i=0;i<c.widgets.size();i++){
      if(!cards_[i]||!values_[i])continue;String v;auto&id=c.widgets[i].id;
      if(id=="speed")v=String(t.speedKph,0)+" km/h";else if(id=="soc")v=String(t.soc,1)+" %";
      else if(id=="power"||id=="rpm")v=String(t.rpm,0)+" rpm";else if(id=="trip")v=String(t.tripKm,1)+" km";
      else if(id=="coolant")v=String(t.coolantC,0)+" C";else if(id=="load")v=String(t.engineLoad,1)+" %";
      else if(id=="gpsSpeed")v=String(t.gpsSpeedKph,1)+" km/h";else if(id=="accel")v=String(t.accelLong,2)+" / "+String(t.accelLat,2)+" g";
      else if(id=="canAge")v=String(now-t.lastCanMs)+" ms";else if(id=="coordinates")v=String(t.latitude,5)+", "+String(t.longitude,5);
      else if(id=="status")v=String(t.gpsFix?"GPS fix | ":"GPS -- | ")+String(now-t.lastCanMs)+"ms";else v="--";
      lv_label_set_text(values_[i],v.c_str());
    }
  }
  void sendToBack(){if(root_)lv_obj_move_background(root_);}
 private:
  lv_obj_t*root_=nullptr;std::array<lv_obj_t*,12>cards_{};std::array<lv_obj_t*,12>values_{};
};}

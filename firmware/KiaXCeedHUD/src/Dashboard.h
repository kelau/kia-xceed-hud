#pragma once
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>
#include "Config.h"
namespace hud {
class Dashboard {
 public:
  void setWebAccessCallback(lv_event_cb_t callback){webAccessCallback_=callback;}
  void begin(const Config& c){root_=createRoot();populate(root_,c);}
  void rebuild(const Config& c){
    auto oldCards=cards_;auto oldTitles=titles_;auto oldValues=values_;auto oldVisuals=visuals_;auto oldIds=ids_;
    cards_.fill(nullptr);titles_.fill(nullptr);values_.fill(nullptr);visuals_.fill(nullptr);for(auto&id:ids_)id.clear();
    for(size_t i=0;i<c.widgets.size();i++){auto&w=c.widgets[i];if(!w.visible)continue;size_t found=oldCards.size();for(size_t j=0;j<oldCards.size();j++)if(oldCards[j]&&oldIds[j]==w.id){found=j;break;}if(found<oldCards.size()){cards_[i]=oldCards[found];titles_[i]=oldTitles[found];values_[i]=oldValues[found];visuals_[i]=oldVisuals[found];oldCards[found]=nullptr;}else{cards_[i]=lv_obj_create(root_);titles_[i]=lv_label_create(cards_[i]);values_[i]=lv_label_create(cards_[i]);}ids_[i]=w.id;configure(i,w);}
    for(auto card:oldCards)if(card)lv_obj_del(card);
  }
  void populate(lv_obj_t* root,const Config& c){for(size_t i=0;i<c.widgets.size();i++){auto&w=c.widgets[i];if(!w.visible)continue;cards_[i]=lv_obj_create(root);titles_[i]=lv_label_create(cards_[i]);values_[i]=lv_label_create(cards_[i]);ids_[i]=w.id;configure(i,w);}}
  void update(const Config& c,const Telemetry&t,uint32_t now){
    for(size_t i=0;i<c.widgets.size();i++){
      if(!cards_[i]||!values_[i])continue;String v;auto&w=c.widgets[i];auto&id=w.id;uint8_t decimals=w.decimals==255?widgetDefaultDecimals(id):w.decimals;float raw=0;bool numeric=true,active=false;
      if(id=="speed"){raw=t.speedKph;v=String(raw,(unsigned)decimals)+" km/h";}else if(id=="soc"){raw=t.soc;v=String(raw,(unsigned)decimals)+" %";}
      else if(id=="power"||id=="rpm"){raw=t.rpm;v=String(raw,(unsigned)decimals)+" rpm";}else if(id=="trip"){raw=t.tripKm;v=String(raw,(unsigned)decimals)+" km";}
      else if(id=="coolant"){raw=t.coolantC;v=String(raw,(unsigned)decimals)+" C";}else if(id=="load"){raw=t.engineLoad;v=String(raw,(unsigned)decimals)+" %";}
      else if(id=="gpsSpeed"){raw=t.gpsSpeedKph;v=String(raw,(unsigned)decimals)+" km/h";}else if(id=="accel"){numeric=false;v=String(t.accelLong,(unsigned)decimals)+" / "+String(t.accelLat,(unsigned)decimals)+" g";}
      else if(id=="canAge"){raw=now-t.lastCanMs;v=String((uint32_t)raw)+" ms";}else if(id=="coordinates"){numeric=false;v=String(t.latitude,5)+", "+String(t.longitude,5);}
      else if(id=="status"){numeric=false;active=now-t.lastCanMs<1000;v=String(t.gpsFix?"GPS fix | ":"GPS -- | ")+String(now-t.lastCanMs)+"ms";}
      else if(id=="version"){numeric=false;v=FIRMWARE_VERSION;}else if(id=="gpsLock"){numeric=false;active=t.gpsFix;v=t.gpsFix?"LOCKED":"NO FIX";}
      else if(id=="uptime"){numeric=false;v=String(now/1000)+" s";}else if(id=="wifi"){numeric=false;active=WiFi.status()==WL_CONNECTED;v=active?WiFi.localIP().toString():"offline";}
      else if(id=="intakeTemp"){raw=t.intakeTempC;v=String(raw,(unsigned)decimals)+" C";}else if(id=="throttle"){raw=t.throttlePct;v=String(raw,(unsigned)decimals)+" %";}
      else if(id=="voltage"){raw=t.controlVoltage;v=String(raw,(unsigned)decimals)+" V";}else if(id=="ambientTemp"){raw=t.ambientTempC;v=String(raw,(unsigned)decimals)+" C";}
      else if(id=="fuelRate"){raw=t.fuelRateLph;v=String(raw,(unsigned)decimals)+" L/h";}else if(id=="wifiSignal"){raw=WiFi.status()==WL_CONNECTED?WiFi.RSSI():-100;v=WiFi.status()==WL_CONNECTED?String((int)raw)+" dBm":"offline";}
      else if(id=="mode"){numeric=false;active=true;v=c.simulateObd?"SIMULATION":c.canListenOnly?"CAN LISTEN":"OBD POLLING";}else if(id=="webAccess"){numeric=false;active=true;v="Tap to connect";}
      else if(id=="time"||id=="date"){numeric=false;time_t epoch=time(nullptr);if(epoch>1700000000){struct tm local;localtime_r(&epoch,&local);char text[24];const char*formats[]={"%H:%M:%S","%H:%M","%I:%M:%S %p","%I:%M %p"};strftime(text,sizeof(text),id=="time"?formats[min((uint8_t)3,w.timeFormat)]:"%Y-%m-%d",&local);v=text;}else v="--";}else{numeric=false;v="--";}
      lv_label_set_text(values_[i],v.c_str());applyVisual(i,w,raw,numeric,active);
    }
  }
  void sendToBack(){if(root_)lv_obj_move_background(root_);}
 private:
  void configure(size_t i,const Widget&w){
    auto card=cards_[i];lv_obj_set_pos(card,w.x,w.y);lv_obj_set_size(card,w.w,w.h);lv_obj_set_style_bg_color(card,lv_color_hex(w.background),0);lv_obj_set_style_bg_opa(card,w.backgroundOpacity,0);lv_obj_set_style_border_color(card,lv_color_hex(0x2dd4bf),0);lv_obj_set_style_border_width(card,w.border?2:0,0);lv_obj_set_style_radius(card,10,0);lv_obj_clear_flag(card,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);if(webAccessCallback_)lv_obj_remove_event_cb(card,webAccessCallback_);if(w.id=="webAccess"&&webAccessCallback_){lv_obj_add_flag(card,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(card,webAccessCallback_,LV_EVENT_CLICKED,nullptr);}
    auto title=titles_[i];lv_label_set_text(title,w.title[0]?w.title:widgetDefaultTitle(w.id));lv_obj_set_style_text_color(title,lv_color_hex(w.textColor),0);lv_obj_align(title,LV_ALIGN_TOP_LEFT,0,0);
    auto value=values_[i];lv_obj_set_style_text_color(value,lv_color_hex(w.textColor),0);lv_obj_set_style_text_font(value,&lv_font_montserrat_14,0);lv_obj_set_style_transform_zoom(value,(uint16_t)w.fontSize*256/14,0);lv_obj_set_style_transform_pivot_x(value,LV_PCT(50),0);lv_obj_set_style_transform_pivot_y(value,LV_PCT(50),0);lv_obj_clear_flag(value,LV_OBJ_FLAG_HIDDEN);
    if(visuals_[i]){lv_obj_del(visuals_[i]);visuals_[i]=nullptr;}
    if(w.visualStyle==WIDGET_BAR){visuals_[i]=lv_bar_create(card);lv_obj_set_size(visuals_[i],max(20,w.w-24),w.visualThickness);lv_obj_align(visuals_[i],LV_ALIGN_BOTTOM_MID,0,-5);lv_bar_set_range(visuals_[i],0,1000);lv_obj_set_style_bg_color(visuals_[i],lv_color_hex(w.trackColor),LV_PART_MAIN);lv_obj_set_style_bg_color(visuals_[i],lv_color_hex(w.accentColor),LV_PART_INDICATOR);lv_obj_align(value,LV_ALIGN_CENTER,0,-3);}
    else if(w.visualStyle==WIDGET_GAUGE){int size=max(30,min(w.w-20,w.h-28));visuals_[i]=lv_arc_create(card);lv_obj_set_size(visuals_[i],size,size);lv_obj_align(visuals_[i],LV_ALIGN_CENTER,0,8);lv_arc_set_range(visuals_[i],0,1000);lv_arc_set_bg_angles(visuals_[i],135,45);lv_obj_remove_style(visuals_[i],nullptr,LV_PART_KNOB);lv_obj_clear_flag(visuals_[i],LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_arc_color(visuals_[i],lv_color_hex(w.trackColor),LV_PART_MAIN);lv_obj_set_style_arc_color(visuals_[i],lv_color_hex(w.accentColor),LV_PART_INDICATOR);lv_obj_set_style_arc_width(visuals_[i],w.visualThickness,LV_PART_MAIN);lv_obj_set_style_arc_width(visuals_[i],w.visualThickness,LV_PART_INDICATOR);lv_obj_align(value,LV_ALIGN_CENTER,0,8);lv_obj_move_foreground(value);}
    else if(w.visualStyle==WIDGET_LIGHT){visuals_[i]=lv_obj_create(card);int diameter=min(48,max(12,(int)w.visualThickness*3));lv_obj_set_size(visuals_[i],diameter,diameter);lv_obj_set_style_radius(visuals_[i],LV_RADIUS_CIRCLE,0);lv_obj_set_style_border_width(visuals_[i],0,0);lv_obj_align(visuals_[i],LV_ALIGN_LEFT_MID,4,8);lv_obj_align(value,LV_ALIGN_CENTER,14,8);}
    else lv_obj_align(value,w.valueAlign==0?LV_ALIGN_LEFT_MID:w.valueAlign==2?LV_ALIGN_RIGHT_MID:LV_ALIGN_CENTER,0,8);
    lv_obj_move_foreground(card);
  }
  void applyVisual(size_t i,const Widget&w,float raw,bool numeric,bool active){if(!visuals_[i])return;float lo=0,hi=1;bool ranged=numeric&&widgetVisualRange(w,lo,hi);int level=ranged?(int)constrain((raw-lo)*1000.0f/(hi-lo),0.0f,1000.0f):(active?1000:0);if(w.visualStyle==WIDGET_BAR)lv_bar_set_value(visuals_[i],level,LV_ANIM_OFF);else if(w.visualStyle==WIDGET_GAUGE)lv_arc_set_value(visuals_[i],level);else if(w.visualStyle==WIDGET_LIGHT){float low,high;widgetLightLimits(w,lo,hi,low,high);uint32_t color=raw>=high?w.highColor:raw>=low?w.midColor:w.lowColor;if(!numeric)color=active?w.highColor:w.lowColor;lv_obj_set_style_bg_color(visuals_[i],lv_color_hex(color),0);lv_obj_set_style_shadow_color(visuals_[i],lv_color_hex(color),0);lv_obj_set_style_shadow_width(visuals_[i],w.visualThickness,0);}}
  static lv_obj_t* createRoot(){auto root=lv_obj_create(lv_scr_act());lv_obj_set_size(root,480,480);lv_obj_set_pos(root,0,0);lv_obj_set_style_bg_color(root,lv_color_hex(0x071018),0);lv_obj_set_style_border_width(root,0,0);lv_obj_set_style_pad_all(root,0,0);lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);return root;}
  lv_obj_t*root_=nullptr;std::array<lv_obj_t*,26>cards_{};std::array<lv_obj_t*,26>titles_{};std::array<lv_obj_t*,26>values_{};std::array<lv_obj_t*,26>visuals_{};std::array<std::string,26>ids_{};lv_event_cb_t webAccessCallback_=nullptr;
};}

#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TouchDrvGT911.hpp>
#include <lvgl.h>
#include <Wire.h>

namespace hud {
class DisplayPort {
 public:
  bool begin(){
    Wire.begin(15,7); touch_.setPins(-1,16);
    if(!touch_.begin(Wire,GT911_SLAVE_ADDRESS_L,15,7))return false;
    touch_.setMaxTouchPoint(1); if(!gfx_->begin())return false; lv_init();
    buf1_=(lv_color_t*)heap_caps_malloc(480*60*sizeof(lv_color_t),MALLOC_CAP_DMA);
    buf2_=(lv_color_t*)heap_caps_malloc(480*60*sizeof(lv_color_t),MALLOC_CAP_DMA);if(!buf1_||!buf2_)return false;
    instance_=this;lv_disp_draw_buf_init(&draw_,buf1_,buf2_,480*60);
    lv_disp_drv_init(&disp_);disp_.hor_res=480;disp_.ver_res=480;disp_.flush_cb=flush;disp_.draw_buf=&draw_;lv_disp_drv_register(&disp_);
    lv_indev_drv_init(&input_);input_.type=LV_INDEV_TYPE_POINTER;input_.read_cb=readTouch;lv_indev_drv_register(&input_);return true;
  }
  void update(){lv_timer_handler();}
 private:
  static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*c){uint32_t w=a->x2-a->x1+1,h=a->y2-a->y1+1;instance_->gfx_->draw16bitRGBBitmap(a->x1,a->y1,(uint16_t*)&c->full,w,h);lv_disp_flush_ready(d);}
  static void readTouch(lv_indev_drv_t*,lv_indev_data_t*d){int16_t x[1],y[1];if(instance_->touch_.getPoint(x,y,1)){d->state=LV_INDEV_STATE_PR;d->point.x=479-x[0];d->point.y=479-y[0];}else d->state=LV_INDEV_STATE_REL;}
  Arduino_DataBus* bus_=new Arduino_SWSPI(GFX_NOT_DEFINED,42,2,1,GFX_NOT_DEFINED);
  Arduino_ESP32RGBPanel* rgb_=new Arduino_ESP32RGBPanel(40,39,38,41,46,3,8,18,17,14,13,12,11,10,9,5,45,48,47,21,1,10,8,50,1,10,8,20);
  Arduino_RGB_Display* gfx_=new Arduino_RGB_Display(480,480,rgb_,2,true,bus_,GFX_NOT_DEFINED,st7701_type1_init_operations,sizeof(st7701_type1_init_operations));
  TouchDrvGT911 touch_;lv_disp_draw_buf_t draw_;lv_disp_drv_t disp_;lv_indev_drv_t input_;lv_color_t *buf1_=nullptr,*buf2_=nullptr;static DisplayPort* instance_;
};
inline DisplayPort* DisplayPort::instance_=nullptr;
}

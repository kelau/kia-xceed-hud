#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "SplashAsset.h"

namespace hud {
class SplashScreen {
 public:
  bool show(uint32_t now){
    pixels_=(lv_color_t*)heap_caps_malloc(SPLASH_WIDTH*SPLASH_HEIGHT*sizeof(lv_color_t),MALLOC_CAP_SPIRAM);
    if(!pixels_)return false;
    size_t at=0;
    for(uint32_t i=0;i<SPLASH_RLE_COUNT;i++){
      uint16_t count=SPLASH_RLE[i]>>16,color=SPLASH_RLE[i]&0xffff;
      for(uint16_t n=0;n<count;n++)pixels_[at++].full=color;
    }
    if(at!=SPLASH_WIDTH*SPLASH_HEIGHT){heap_caps_free(pixels_);pixels_=nullptr;return false;}
    canvas_=lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(canvas_,pixels_,SPLASH_WIDTH,SPLASH_HEIGHT,LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(canvas_,0,0);lv_obj_clear_flag(canvas_,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(canvas_);shownAt_=now;return true;
  }
  void update(uint32_t now){
    if(!canvas_||now-shownAt_<2500)return;
    lv_obj_del(canvas_);canvas_=nullptr;heap_caps_free(pixels_);pixels_=nullptr;
  }
 private:
  lv_obj_t*canvas_=nullptr;lv_color_t*pixels_=nullptr;uint32_t shownAt_=0;
};
}

#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TouchDrvGT911.hpp>
#include <lvgl.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <driver/gpio.h>
#include <freertos/semphr.h>
#include <array>

namespace hud {
class DisplayPort {
 public:
  static constexpr size_t SCREENSHOT_BYTES=480*480*sizeof(uint16_t);
  bool begin(){
    Wire.begin(15,7); touch_.setPins(-1,16);
    if(!touch_.begin(Wire,GT911_SLAVE_ADDRESS_L,15,7))return false;
    touch_.setMaxTouchPoint(1);if(!gfx_->begin()||!rgb_->getFrameBuffers(&front_,&back_))return false;memset(front_,0,SCREENSHOT_BYTES);memset(back_,0,SCREENSHOT_BYTES);Cache_WriteBack_Addr((uint32_t)front_,SCREENSHOT_BYTES);Cache_WriteBack_Addr((uint32_t)back_,SCREENSHOT_BYTES);vsyncSemaphore_=xSemaphoreCreateBinary();if(vsyncSemaphore_){gpio_input_enable(GPIO_NUM_39);gpio_set_intr_type(GPIO_NUM_39,GPIO_INTR_NEGEDGE);esp_err_t service=gpio_install_isr_service(ESP_INTR_FLAG_IRAM);if(service==ESP_OK||service==ESP_ERR_INVALID_STATE)vsyncReady_=gpio_isr_handler_add(GPIO_NUM_39,onVsync,this)==ESP_OK;}lv_init();
    buf1_=(lv_color_t*)heap_caps_malloc(480*60*sizeof(lv_color_t),MALLOC_CAP_DMA);frame_=(uint16_t*)heap_caps_calloc(480*480,sizeof(uint16_t),MALLOC_CAP_SPIRAM);if(!buf1_||!frame_)return false;
    instance_=this;lv_disp_draw_buf_init(&draw_,buf1_,nullptr,480*60);
    lv_disp_drv_init(&disp_);disp_.hor_res=480;disp_.ver_res=480;disp_.flush_cb=flush;disp_.draw_buf=&draw_;lv_disp_drv_register(&disp_);
    lv_indev_drv_init(&input_);input_.type=LV_INDEV_TYPE_POINTER;input_.read_cb=readTouch;input_.long_press_time=700;lv_indev_drv_register(&input_);return true;
  }
  void update(){uint32_t now=millis(),elapsed=now-lastTickMs_;if(elapsed){lv_tick_inc(elapsed);lastTickMs_=now;}lv_timer_handler();}
  void presentAtomic(){deferCommit_=true;pendingOverflow_=true;lv_refr_now(lv_disp_get_default());deferCommit_=false;if(pendingCount_||pendingOverflow_)commitPending();}
  void setBrightness(uint8_t percent){
    percent=constrain(percent,5,100);
    if(!dimmer_){
      dimmer_=lv_obj_create(lv_scr_act());
      lv_obj_set_pos(dimmer_,0,0);lv_obj_set_size(dimmer_,480,480);
      lv_obj_set_style_bg_color(dimmer_,lv_color_black(),0);
      lv_obj_set_style_border_width(dimmer_,0,0);lv_obj_set_style_radius(dimmer_,0,0);
      lv_obj_set_style_pad_all(dimmer_,0,0);
      lv_obj_clear_flag(dimmer_,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_set_style_bg_opa(dimmer_,(lv_opa_t)((100-percent)*255/100),0);
    lv_obj_move_foreground(dimmer_);
  }
  void clear(){if(front_&&back_){memset(front_,0,SCREENSHOT_BYTES);memset(back_,0,SCREENSHOT_BYTES);Cache_WriteBack_Addr((uint32_t)front_,SCREENSHOT_BYTES);Cache_WriteBack_Addr((uint32_t)back_,SCREENSHOT_BYTES);}}
  void addDiagnostics(JsonObject output)const{output["presentationMode"]="double-buffer-vsync-swap";output["atomicSwaps"]=commits_;output["swapFailures"]=swapFailures_;output["vsyncTimeouts"]=vsyncTimeouts_;output["vsyncWaitLastUs"]=vsyncWaitLastUs_;output["vsyncWaitAverageUs"]=vsyncWaitAverageUs_;output["vsyncWaitPeakUs"]=vsyncWaitPeakUs_;output["commitLastUs"]=commitLastUs_;output["commitAverageUs"]=commitAverageUs_;output["commitPeakUs"]=commitPeakUs_;}
  size_t readScreenshot(uint8_t*buffer,size_t maximum,size_t index)const{if(!frame_||index>=SCREENSHOT_BYTES)return 0;size_t count=min(maximum,SCREENSHOT_BYTES-index);memcpy(buffer,(const uint8_t*)frame_+index,count);return count;}
 private:
  static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*c){uint32_t w=a->x2-a->x1+1,h=a->y2-a->y1+1;for(uint32_t row=0;row<h;row++)memcpy(instance_->frame_+(a->y1+row)*480+a->x1,(uint16_t*)&c[row*w].full,w*sizeof(uint16_t));if(instance_->pendingCount_<instance_->pendingAreas_.size())instance_->pendingAreas_[instance_->pendingCount_++]=*a;else instance_->pendingOverflow_=true;if(lv_disp_flush_is_last(d)&&!instance_->deferCommit_)instance_->commitPending();lv_disp_flush_ready(d);}
  static void IRAM_ATTR onVsync(void*arg){auto self=(DisplayPort*)arg;BaseType_t wake=pdFALSE;xSemaphoreGiveFromISR(self->vsyncSemaphore_,&wake);if(wake)portYIELD_FROM_ISR();}
  bool waitVsync(){if(!vsyncReady_)return false;xSemaphoreTake(vsyncSemaphore_,0);return xSemaphoreTake(vsyncSemaphore_,pdMS_TO_TICKS(40))==pdTRUE;}
  void copyLogicalAreaToBack(const lv_area_t&a){for(int32_t y=a.y1;y<=a.y2;y++)for(int32_t x=a.x1;x<=a.x2;x++)back_[(479-y)*480+(479-x)]=frame_[y*480+x];}
  void copyPhysicalArea(uint16_t*destination,const uint16_t*source,const lv_area_t&a){int32_t x1=479-a.x2,x2=479-a.x1;for(int32_t y=479-a.y2;y<=479-a.y1;y++)memcpy(destination+y*480+x1,source+y*480+x1,(x2-x1+1)*sizeof(uint16_t));}
  void commitPending(){uint32_t commitStarted=micros();if(pendingOverflow_){for(int32_t y=0;y<480;y++)for(int32_t x=0;x<480;x++)back_[(479-y)*480+(479-x)]=frame_[y*480+x];}else for(size_t n=0;n<pendingCount_;n++)copyLogicalAreaToBack(pendingAreas_[n]);Cache_WriteBack_Addr((uint32_t)back_,SCREENSHOT_BYTES);uint32_t waitStarted=micros();bool requested=rgb_->switchFrameBuffer(back_,480,480),synced=requested&&waitVsync();uint32_t waited=micros()-waitStarted;if(requested){std::swap(front_,back_);if(pendingOverflow_)memcpy(back_,front_,SCREENSHOT_BYTES);else for(size_t n=0;n<pendingCount_;n++)copyPhysicalArea(back_,front_,pendingAreas_[n]);commits_++;}else swapFailures_++;uint32_t committed=micros()-commitStarted;pendingCount_=0;pendingOverflow_=false;if(!synced)vsyncTimeouts_++;vsyncWaitLastUs_=waited;vsyncWaitAverageUs_=vsyncWaitAverageUs_?vsyncWaitAverageUs_*.9f+waited*.1f:waited;vsyncWaitPeakUs_=max(vsyncWaitPeakUs_,(float)waited);commitLastUs_=committed;commitAverageUs_=commitAverageUs_?commitAverageUs_*.9f+committed*.1f:committed;commitPeakUs_=max(commitPeakUs_,(float)committed);}
  static void readTouch(lv_indev_drv_t*,lv_indev_data_t*d){int16_t x[1],y[1];if(instance_->touch_.getPoint(x,y,1)){d->state=LV_INDEV_STATE_PR;d->point.x=479-x[0];d->point.y=479-y[0];}else d->state=LV_INDEV_STATE_REL;}
  Arduino_DataBus* bus_=new Arduino_SWSPI(GFX_NOT_DEFINED,42,2,1,GFX_NOT_DEFINED);
  Arduino_ESP32RGBPanel* rgb_=new Arduino_ESP32RGBPanel(40,39,38,41,46,3,8,18,17,14,13,12,11,10,9,5,45,48,47,21,1,10,8,50,1,10,8,20,0,8000000,false,0,0,0);
  Arduino_RGB_Display* gfx_=new Arduino_RGB_Display(480,480,rgb_,2,true,bus_,GFX_NOT_DEFINED,st7701_type1_init_operations,sizeof(st7701_type1_init_operations));
  TouchDrvGT911 touch_;lv_disp_draw_buf_t draw_;lv_disp_drv_t disp_;lv_indev_drv_t input_;lv_color_t *buf1_=nullptr;uint16_t*frame_=nullptr,*front_=nullptr,*back_=nullptr;lv_obj_t*dimmer_=nullptr;SemaphoreHandle_t vsyncSemaphore_=nullptr;bool vsyncReady_=false,pendingOverflow_=false,deferCommit_=false;std::array<lv_area_t,64>pendingAreas_{};size_t pendingCount_=0;uint32_t lastTickMs_=0,commits_=0,swapFailures_=0,vsyncTimeouts_=0;float vsyncWaitLastUs_=0,vsyncWaitAverageUs_=0,vsyncWaitPeakUs_=0,commitLastUs_=0,commitAverageUs_=0,commitPeakUs_=0;static DisplayPort* instance_;
};
inline DisplayPort* DisplayPort::instance_=nullptr;
}

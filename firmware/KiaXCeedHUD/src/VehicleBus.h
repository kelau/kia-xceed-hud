#pragma once
#include <Arduino.h>
#include <driver/twai.h>
#include "Core.h"
namespace hud {
class VehicleBus {
 public:
  bool begin(bool listenOnly,uint16_t rate=500){twai_general_config_t g=TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_6,GPIO_NUM_0,listenOnly?TWAI_MODE_LISTEN_ONLY:TWAI_MODE_NORMAL);twai_timing_config_t t=TWAI_TIMING_CONFIG_500KBITS();if(rate==250){twai_timing_config_t slow=TWAI_TIMING_CONFIG_250KBITS();t=slow;}twai_filter_config_t f=TWAI_FILTER_CONFIG_ACCEPT_ALL();return twai_driver_install(&g,&t,&f)==ESP_OK&&twai_start()==ESP_OK;}
  bool receive(CanFrame& f){twai_message_t m;if(twai_receive(&m,0)!=ESP_OK)return false;f.id=m.identifier;f.dlc=m.data_length_code;f.ms=millis();for(int i=0;i<f.dlc;i++)f.data[i]=m.data[i];return true;}
  bool requestPid(uint8_t pid){twai_message_t m{};m.identifier=0x7DF;m.data_length_code=8;m.data[0]=2;m.data[1]=1;m.data[2]=pid;return twai_transmit(&m,pdMS_TO_TICKS(10))==ESP_OK;}
};}

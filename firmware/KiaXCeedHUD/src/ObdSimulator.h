#pragma once
#include "Core.h"
namespace hud {
inline CanFrame simulatedFrame(uint32_t now,uint8_t sequence){
  CanFrame f;f.id=0x7E8;f.ms=now;uint8_t slot=sequence%10;
  if(slot==0){f.dlc=7;f.data={6,0x41,0x00,0x18,0x1B,0x80,0x03};return f;}
  uint8_t pid=0x0D,value=0;uint16_t raw=0;
  switch(slot){
    case 1:pid=0x0D;value=20+(now/200)%81;break;
    case 2:pid=0x0C;raw=(900+(now/10)%3200)*4;break;
    case 3:pid=0x05;value=105+(now/5000)%15;break;
    case 4:pid=0x04;value=40+(now/80)%180;break;
    case 5:pid=0x2F;value=170-(now/5000)%40;break;
    case 6:pid=0x0F;value=65+(now/3000)%20;break;
    case 7:pid=0x11;value=30+(now/90)%190;break;
    case 8:pid=0x42;raw=12300+(now/100)%800;break;
    default:pid=0x5C;value=100+(now/4000)%20;
  }
  bool two=pid==0x0C||pid==0x42;f.dlc=two?5:4;f.data[0]=f.dlc-1;f.data[1]=0x41;f.data[2]=pid;
  if(two){f.data[3]=raw>>8;f.data[4]=raw;}else f.data[3]=value;return f;
}
}

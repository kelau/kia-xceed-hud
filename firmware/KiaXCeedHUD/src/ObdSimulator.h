#pragma once
#include "Core.h"
namespace hud {
inline constexpr uint32_t SIMULATED_CAN_FPS=100;
inline constexpr uint32_t SIMULATED_CAN_PERIOD_US=1000000/SIMULATED_CAN_FPS;
inline constexpr uint8_t SIMULATED_CAN_MAX_CATCH_UP=64;

inline CanFrame simulatedFrame(uint32_t now,uint32_t sequence){
  CanFrame f;f.ms=now;bool obdFrame=(sequence&1u)==0;uint8_t slot=(sequence/2)%19+1;
  if(!obdFrame){
    static constexpr uint16_t ids[19]={0x100,0x120,0x130,0x153,0x164,0x180,0x1A0,0x1B0,0x1C4,0x200,0x220,0x260,0x280,0x2A0,0x300,0x320,0x340,0x3A0,0x420};
    f.id=ids[slot-1];f.dlc=8;uint32_t phase=now/5+sequence;
    for(uint8_t i=0;i<8;i++)f.data[i]=(uint8_t)((phase*(i+3)+(slot*29)+(i*i*7))>>((i&1)*3));
    f.data[0]=(uint8_t)(20+(now/200)%81);f.data[1]=(uint8_t)((900+(now/10)%3200)>>4);f.data[7]=(uint8_t)sequence;
    return f;
  }
  f.id=0x7E8;uint8_t pidSlot=(sequence/2)%10;
  if(pidSlot==0){f.dlc=7;f.data={6,0x41,0x00,0x18,0x1B,0x80,0x03};return f;}
  uint8_t pid=0x0D,value=0;uint16_t raw=0;
  switch(pidSlot){
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

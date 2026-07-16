#pragma once
#include "Core.h"
namespace hud { inline CanFrame simulatedFrame(uint32_t now,uint8_t sequence){uint8_t pid=0x0D;uint16_t raw=0;uint8_t value=0;switch(sequence%5){case 0:pid=0x0D;value=20+(now/200)%81;break;case 1:pid=0x0C;raw=(900+(now/10)%3200)*4;break;case 2:pid=0x05;value=105+(now/5000)%15;break;case 3:pid=0x04;value=40+(now/80)%180;break;default:pid=0x2F;value=170-(now/5000)%40;}CanFrame f;f.id=0x7E8;f.dlc=pid==0x0C?5:4;f.data[0]=f.dlc-1;f.data[1]=0x41;f.data[2]=pid;if(pid==0x0C){f.data[3]=raw>>8;f.data[4]=raw;}else f.data[3]=value;f.ms=now;return f;} }

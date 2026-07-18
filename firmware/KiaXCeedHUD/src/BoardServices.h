#pragma once
#include <Arduino.h>
#include <SD_MMC.h>
#include <Wire.h>

namespace hud {
class BoardServices {
 public:
  bool begin(){
    expanderPresent_=readRegister(TCA9554,3,expanderConfig_)&&readRegister(TCA9554,1,expanderOutput_);
    if(expanderPresent_){expanderConfig_&=~(1u<<5);expanderOutput_&=~(1u<<5);writeRegister(TCA9554,1,expanderOutput_);writeRegister(TCA9554,3,expanderConfig_);}
    Wire.beginTransmission(SW6106);batteryPresent_=Wire.endTransmission()==0;
    if(batteryPresent_)writeRegister(SW6106,0x38,0x0A);
    SD_MMC.setPins(2,1,4);sdMounted_=SD_MMC.begin("/sdcard",true);if(sdMounted_){SD_MMC.mkdir("/hud");SD_MMC.mkdir("/hud/fonts");SD_MMC.mkdir("/hud/icons");writeReadme();}
    return expanderPresent_||batteryPresent_||sdMounted_;
  }
  void update(uint32_t now){
    if(beepUntil_&&(int32_t)(now-beepUntil_)>=0){setBuzzer(false);beepUntil_=0;}
    if(batteryPresent_&&now-lastBatteryRead_>=1000){lastBatteryRead_=now;readRegister(SW6106,0xB0,batteryStatus_);}
  }
  void beep(uint32_t durationMs){if(!expanderPresent_)return;setBuzzer(true);beepUntil_=millis()+constrain(durationMs,50u,10000u);}
  bool sdMounted()const{return sdMounted_;}bool batteryPresent()const{return batteryPresent_;}uint8_t batteryStatus()const{return batteryStatus_;}bool buzzerPresent()const{return expanderPresent_;}
 private:
  static constexpr uint8_t TCA9554=0x20,SW6106=0x3C;
  bool readRegister(uint8_t device,uint8_t reg,uint8_t&value){Wire.beginTransmission(device);Wire.write(reg);if(Wire.endTransmission(false)!=0)return false;if(Wire.requestFrom(device,(uint8_t)1)!=(uint8_t)1)return false;value=Wire.read();return true;}
  bool writeRegister(uint8_t device,uint8_t reg,uint8_t value){Wire.beginTransmission(device);Wire.write(reg);Wire.write(value);return Wire.endTransmission()==0;}
  void setBuzzer(bool on){if(on)expanderOutput_|=1u<<5;else expanderOutput_&=~(1u<<5);writeRegister(TCA9554,1,expanderOutput_);}
  void writeReadme(){if(SD_MMC.exists("/hud/README.txt"))return;File f=SD_MMC.open("/hud/README.txt",FILE_WRITE);if(!f)return;f.println("Kia XCeed HUD resource card");f.println("fonts/: reserved for LVGL binary fonts (.bin)");f.println("icons/: HUD RGB565 resources uploaded by the Web UI (.rgb565)");f.close();}
  bool expanderPresent_=false,batteryPresent_=false,sdMounted_=false;uint8_t expanderConfig_=0xff,expanderOutput_=0,batteryStatus_=0;uint32_t beepUntil_=0,lastBatteryRead_=0;
};
}

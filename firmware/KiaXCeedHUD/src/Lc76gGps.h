#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <stdlib.h>
#include <string.h>
#include "Core.h"

namespace hud {

// Non-blocking Quectel LC76G(AB) I2C transport. Reads are deliberately bounded
// so the shared GT911 touch bus remains responsive.
class Lc76gGps {
 public:
  void begin(TwoWire& wire=Wire){wire_=&wire;state_=QUERY_LENGTH;nextActionMs_=millis()+100;}
  void update(Telemetry& telemetry,uint32_t now){
    if(!wire_||(int32_t)(now-nextActionMs_)<0)return;
    switch(state_){
      case QUERY_LENGTH:
        if(sendReadCommand(0x0008,4)){state_=READ_LENGTH;nextActionMs_=now+10;}else retry(now);
        break;
      case READ_LENGTH:{
        uint8_t raw[4];if(!receive(raw,sizeof(raw))){retry(now);break;}
        uint32_t available=(uint32_t)raw[0]|((uint32_t)raw[1]<<8)|((uint32_t)raw[2]<<16)|((uint32_t)raw[3]<<24);
        pending_=(uint16_t)(available<MAX_READ?available:MAX_READ);
        if(!pending_){present_=true;state_=QUERY_LENGTH;nextActionMs_=now+50;break;}
        if(sendReadCommand(0x2000,pending_)){state_=READ_DATA;nextActionMs_=now+10;}else retry(now);
        break;
      }
      case READ_DATA:{
        uint8_t data[MAX_READ];if(!receive(data,pending_)){retry(now);break;}
        for(uint16_t i=0;i<pending_;i++)consume((char)data[i],telemetry,now);
        bytesRead_+=pending_;errors_=0;state_=QUERY_LENGTH;nextActionMs_=now+2;break;
      }
    }
  }
  bool present()const{return present_;}
  uint32_t bytesRead()const{return bytesRead_;}
  uint32_t errors()const{return totalErrors_;}
  uint32_t lastSentenceMs()const{return lastSentenceMs_;}

 private:
  enum State:uint8_t{QUERY_LENGTH,READ_LENGTH,READ_DATA};
  static constexpr uint8_t COMMAND_ADDRESS=0x50,READ_ADDRESS=0x54;
  static constexpr uint16_t MAX_READ=96;
  TwoWire* wire_=nullptr;State state_=QUERY_LENGTH;
  uint32_t nextActionMs_=0,bytesRead_=0,totalErrors_=0,lastSentenceMs_=0;
  uint16_t pending_=0,lineLength_=0;uint8_t errors_=0;bool present_=false;char line_[128]{};

  bool sendReadCommand(uint16_t offset,uint32_t length){
    uint8_t command[8]={(uint8_t)offset,(uint8_t)(offset>>8),0x51,0xAA,(uint8_t)length,(uint8_t)(length>>8),(uint8_t)(length>>16),(uint8_t)(length>>24)};
    wire_->beginTransmission(COMMAND_ADDRESS);wire_->write(command,sizeof(command));return wire_->endTransmission()==0;
  }
  bool receive(uint8_t* destination,uint16_t length){
    if(wire_->requestFrom(READ_ADDRESS,(uint8_t)length)!=(uint8_t)length)return false;
    for(uint16_t i=0;i<length;i++)destination[i]=(uint8_t)wire_->read();present_=true;return true;
  }
  void retry(uint32_t now){totalErrors_++;errors_++;state_=QUERY_LENGTH;nextActionMs_=now+(errors_>5?1000:100);}
  static char* field(char*& cursor){if(!cursor)return nullptr;char* value=cursor;char* comma=strchr(cursor,',');if(comma){*comma='\0';cursor=comma+1;}else cursor=nullptr;return value;}
  static float coordinate(const char* value,char hemisphere){if(!value||!*value)return 0;double packed=strtod(value,nullptr);int degrees=(int)(packed/100.0);double result=degrees+(packed-degrees*100.0)/60.0;return (hemisphere=='S'||hemisphere=='W')?-(float)result:(float)result;}
  static bool checksumValid(const char* sentence){if(!sentence||sentence[0]!='$')return false;const char* star=strchr(sentence,'*');if(!star||strlen(star)<3)return false;uint8_t sum=0;for(const char*p=sentence+1;p<star;p++)sum^=(uint8_t)*p;return sum==(uint8_t)strtoul(star+1,nullptr,16);}
  void parse(char* sentence,Telemetry& t,uint32_t now){
    if(!checksumValid(sentence))return;char* star=strchr(sentence,'*');*star='\0';char* cursor=sentence;char* type=field(cursor);if(!type)return;size_t n=strlen(type);
    if(n>=3&&!strcmp(type+n-3,"RMC")){
      field(cursor);char* status=field(cursor);char* latitude=field(cursor);char* ns=field(cursor);char* longitude=field(cursor);char* ew=field(cursor);char* knots=field(cursor);char* course=field(cursor);
      bool fix=status&&status[0]=='A';t.gpsFix=fix;if(fix){t.latitude=coordinate(latitude,ns?ns[0]:0);t.longitude=coordinate(longitude,ew?ew[0]:0);t.gpsSpeedKph=knots?strtof(knots,nullptr)*1.852f:0;t.gpsHeadingDeg=course?strtof(course,nullptr):0;t.lastGpsMs=now;}
    }else if(n>=3&&!strcmp(type+n-3,"GGA")){
      field(cursor);field(cursor);field(cursor);field(cursor);field(cursor);char* quality=field(cursor);char* satellites=field(cursor);char* hdop=field(cursor);char* altitude=field(cursor);
      t.gpsFix=quality&&atoi(quality)>0;t.gpsSatellites=satellites?(uint8_t)atoi(satellites):0;t.gpsHdop=hdop?strtof(hdop,nullptr):0;t.gpsAltitudeM=altitude?strtof(altitude,nullptr):0;if(t.gpsFix)t.lastGpsMs=now;
    }lastSentenceMs_=now;
  }
  void consume(char c,Telemetry& telemetry,uint32_t now){
    if(c=='$'){lineLength_=0;line_[lineLength_++]=c;return;}if(!lineLength_)return;
    if(c=='\n'){line_[lineLength_]='\0';parse(line_,telemetry,now);lineLength_=0;return;}
    if(c!='\r'&&lineLength_<sizeof(line_)-1)line_[lineLength_++]=c;else if(lineLength_>=sizeof(line_)-1)lineLength_=0;
  }
};
}

#pragma once
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD_MMC.h>

namespace hud::graphic_cache {
static constexpr const char* DATA_PATH="/splash.rgb565";
static constexpr const char* TEMP_PATH="/splash.tmp";
static constexpr const char* META_PATH="/splash-resource.txt";
static constexpr const char* META_TEMP_PATH="/splash-resource.tmp";

inline bool resourcePath(const String&path){return path.startsWith("/hud/graphics/")||path.startsWith("/hud/icons/");}

inline bool validRgb565(File&file,uint16_t&width,uint16_t&height,size_t&bytes){
  if(!file||file.isDirectory()||file.size()<4)return false;
  file.seek(0);int w0=file.read(),w1=file.read(),h0=file.read(),h1=file.read();
  if(w0<0||w1<0||h0<0||h1<0)return false;
  width=(uint16_t)(w0|(w1<<8));height=(uint16_t)(h0|(h1<<8));bytes=(size_t)width*height*2;
  file.seek(0);return width&&height&&width<=480&&height<=480&&file.size()==bytes+4;
}

inline String configuredResource(){
  if(!LittleFS.begin(true))return "";File meta=LittleFS.open(META_PATH,"r");if(!meta)return "";
  String path=meta.readString();meta.close();path.trim();return path;
}

inline bool open(const String&resource,File&file){
  if(!resourcePath(resource)||configuredResource()!=resource)return false;
  file=LittleFS.open(DATA_PATH,"r");uint16_t width,height;size_t bytes;
  if(!validRgb565(file,width,height,bytes)){if(file)file.close();return false;}return true;
}

inline bool refresh(const String&resource){
  if(!resourcePath(resource)||SD_MMC.cardType()==CARD_NONE)return false;
  File source=SD_MMC.open(resource,"r");uint16_t width,height;size_t bytes;
  if(!validRgb565(source,width,height,bytes))return false;
  if(!LittleFS.begin(true)){source.close();return false;}
  LittleFS.remove(TEMP_PATH);File target=LittleFS.open(TEMP_PATH,"w");if(!target){source.close();return false;}
  uint8_t buffer[1024];size_t remaining=bytes+4,written=0;
  while(remaining){size_t count=source.read(buffer,min(remaining,sizeof(buffer)));if(!count)break;written+=target.write(buffer,count);remaining-=count;}
  source.close();target.close();if(remaining||written!=bytes+4){LittleFS.remove(TEMP_PATH);return false;}
  LittleFS.remove(META_TEMP_PATH);File meta=LittleFS.open(META_TEMP_PATH,"w");if(!meta||meta.print(resource)!=resource.length()){if(meta)meta.close();LittleFS.remove(TEMP_PATH);LittleFS.remove(META_TEMP_PATH);return false;}meta.close();
  LittleFS.remove(DATA_PATH);if(!LittleFS.rename(TEMP_PATH,DATA_PATH)){LittleFS.remove(TEMP_PATH);LittleFS.remove(META_TEMP_PATH);return false;}
  LittleFS.remove(META_PATH);if(!LittleFS.rename(META_TEMP_PATH,META_PATH)){LittleFS.remove(META_TEMP_PATH);LittleFS.remove(DATA_PATH);return false;}
  return true;
}
}

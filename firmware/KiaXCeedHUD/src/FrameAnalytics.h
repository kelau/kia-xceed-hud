#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <new>
#include "Core.h"

namespace hud {
inline constexpr size_t FRAME_TYPE_CAPACITY=64;
inline constexpr size_t FRAME_HISTORY_BUCKETS=FRAME_HISTORY_MS/1000+1;

struct FrameBucket {
  uint32_t second=UINT32_MAX,firstMs=0,lastMs=0,count=0;
  float minimum=0,maximum=0,latest=0;
  bool metric=false;
};

struct FrameTypeSummary {
  uint32_t count=0,firstMs=0,lastMs=0;
  float minimum=0,maximum=0,latest=0;
  bool metric=false;
};

struct FrameTypeStats {
  bool used=false,hasPrevious=false,metric=false;
  char key[16]={},unit[12]={};
  CanFrame latest{},previous{};
  uint32_t lastChangeMs=0;
  float realMin=0,realMax=0;
  FrameBucket buckets[FRAME_HISTORY_BUCKETS]{};
};

class FrameAnalytics {
 public:
  void record(const CanFrame& frame){
    char key[16];makeKey(frame,key,sizeof(key));
    FrameTypeStats* type=find(key);
    if(!type)type=allocate(key,frame.ms);
    if(!type)return;
    if(!type->hasPrevious||frame.dlc!=type->previous.dlc||memcmp(frame.data.data(),type->previous.data.data(),frame.dlc)!=0)type->lastChangeMs=frame.ms;
    type->previous=frame;type->hasPrevious=true;type->latest=frame;
    float value=0,realMin=0,realMax=0;const char* unit="";
    bool metric=frameMetric(frame,value,realMin,realMax,unit);
    if(metric){type->metric=true;type->realMin=realMin;type->realMax=realMax;snprintf(type->unit,sizeof(type->unit),"%s",unit);}
    uint32_t second=frame.ms/1000;FrameBucket& bucket=type->buckets[second%FRAME_HISTORY_BUCKETS];
    if(bucket.second!=second){bucket={};bucket.second=second;bucket.firstMs=frame.ms;}
    bucket.lastMs=frame.ms;bucket.count++;
    if(metric){if(!bucket.metric){bucket.minimum=bucket.maximum=value;bucket.metric=true;}else{bucket.minimum=std::min(bucket.minimum,value);bucket.maximum=std::max(bucket.maximum,value);}bucket.latest=value;}
  }
  FrameTypeStats* find(const char* key){for(auto& type:types_)if(type.used&&!strcmp(type.key,key))return &type;return nullptr;}
  const FrameTypeStats* find(const char* key)const{for(const auto& type:types_)if(type.used&&!strcmp(type.key,key))return &type;return nullptr;}
  FrameTypeSummary summarize(const FrameTypeStats& type,uint32_t nowMs)const{
    FrameTypeSummary out;
    for(const auto& bucket:type.buckets){if(bucket.second==UINT32_MAX||nowMs-bucket.lastMs>FRAME_HISTORY_MS)continue;if(!out.count)out.firstMs=bucket.firstMs;else out.firstMs=std::min(out.firstMs,bucket.firstMs);out.lastMs=std::max(out.lastMs,bucket.lastMs);out.count+=bucket.count;if(bucket.metric){if(!out.metric){out.minimum=bucket.minimum;out.maximum=bucket.maximum;out.metric=true;}else{out.minimum=std::min(out.minimum,bucket.minimum);out.maximum=std::max(out.maximum,bucket.maximum);}if(bucket.lastMs>=out.lastMs)out.latest=bucket.latest;}}
    if(type.metric){float v,a,b;const char*u;if(frameMetric(type.latest,v,a,b,u))out.latest=v;}
    return out;
  }
  FrameTypeStats* data(){return types_;}const FrameTypeStats* data()const{return types_;}
  static void makeKey(const CanFrame& frame,char* output,size_t size){if(frame.id>=0x7E8&&frame.id<=0x7EF&&frame.dlc>=3&&frame.data[1]==0x41)snprintf(output,size,"%03X:%02X",(unsigned)frame.id,frame.data[2]);else snprintf(output,size,"%03X",(unsigned)frame.id);}
 private:
  FrameTypeStats* allocate(const char* key,uint32_t now){FrameTypeStats* slot=nullptr;for(auto& type:types_)if(!type.used){slot=&type;break;}if(!slot){slot=&types_[0];for(auto& type:types_)if(type.latest.ms<slot->latest.ms)slot=&type;if(now-slot->latest.ms<=FRAME_HISTORY_MS)return nullptr;}slot->~FrameTypeStats();new(slot)FrameTypeStats();slot->used=true;snprintf(slot->key,sizeof(slot->key),"%s",key);return slot;}
  FrameTypeStats types_[FRAME_TYPE_CAPACITY]{};
};
}

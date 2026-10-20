#pragma once
#include "WickedEngine.h"
#include <vector>
#include <cmath>
namespace stalthos::engine {
// Original deterministic generated stand-in, not a recording or measured foot contact.
inline std::vector<uint8_t> bodyRustleWave(){
 const uint32_t rate=22050,count=3087;std::vector<uint8_t> bytes;
 auto u16=[&](uint16_t v){bytes.push_back(uint8_t(v));bytes.push_back(uint8_t(v>>8));};auto u32=[&](uint32_t v){u16(uint16_t(v));u16(uint16_t(v>>16));};auto tag=[&](const char* s){for(int i=0;i<4;++i)bytes.push_back(uint8_t(s[i]));};
 tag("RIFF");u32(36+count*2);tag("WAVE");tag("fmt ");u32(16);u16(1);u16(1);u32(rate);u32(rate*2);u16(2);u16(16);tag("data");u32(count*2);
 uint32_t seed=42;double previous=0;
 for(uint32_t i=0;i<count;++i){seed=1664525*seed+1013904223;const double noise=double(seed>>8)/8388607.5-1,t=double(i)/count,envelope=std::pow(std::sin(3.141592653589793*t),2);const double sample=.22*envelope*(.7*noise+.3*(noise-previous));previous=noise;u16(uint16_t(int16_t(std::clamp(sample,-1.0,1.0)*32767)));}
 return bytes;
}
}

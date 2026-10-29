#pragma once
#include <algorithm>
#include <cmath>

namespace rarestage::patterns {
struct float2 { float x,y; };
namespace {
using std::sin;using std::cos;using std::floor;using std::abs;using std::atan2;using std::fmod;using std::pow;
float saturate(float v){return std::clamp(v,0.f,1.f);}
float length(float2 v){return std::hypot(v.x,v.y);}
float max(float a,float b){return std::max(a,b);}
float frac(float v){return v-std::floor(v);}
float smoothstep(float a,float b,float v){float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
float liquidEase(float v){v=std::clamp(v,0.f,1.f);return v*v*v*(v*(v*6-15)+10);}
#define RS_PATTERN_SOURCE(...) __VA_ARGS__
#include "stage_patterns_shared.inc"
#undef RS_PATTERN_SOURCE
}
struct Field {float row,column,signal;};
inline Field sample(float seed,float2 id,float beat){
 const float clock=beat*.125f,blend=motifBlend(clock);
 const float a=authoredMotif(std::floor(clock),seed),b=authoredMotif(std::floor(clock)+1,seed);
 const float2 center{(id.x+.5f)/10,(id.y+.5f)/9};
 const auto mix=[&](float x,float y){return x+(y-x)*blend;};
 return {mix(rowGesture(a,id.y,beat),rowGesture(b,id.y,beat)),
         mix(columnGesture(a,id.x,beat),columnGesture(b,id.x,beat)),
         mix(wallPattern(a,id,center,beat*.785398f),wallPattern(b,id,center,beat*.785398f))};
}
}

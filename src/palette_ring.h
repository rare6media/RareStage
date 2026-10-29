#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace rarestage::show {
struct PaletteStop {const wchar_t* name;float primary,accent,spectral;};
#define RS_WIDE_IMPL(s) L##s
#define RS_WIDE(s) RS_WIDE_IMPL(s)
inline constexpr auto Palettes=std::array{
#define RS_PALETTE(id,name,p,a,s) PaletteStop{RS_WIDE(name),float(p),float(a),float(s)},
#include "palette_ring.inc"
#undef RS_PALETTE
};
#undef RS_WIDE
#undef RS_WIDE_IMPL
inline constexpr int PaletteCount=int(Palettes.size());
static_assert(PaletteCount==24,"palette_ring_shader.h declares the same ring size");
inline float WrapPalette(float position){const float n=float(PaletteCount);position=std::fmod(position,n);return position<0?position+n:position;}
// Signed shortest travel from a to b around the ring, in stops.
inline float PaletteDelta(float a,float b){float d=std::fmod(b-a,float(PaletteCount));if(d>PaletteCount*.5f)d-=PaletteCount;if(d<-PaletteCount*.5f)d+=PaletteCount;return d;}
inline int PaletteDistance(int a,int b){return int(std::lround(std::abs(PaletteDelta(float(a),float(b)))));}
// Same hue model and luminance balance as the shader, used for Console swatches.
inline std::array<float,3> HueColor(float degrees,float boost=1.f){
 const float h=degrees/360.f;auto channel=[&](float offset){float x=h+offset;x-=std::floor(x);return std::clamp(std::abs(x*6-3)-1,0.f,1.f);};
 std::array<float,3> rgb{channel(0),channel(2.f/3),channel(1.f/3)};
 const float luma=rgb[0]*.2126f+rgb[1]*.7152f+rgb[2]*.0722f,scale=1+(.60f/std::max(luma,.08f)-1)*.65f;
 for(auto&v:rgb)v=std::min(1.2f,(v*.98f+.02f)*scale*boost);return rgb;
}
inline std::array<float,3> PaletteColor(int id,int role){
 const auto&stop=Palettes[size_t(std::clamp(id,0,PaletteCount-1))];
 return HueColor(role==0?stop.primary:role==1?stop.accent:stop.spectral,role==1?1.08f:role==2?.92f:1.f);
}
}

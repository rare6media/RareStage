#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace rarestage::show {
// Procedural colour harmony: Color(t) = a + b * cos(2*pi*(c*t + d)).
// Primary, accent and spectral are three evaluations of one cosine palette at t0, t0+accent and
// t0+spectral, so every harmony is a short list of offsets rather than three unrelated colours.
// The ring colour the Director chose supplies the primary hue; the section supplies the harmony.

enum class Harmony:std::uint8_t{Analogous,Complementary,Triadic,Split};
using Rgb=std::array<float,3>;
struct KeyColors{std::array<Rgb,3> c{};};     // primary, accent, spectral

struct CosinePalette{
 Rgb a{.5f,.5f,.5f},b{.5f,.5f,.5f},c{1,1,1},d{0,-.3333f,-.6667f};
 Rgb eval(float t)const{Rgb o;for(int i=0;i<3;++i)o[size_t(i)]=std::clamp(a[size_t(i)]+b[size_t(i)]*std::cos(6.2831853f*(c[size_t(i)]*t+d[size_t(i)])),0.f,1.f);return o;}
};
// Same luminance balance as the shader's ring hues, so procedural and ring colours sit together.
inline Rgb Balance(Rgb c){const float luma=c[0]*.2126f+c[1]*.7152f+c[2]*.0722f,scale=1+(.60f/std::max(luma,.08f)-1)*.65f;for(auto&v:c)v=std::min(1.2f,(v*.98f+.02f)*scale);return c;}
inline float Fract(float x){return x-std::floor(x);}
inline float SmoothStep01(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
inline std::uint32_t PaletteHash(std::uint32_t x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;x^=x>>16;return x;}
inline float PaletteUnit(std::uint32_t seed,std::uint32_t salt){return float(PaletteHash(seed*2654435761U+salt)&0xFFFFFF)/16777215.f;}
// Section intent: 0 open, 1 build, 2 drop, 3 breakdown, 4 drive.
inline Harmony HarmonyForSection(int section){
 switch(section){case 1:return Harmony::Split;case 2:return Harmony::Complementary;case 4:return Harmony::Triadic;default:return Harmony::Analogous;}
}
struct HarmonyOffsets{float accent,spectral;};
inline HarmonyOffsets OffsetsFor(Harmony h,float contrast){
 const float k=.75f+.4f*std::clamp(contrast,0.f,1.f);
 switch(h){
  case Harmony::Complementary:return {.5f,.25f*k};
  case Harmony::Triadic:return {.3333f,.6667f};
  case Harmony::Split:return {.42f,.58f};
  default:return {.07f*k,.14f*k};
 }
}
// Quiet music gets a darker, calmer palette; loud music gets the full swing.
inline CosinePalette MakePalette(float energy,std::uint32_t seed){
 energy=std::clamp(energy,0.f,1.f);CosinePalette p;
 const float a=.5f-(1-energy)*.10f,b=.34f+.14f*energy;
 for(int i=0;i<3;++i){
  p.a[size_t(i)]=a;p.b[size_t(i)]=b;
  p.c[size_t(i)]=1.f+(PaletteUnit(seed,10u+unsigned(i))-.5f)*.12f;
  p.d[size_t(i)]=-float(i)/3.f+(PaletteUnit(seed,20u+unsigned(i))-.5f)*.08f;
 }
 return p;
}
struct PaletteSpec{float hueDegrees{0};int section{0};float energy{.4f};float contrast{.5f};std::uint32_t seed{1};};
// What the shader needs, already packed as float4s: coefficients in xyz, extras in w.
struct GpuPalette{
 std::array<float,4> a{},b{},c{},d{};            // a.w mix weight, b.w t0, c.w accent offset, d.w spectral offset
 std::array<std::array<float,4>,3> previous{};   // evaluated colours being left; previous[0].w = arrival weight (1 = none)
};

class ProceduralPalette{
 CosinePalette pal_{};Harmony harmony_{Harmony::Analogous};bool started_{false};
 float hueFrom_{0},hueTo_{0},huePos_{0},hueProgress_{1};
 float offsetAccent_{.07f},offsetSpectral_{.14f},contrast_{.5f};
 KeyColors prev_{};float bridge_{1},glide_{3.f};
 float energy_{.4f},weight_{0};bool enabled_{false};
 float shift_{0},spread_{.5f};
 static float shortest(float from,float to){float d=Fract(to-from);return d>.5f?d-1:d;}
 KeyColors evaluate()const{
  const float spreadScale=.5f+spread_;const float t0=Fract(huePos_+shift_);
  KeyColors k;k.c[0]=Balance(pal_.eval(t0));k.c[1]=Balance(pal_.eval(t0+offsetAccent_*spreadScale));k.c[2]=Balance(pal_.eval(t0+offsetSpectral_*spreadScale));return k;
 }
public:
 void setEnabled(bool on){enabled_=on;}bool enabled()const{return enabled_;}
 void setModulation(float shift,float spread){shift_=std::clamp(shift,0.f,1.f);spread_=std::clamp(spread,0.f,1.f);}
 // Called whenever the Director's colour, section or energy class changes. A change of harmony
 // bridges between evaluated colours; a change of hue within a harmony slides the phase the short
 // way round the wheel, which keeps every in-between colour saturated.
 void retarget(const PaletteSpec&spec,float glideSeconds){
  const float hue=Fract(spec.hueDegrees/360.f);const Harmony h=HarmonyForSection(spec.section);
  glide_=std::clamp(glideSeconds,.3f,10.f);contrast_=spec.contrast;
  if(!started_){started_=true;harmony_=h;pal_=MakePalette(spec.energy,spec.seed);const auto o=OffsetsFor(h,contrast_);offsetAccent_=o.accent;offsetSpectral_=o.spectral;huePos_=hueFrom_=hueTo_=hue;hueProgress_=1;bridge_=1;energy_=spec.energy;return;}
  if(h==harmony_){hueFrom_=huePos_;hueTo_=hue;hueProgress_=0;return;}
  prev_=currentKeys();bridge_=0;
  harmony_=h;pal_=MakePalette(energy_,spec.seed);const auto o=OffsetsFor(h,contrast_);offsetAccent_=o.accent;offsetSpectral_=o.spectral;
  hueFrom_=huePos_=hueTo_=hue;hueProgress_=1;
 }
 void setEnergy(float e){energy_=std::clamp(e,0.f,1.f);}
 void update(float dt){
  dt=std::clamp(dt,0.f,.25f);
  if(hueProgress_<1){hueProgress_=std::min(1.f,hueProgress_+dt/glide_);huePos_=Fract(hueFrom_+shortest(hueFrom_,hueTo_)*SmoothStep01(hueProgress_));}
  if(bridge_<1)bridge_=std::min(1.f,bridge_+dt/glide_);
  // Energy eases into the amplitude of the live palette without a retarget.
  const float target=std::clamp(energy_,0.f,1.f),a=.5f-(1-target)*.10f,b=.34f+.14f*target;
  for(int i=0;i<3;++i){pal_.a[size_t(i)]+=(a-pal_.a[size_t(i)])*std::min(1.f,dt*.8f);pal_.b[size_t(i)]+=(b-pal_.b[size_t(i)])*std::min(1.f,dt*.8f);}
  weight_+=std::clamp((enabled_?1.f:0.f)-weight_,-dt/1.5f,dt/1.5f);
 }
 // The colours the viewer sees right now, including any bridge in progress.
 KeyColors currentKeys()const{
  KeyColors k=evaluate();const float w=SmoothStep01(bridge_);
  if(w<1)for(int i=0;i<3;++i)for(int j=0;j<3;++j)k.c[size_t(i)][size_t(j)]=prev_.c[size_t(i)][size_t(j)]+(k.c[size_t(i)][size_t(j)]-prev_.c[size_t(i)][size_t(j)])*w;
  return k;
 }
 float weight()const{return weight_;}
 Harmony harmony()const{return harmony_;}
 GpuPalette gpu()const{
  GpuPalette g;const float spreadScale=.5f+spread_;
  for(int i=0;i<3;++i){g.a[size_t(i)]=pal_.a[size_t(i)];g.b[size_t(i)]=pal_.b[size_t(i)];g.c[size_t(i)]=pal_.c[size_t(i)];g.d[size_t(i)]=pal_.d[size_t(i)];}
  g.a[3]=weight_;g.b[3]=Fract(huePos_+shift_);g.c[3]=offsetAccent_*spreadScale;g.d[3]=offsetSpectral_*spreadScale;
  for(int i=0;i<3;++i){for(int j=0;j<3;++j)g.previous[size_t(i)][size_t(j)]=prev_.c[size_t(i)][size_t(j)];g.previous[size_t(i)][3]=0;}
  g.previous[0][3]=SmoothStep01(bridge_);return g;
 }
};
}

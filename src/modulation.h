#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace rarestage::show {
// Universal modulation matrix. Audio and time sources are routed through a typed patch bay
// into a fixed pool of 32 semantic parameters (float4 g_Param[8] on the GPU). Everything here is
// fixed-size and allocation free, so it can run every frame on the render thread; the audio thread
// only ever supplies a finished snapshot.

enum class ModSource:std::uint8_t{Sub,KickBody,BassTone,LowMid,Mid,Presence,Air,KickOnset,SnareOnset,BeatOnset,Flux,Rise,Energy,BeatPhase,LfoSlow,LfoMedium,LfoFast,Constant,Count};
inline constexpr int ModSourceCount=int(ModSource::Count);
enum class ModShape:std::uint8_t{Linear,Squared,Sqrt,Smooth};
enum class ModMode:std::uint8_t{Add,Multiply,Maximum,Replace};

// Schema: shader-space range and neutral value of each parameter. Routes work on the normalised
// 0..1 value; the schema converts it. The default is what an unrouted or disabled matrix produces.
struct ModDestination{const char*name;float lo,hi,def;};
inline constexpr int ModParamCount=32;
inline constexpr std::array<ModDestination,ModParamCount> ModDestinations{{
 {"Pulse",.8f,1.4f,1.f},{"Displacement",0.f,1.f,0.f},{"Emission",.5f,2.f,1.f},{"Warp",0.f,1.f,0.f},
 {"Density",.5f,2.f,1.f},{"CameraDrift",0.f,2.f,1.f},{"CameraDepth",-1.f,1.f,0.f},{"Orbit",0.f,2.f,1.f},
 {"Rotation",-1.f,1.f,0.f},{"Atmosphere",0.f,1.f,.3f},{"Contrast",.8f,1.4f,1.f},{"Saturation",.6f,1.5f,1.f},
 {"Exposure",.85f,1.25f,1.f},{"Bloom",.5f,1.8f,1.f},{"ParticleSpeed",0.f,2.f,1.f},{"Sparkle",0.f,1.f,0.f},
 {"PaletteShift",0.f,1.f,0.f},{"PaletteSpread",0.f,1.f,.5f},{"TempoScale",.5f,2.f,1.f},{"Symmetry",0.f,1.f,.5f},
 {"ClockTravel",0.f,1.f,0.f},{"ClockPulse",0.f,1.f,0.f},{"ClockDrift",0.f,1.f,0.f},{"ClockWarp",0.f,1.f,0.f},
 {"ClockEnergy",0.f,1.f,0.f},{"ClockBuild",0.f,1.f,0.f},{"ClockDrop",0.f,1.f,0.f},{"Aux7",0.f,1.f,0.f},
 {"Aux8",0.f,1.f,0.f},{"Aux9",0.f,1.f,0.f},{"Aux10",0.f,1.f,0.f},{"Aux11",0.f,1.f,0.f}}};
// Parameters 20-26 are reserved for the engine's integrated motion clocks (GenerativeClock). The renderer writes them
// straight into g_Param after the pool is copied, so they carry unbounded values and are never routed.
enum ModClock:int{PClockFirst=20,PClockLast=26};
// Named indices of the parameters the renderer consumes today.
enum ModParam:int{PContrast=10,PSaturation=11,PExposure=12,PBloom=13,PPaletteShift=16,PPaletteSpread=17};

inline constexpr int MaxModRoutes=48;
inline constexpr std::uint8_t AllSections=0x1F;
struct ModRoute{
 ModSource source{ModSource::Constant};std::uint8_t dest{0};
 float gain{1},offset{0};ModShape shape{ModShape::Linear};
 float attack{.02f},release{.25f};       // seconds; zero is instantaneous
 float lo{-1},hi{1};                     // clamp on this route's contribution
 ModMode mode{ModMode::Add};std::uint8_t sectionMask{AllSections};bool enabled{true};
};
struct ModBank{std::array<ModRoute,MaxModRoutes> routes{};int count{0};
 bool add(const ModRoute&route){if(count>=MaxModRoutes||route.dest>=ModParamCount)return false;routes[size_t(count++)]=route;return true;}
};
// One coherent view of the music for a single frame. Built once by the caller; the matrix never reads
// live analysis state.
struct ModSnapshot{std::array<float,ModSourceCount> value{};int section{0};};

class ModulationMatrix{
 std::array<ModBank,2> bank_{};std::array<std::array<float,MaxModRoutes>,2> envelope_{};
 int target_{0};float mix_{0},fadeSeconds_{2.f},depth_{0};bool enabled_{false};
 std::array<float,3> lfoPhase_{};std::array<float,ModParamCount> output_{};
 static float shape(ModShape s,float x){x=std::clamp(x,0.f,1.f);switch(s){case ModShape::Squared:return x*x;case ModShape::Sqrt:return std::sqrt(x);case ModShape::Smooth:return x*x*(3-2*x);default:return x;}}
 static float norm(int param){const auto&d=ModDestinations[size_t(param)];return d.hi>d.lo?(d.def-d.lo)/(d.hi-d.lo):0.f;}
 void evaluateBank(int index,const ModSnapshot&snap,float dt,std::array<float,ModParamCount>&out){
  for(int i=0;i<ModParamCount;++i)out[size_t(i)]=norm(i);
  const auto&bank=bank_[size_t(index)];auto&env=envelope_[size_t(index)];
  for(int i=0;i<bank.count;++i){
   const ModRoute&r=bank.routes[size_t(i)];float&e=env[size_t(i)];
   const bool live=r.enabled&&(r.sectionMask>>std::clamp(snap.section,0,4)&1);
   const float x=live?shape(r.shape,snap.value[size_t(r.source)])*r.gain+r.offset:0.f;
   const float tau=x>e?r.attack:r.release;e=tau<=1e-4f?x:e+(x-e)*(1-std::exp(-dt/tau));
   const float c=std::clamp(e,r.lo,r.hi);float&v=out[r.dest];
   switch(r.mode){case ModMode::Add:v+=c;break;case ModMode::Multiply:v*=c;break;case ModMode::Maximum:v=std::max(v,c);break;case ModMode::Replace:v=c;break;}
  }
  for(auto&v:out)v=std::clamp(v,0.f,1.f);
 }
public:
 static constexpr int LfoCount=3;
 ModulationMatrix(){for(int i=0;i<ModParamCount;++i)output_[size_t(i)]=ModDestinations[size_t(i)].def;}
 void setEnabled(bool on){enabled_=on;}bool enabled()const{return enabled_;}
 // Stages a patch in the bank that is not currently selected. Envelopes of routes that exist in
 // both patches are carried over, so changing the patch never snaps a running envelope to zero.
 void stage(const ModBank&bank){
  const int idle=1-target_;bank_[size_t(idle)]=bank;
  for(int i=0;i<bank.count;++i){float carried=0;
   for(int j=0;j<bank_[size_t(target_)].count;++j){const auto&a=bank_[size_t(target_)].routes[size_t(j)],&b=bank.routes[size_t(i)];if(a.source==b.source&&a.dest==b.dest){carried=envelope_[size_t(target_)][size_t(j)];break;}}
   envelope_[size_t(idle)][size_t(i)]=carried;}
 }
 // Starts a crossfade to the staged patch. Re-committing while a fade is running is allowed; the
 // blend simply reverses from where it is.
 void commit(float seconds){target_=1-target_;fadeSeconds_=std::clamp(seconds,.05f,30.f);}
 // Loads a patch into whichever bank is selected, with no fade. For startup and tests.
 void load(const ModBank&bank){bank_[size_t(target_)]=bank;envelope_[size_t(target_)].fill(0);mix_=float(target_);}
 int selectedBank()const{return target_;}float crossfade()const{return mix_;}
 const std::array<float,ModParamCount>& output()const{return output_;}
 float depth()const{return depth_;}
 // dt in seconds. Writes shader-space values to output().
 void update(const ModSnapshot&in,float dt){
  dt=std::clamp(dt,0.f,.25f);
  static constexpr float lfoHz[LfoCount]{.05f,.25f,1.f};
  ModSnapshot snap=in;
  for(int i=0;i<LfoCount;++i){lfoPhase_[size_t(i)]=std::fmod(lfoPhase_[size_t(i)]+dt*lfoHz[i],1.f);snap.value[size_t(int(ModSource::LfoSlow)+i)]=.5f+.5f*std::sin(6.2831853f*lfoPhase_[size_t(i)]);}
  snap.value[size_t(ModSource::Constant)]=1.f;
  for(auto&v:snap.value)if(!std::isfinite(v))v=0;
  std::array<float,ModParamCount> a{},b{};evaluateBank(0,snap,dt,a);evaluateBank(1,snap,dt,b);
  const float goal=float(target_);const float step=dt/fadeSeconds_;mix_+=std::clamp(goal-mix_,-step,step);
  const float w=mix_*mix_*(3-2*mix_);
  depth_+=std::clamp((enabled_?1.f:0.f)-depth_,-dt*2.f,dt*2.f);
  for(int i=0;i<ModParamCount;++i){
   const auto&d=ModDestinations[size_t(i)];const float n=a[size_t(i)]+(b[size_t(i)]-a[size_t(i)])*w;
   const float shaderValue=d.lo+(d.hi-d.lo)*n;output_[size_t(i)]=d.def+(shaderValue-d.def)*depth_;
  }
 }
};

// Neutral pool laid out as the GPU sees it, for any Constants that never pass through the matrix.
inline std::array<std::array<float,4>,8> NeutralParams(){std::array<std::array<float,4>,8> p{};for(int i=0;i<ModParamCount;++i)p[size_t(i>>2)][size_t(i&3)]=ModDestinations[size_t(i)].def;return p;}

// Source-agnostic helpers for building routes.
inline ModRoute Route(ModSource s,int dest,float gain,float attack,float release,ModMode mode=ModMode::Add,std::uint8_t mask=AllSections,ModShape shape=ModShape::Linear){
 ModRoute r;r.source=s;r.dest=std::uint8_t(dest);r.gain=gain;r.attack=attack;r.release=release;r.mode=mode;r.sectionMask=mask;r.shape=shape;return r;
}
// Sections: 0 open, 1 build, 2 drop, 3 breakdown, 4 drive.
inline constexpr std::uint8_t SectionBit(int s){return std::uint8_t(1u<<s);}
// Default patch: every lane has its own job, slow sources carry the arc of the track and fast
// sources only add accents, so the response stays cohesive between hits.
inline ModBank CohesivePatch(){
 ModBank b;using S=ModSource;
 b.add(Route(S::KickOnset,0,.55f,.012f,.20f));                       // pulse: kick punch
 b.add(Route(S::Sub,1,.45f,.04f,.35f));b.add(Route(S::BassTone,1,.35f,.05f,.30f)); // displacement: low end
 b.add(Route(S::Presence,2,.40f,.03f,.28f));b.add(Route(S::Energy,2,.20f,.30f,.90f)); // emission
 b.add(Route(S::Mid,3,.35f,.12f,.60f));b.add(Route(S::LfoSlow,3,.15f,.5f,.5f));       // warp
 b.add(Route(S::Energy,4,.35f,.5f,1.4f));                              // density follows the section arc
 b.add(Route(S::Energy,5,.40f,.6f,1.6f));b.add(Route(S::LfoSlow,5,.20f,.5f,.5f));   // camera drift keeps moving
 b.add(Route(S::Rise,6,.60f,.8f,1.2f,ModMode::Add,SectionBit(1)|SectionBit(2)));    // camera depth on builds/drops
 b.add(Route(S::LfoMedium,7,.35f,.5f,.5f));b.add(Route(S::LfoSlow,8,.50f,.5f,.5f));
 b.add(Route(S::Air,9,.50f,.10f,.80f));                                // atmosphere: air band
 b.add(Route(S::Rise,10,.35f,.5f,1.0f,ModMode::Add,SectionBit(1)|SectionBit(2)));  // contrast tightens into drops
 b.add(Route(S::Presence,11,.18f,.10f,.60f));b.add(Route(S::Energy,11,.12f,.40f,1.2f));
 b.add(Route(S::KickBody,12,.22f,.02f,.30f));                          // exposure: gentle kick breath
 b.add(Route(S::KickOnset,13,.30f,.012f,.24f));b.add(Route(S::Energy,13,.22f,.30f,.90f)); // bloom
 b.add(Route(S::Air,15,.55f,.02f,.18f));b.add(Route(S::Flux,15,.30f,.02f,.15f));     // sparkle
 b.add(Route(S::LfoSlow,16,.06f,.5f,.5f));                             // slow palette drift
 return b;
}
// Breakdown patch: keeps the slow motion, drops the accents.
inline ModBank AmbientPatch(){
 ModBank b;using S=ModSource;
 b.add(Route(S::Sub,1,.20f,.10f,.80f));
 b.add(Route(S::Energy,2,.15f,.60f,1.6f));
 b.add(Route(S::LfoSlow,3,.30f,.5f,.5f));
 b.add(Route(S::LfoSlow,5,.40f,.5f,.5f));b.add(Route(S::LfoMedium,7,.30f,.5f,.5f));b.add(Route(S::LfoSlow,8,.40f,.5f,.5f));
 b.add(Route(S::Air,9,.65f,.30f,1.20f));
 b.add(Route(S::Energy,13,.12f,.60f,1.4f));
 b.add(Route(S::LfoSlow,16,.10f,.5f,.5f));
 return b;
}
}

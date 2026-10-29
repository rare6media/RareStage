#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
// Reproducible DNA of a generated world. A WorldSpec is plain data: the same spec always produces
// the same HLSL (world_grammar.h), and Generate(seed, family, quality) always produces the same spec.
// Nothing in here touches the GPU, so all of it is covered by offline tests.
namespace rarestage::gen {

inline constexpr std::uint32_t SchemaVersion=1;
inline constexpr std::uint32_t GrammarVersion=1;

// The first eight are the structural (ray-marched) families, kept for use as accents. The last four are the
// scene families the Director builds worlds from: screen-space fields choreographed directly to the music.
enum class Family:std::uint8_t{PanelCascade,Monolith,Cathedral,Conduit,FractalChamber,OrbitalStructures,AbstractFields,NestedArchitecture,CosmicSpace,CircuitBoard,VirusSpread,ErrorHorizon,Count};
inline constexpr int FamilyCount=int(Family::Count);
inline constexpr const char* FamilyNames[FamilyCount]{"PANEL CASCADE","MONOLITH","CATHEDRAL","CONDUIT","FRACTAL CHAMBER","ORBITAL STRUCTURES","ABSTRACT FIELDS","NESTED ARCHITECTURE","COSMIC SPACE","CIRCUIT BOARD","VIRUS SPREAD","ERROR HORIZON"};
inline constexpr bool IsField(Family f){return int(f)>=int(Family::CosmicSpace);}
inline constexpr int FirstFieldFamily=int(Family::CosmicSpace);

// Domain transforms applied to the sample point before the primitives are evaluated.
enum class DomainKind:std::uint8_t{None,RepeatCartesian,RepeatPolar,Kaleidoscope,Fold,Twist,Displace,Count};
struct DomainOp{DomainKind kind{DomainKind::None};float a{0},b{0},c{0};std::uint8_t axisMask{0};};

enum class PrimKind:std::uint8_t{Box,Slab,Prism,Cylinder,Monolith,Torus,Sphere,Gyroid,Conduit,Arch,Count};
enum class CombineKind:std::uint8_t{Union,Intersect,Subtract,SmoothUnion,Count};
struct Primitive{
 PrimKind kind{PrimKind::Box};CombineKind combine{CombineKind::Union};
 float sx{1},sy{1},sz{1};          // half extents / radii, meaning depends on kind
 float ox{0},oy{0},oz{0};          // placement offset
 float shell{0};                   // >0 hollows the primitive to a shell of this thickness
 float smooth{.1f};                // blend width for SmoothUnion
};

struct Material{float emission{1},fresnel{3},specular{24},edgeGlow{.4f},colorSpread{1};};
struct Lighting{float azimuth{.6f},elevation{.7f},rim{.6f},fog{.04f},ambient{.18f};bool shadows{false};};
enum class CameraMode:std::uint8_t{Orbit,Dolly,Drift,LowSweep,Count};
struct CameraSpec{CameraMode mode{CameraMode::Orbit};float distance{6},fov{1.6f},speed{.25f},height{1.2f};};
struct MotionSpec{float rotation{.1f},breathe{.05f},scroll{0},sway{.1f};};
// How strongly this world follows each semantic parameter of the modulation pool (0 ignores it).
struct ModulationProfile{std::uint8_t id{0};float pulse{.6f},displacement{.5f},warp{.5f},density{.4f},emission{.8f},atmosphere{.6f};};
struct Budget{std::uint8_t marchSteps{48};std::uint8_t quality{1};};

// Scene families are described by eight normalised identity values (their meaning is per family and fixed in
// world_grammar.h), a small structural accent, and how often the pattern changes. All values are 0..1.
enum class AccentKind:std::uint8_t{None,Monolith,Ring,Chip,Count};
struct FieldParams{
 std::array<float,8> v{.5f,.5f,.5f,.5f,.5f,.5f,.5f,.5f};
 AccentKind accent{AccentKind::None};float accentSize{.1f};
 std::uint8_t phraseBeats{16};   // beats between pattern changes: 8, 16 or 32
 std::uint8_t octaves{3};        // noise detail, 2..5
};

inline constexpr int MaxDomainOps=4,MaxPrimitives=4;
struct WorldSpec{
 std::uint32_t schema{SchemaVersion},grammar{GrammarVersion};
 std::uint64_t id{0};std::uint32_t seed{0};Family family{Family::PanelCascade};
 std::array<DomainOp,MaxDomainOps> domain{};std::uint8_t domainCount{0};
 std::array<Primitive,MaxPrimitives> prim{};std::uint8_t primCount{0};
 Material material{};Lighting lighting{};CameraSpec camera{};MotionSpec motion{};ModulationProfile modulation{};
 float paletteOffset{0},paletteSpread{1};
 Budget budget{};
 FieldParams field{};
 std::uint64_t shaderHash{0};      // filled by the grammar once the source exists; zero means not yet emitted
};

// ---- deterministic randomness ---------------------------------------------------------------
struct Rng{
 std::uint64_t s;explicit Rng(std::uint64_t seed):s(seed*0x9E3779B97F4A7C15ull+0x1234567ull){}
 std::uint64_t next(){s+=0x9E3779B97F4A7C15ull;std::uint64_t z=s;z=(z^(z>>30))*0xBF58476D1CE4E5B9ull;z=(z^(z>>27))*0x94D049BB133111EBull;return z^(z>>31);}
 float unit(){return float(next()>>40)/16777216.f;}
 float range(float lo,float hi){return q(lo+(hi-lo)*unit());}
 int pick(int n){return int(next()%std::uint64_t(n));}
 bool chance(float p){return unit()<p;}
 // Quantised to 1/1000 so printed shader constants are exact and stable.
 static float q(float x){return std::round(x*1000.f)/1000.f;}
};

// ---- quality / budget -----------------------------------------------------------------------
// Quality 0 = cheapest, 2 = richest. Steps are a static bound on the ray march.
inline Budget BudgetForQuality(int quality){quality=std::clamp(quality,0,2);Budget b;b.quality=std::uint8_t(quality);b.marchSteps=std::uint8_t(quality==0?28:quality==1?44:64);return b;}

// Static cost estimate in arbitrary "map evaluations": steps * (map cost) + normals + shadow march.
inline float PrimitiveCost(PrimKind k){switch(k){case PrimKind::Gyroid:return 3.f;case PrimKind::Conduit:case PrimKind::Arch:return 2.f;case PrimKind::Torus:case PrimKind::Prism:case PrimKind::Cylinder:return 1.4f;default:return 1.f;}}
inline float DomainCost(DomainKind k){switch(k){case DomainKind::Kaleidoscope:return 2.5f;case DomainKind::Fold:return 2.f;case DomainKind::Displace:return 2.f;case DomainKind::Twist:return .8f;case DomainKind::RepeatPolar:return 1.f;case DomainKind::RepeatCartesian:return .6f;default:return 0.f;}}
inline float FieldCost(const WorldSpec&w){
 float base=0;switch(w.family){case Family::CosmicSpace:base=150.f;break;case Family::CircuitBoard:base=170.f;break;case Family::VirusSpread:base=210.f;break;default:base=190.f;break;}
 return base*(.7f+.15f*float(w.field.octaves))*(w.field.accent==AccentKind::None?1.f:1.1f);
}
inline float ComplexityScore(const WorldSpec&w){
 if(IsField(w.family))return FieldCost(w);
 float map=1.f;for(int i=0;i<w.domainCount;++i)map+=DomainCost(w.domain[size_t(i)].kind);
 for(int i=0;i<w.primCount;++i){map+=PrimitiveCost(w.prim[size_t(i)].kind);if(w.prim[size_t(i)].shell>0)map+=.2f;}
 float evals=float(w.budget.marchSteps)+4.f+(w.lighting.shadows?16.f:0.f);
 return map*evals;
}
// Soft limit warns the Director, hard limit rejects before any compilation is attempted.
inline constexpr float SoftComplexity=420.f,HardComplexity=900.f;

// ---- validation -----------------------------------------------------------------------------
enum class Reject:std::uint8_t{None,Schema,Grammar,Count,NonFinite,Range,Incompatible,Complexity};
inline const char* RejectName(Reject r){switch(r){case Reject::None:return "ok";case Reject::Schema:return "schema version";case Reject::Grammar:return "grammar version";case Reject::Count:return "operator count";case Reject::NonFinite:return "non-finite value";case Reject::Range:return "value out of range";case Reject::Incompatible:return "incompatible modules";default:return "over complexity budget";}}

inline bool Finite(float v){return std::isfinite(v);}
inline bool InRange(float v,float lo,float hi){return Finite(v)&&v>=lo&&v<=hi;}
// Legal module combinations. Heavy operators are kept to the families that are designed around them.
inline bool DomainAllowed(Family f,DomainKind k){
 switch(k){
  case DomainKind::Kaleidoscope:case DomainKind::Fold:return f==Family::FractalChamber||f==Family::NestedArchitecture||f==Family::OrbitalStructures;
  case DomainKind::Displace:return f==Family::AbstractFields||f==Family::Conduit||f==Family::Monolith;
  default:return true;
 }
}
inline bool PrimitiveAllowed(Family f,PrimKind k){
 switch(k){
  case PrimKind::Gyroid:return f==Family::AbstractFields||f==Family::FractalChamber;
  case PrimKind::Conduit:return f==Family::Conduit||f==Family::Cathedral;
  case PrimKind::Arch:return f==Family::Cathedral||f==Family::NestedArchitecture;
  case PrimKind::Torus:return f==Family::OrbitalStructures||f==Family::Conduit||f==Family::NestedArchitecture;
  default:return true;
 }
}
inline Reject Validate(const WorldSpec&w){
 if(w.schema!=SchemaVersion)return Reject::Schema;if(w.grammar!=GrammarVersion)return Reject::Grammar;
 if(int(w.family)>=FamilyCount)return Reject::Count;
 if(IsField(w.family)){
  const auto&f=w.field;for(float x:f.v)if(!Finite(x))return Reject::NonFinite;if(!Finite(f.accentSize))return Reject::NonFinite;
  for(float x:f.v)if(x<0.f||x>1.f)return Reject::Range;
  if(int(f.accent)>=int(AccentKind::Count)||!InRange(f.accentSize,0.f,.25f)||(f.phraseBeats!=8&&f.phraseBeats!=16&&f.phraseBeats!=32)||f.octaves<2||f.octaves>5)return Reject::Range;
  if(w.domainCount!=0||w.primCount!=0)return Reject::Incompatible;
  if(ComplexityScore(w)>HardComplexity)return Reject::Complexity;
  return Reject::None;
 }
 if(w.domainCount>MaxDomainOps||w.primCount==0||w.primCount>MaxPrimitives)return Reject::Count;
 auto finite=[&](std::initializer_list<float> v){for(float x:v)if(!Finite(x))return false;return true;};
 for(int i=0;i<w.domainCount;++i){const auto&d=w.domain[size_t(i)];if(!finite({d.a,d.b,d.c}))return Reject::NonFinite;if(int(d.kind)>=int(DomainKind::Count))return Reject::Range;
  if(!DomainAllowed(w.family,d.kind))return Reject::Incompatible;
  switch(d.kind){
   case DomainKind::RepeatCartesian:if(!InRange(d.a,.6f,8.f)||!InRange(d.b,.6f,8.f)||!InRange(d.c,.6f,8.f))return Reject::Range;break;
   case DomainKind::RepeatPolar:if(!InRange(d.a,2.f,16.f))return Reject::Range;break;
   case DomainKind::Kaleidoscope:case DomainKind::Fold:if(!InRange(d.a,1.f,4.f)||!InRange(d.b,.2f,2.5f))return Reject::Range;break;
   case DomainKind::Twist:if(!InRange(d.a,-.6f,.6f))return Reject::Range;break;
   case DomainKind::Displace:if(!InRange(d.a,.3f,3.f)||!InRange(d.b,0.f,.4f))return Reject::Range;break;
   default:break;}}
 bool positive=false;
 for(int i=0;i<w.primCount;++i){const auto&p=w.prim[size_t(i)];if(!finite({p.sx,p.sy,p.sz,p.ox,p.oy,p.oz,p.shell,p.smooth}))return Reject::NonFinite;
  if(int(p.kind)>=int(PrimKind::Count)||int(p.combine)>=int(CombineKind::Count))return Reject::Range;
  if(!PrimitiveAllowed(w.family,p.kind))return Reject::Incompatible;
  if(!InRange(p.sx,.02f,8.f)||!InRange(p.sy,.02f,8.f)||!InRange(p.sz,.02f,8.f)||!InRange(p.ox,-8.f,8.f)||!InRange(p.oy,-8.f,8.f)||!InRange(p.oz,-8.f,8.f)||!InRange(p.shell,0.f,.6f)||!InRange(p.smooth,.01f,1.f))return Reject::Range;
  // A subtraction or intersection needs something already present to act on.
  if((p.combine==CombineKind::Subtract||p.combine==CombineKind::Intersect)&&!positive)return Reject::Incompatible;
  if(p.combine==CombineKind::Union||p.combine==CombineKind::SmoothUnion)positive=true;}
 if(!positive)return Reject::Incompatible;
 const auto&m=w.material;const auto&l=w.lighting;const auto&c=w.camera;const auto&mo=w.motion;const auto&mp=w.modulation;
 if(!finite({m.emission,m.fresnel,m.specular,m.edgeGlow,m.colorSpread,l.azimuth,l.elevation,l.rim,l.fog,l.ambient,c.distance,c.fov,c.speed,c.height,mo.rotation,mo.breathe,mo.scroll,mo.sway,mp.pulse,mp.displacement,mp.warp,mp.density,mp.emission,mp.atmosphere,w.paletteOffset,w.paletteSpread}))return Reject::NonFinite;
 if(!InRange(m.emission,.2f,3.f)||!InRange(m.fresnel,.5f,8.f)||!InRange(m.specular,2.f,80.f)||!InRange(m.edgeGlow,0.f,1.5f)||!InRange(m.colorSpread,.2f,4.f))return Reject::Range;
 if(!InRange(l.rim,0.f,1.5f)||!InRange(l.fog,0.f,.3f)||!InRange(l.ambient,0.f,.6f)||!InRange(c.distance,1.f,16.f)||!InRange(c.fov,.8f,2.6f)||!InRange(c.speed,0.f,1.5f)||!InRange(c.height,-3.f,6.f))return Reject::Range;
 if(!InRange(mo.rotation,-1.f,1.f)||!InRange(mo.breathe,0.f,.3f)||!InRange(mo.scroll,-2.f,2.f)||!InRange(mo.sway,0.f,.6f))return Reject::Range;
 if(!InRange(mp.pulse,0.f,1.f)||!InRange(mp.displacement,0.f,1.f)||!InRange(mp.warp,0.f,1.f)||!InRange(mp.density,0.f,1.f)||!InRange(mp.emission,0.f,1.f)||!InRange(mp.atmosphere,0.f,1.f))return Reject::Range;
 if(w.budget.marchSteps<16||w.budget.marchSteps>96||w.budget.quality>2)return Reject::Range;
 if(ComplexityScore(w)>HardComplexity)return Reject::Complexity;
 return Reject::None;
}

// ---- generation -----------------------------------------------------------------------------
inline std::uint64_t SpecId(Family f,std::uint32_t seed,int quality){return (std::uint64_t(seed)<<16)|(std::uint64_t(f)<<8)|std::uint64_t(quality&0xFF);}

// Scene families: identity values, accent and cadence. Everything the shader needs beyond this comes from the
// music, so two seeds of one family differ in look and layout but move to the music in the same way.
inline void GenerateField(WorldSpec&w,Rng&r){
 for(float&x:w.field.v)x=r.range(.05f,.95f);
 w.field.phraseBeats=std::uint8_t(r.chance(.5f)?16:(r.chance(.5f)?8:32));
 w.field.octaves=std::uint8_t(std::clamp(2+int(w.budget.quality),2,5));
 w.field.accentSize=r.range(.05f,.16f);
 switch(w.family){
  case Family::CosmicSpace:w.field.accent=r.chance(.35f)?AccentKind::None:(r.chance(.5f)?AccentKind::Ring:AccentKind::Monolith);break;
  case Family::CircuitBoard:w.field.accent=r.chance(.2f)?AccentKind::None:AccentKind::Chip;break;
  case Family::VirusSpread:w.field.accent=r.chance(.6f)?AccentKind::None:AccentKind::Ring;break;
  default:w.field.accent=r.chance(.4f)?AccentKind::None:AccentKind::Monolith;break;}
 w.camera.speed=r.range(.12f,.4f);w.motion.sway=r.range(.05f,.3f);
 w.modulation.id=std::uint8_t(r.pick(8));w.modulation.pulse=r.range(.4f,.9f);w.modulation.displacement=r.range(.2f,.8f);w.modulation.warp=r.range(.2f,.8f);w.modulation.density=r.range(.2f,.7f);w.modulation.emission=r.range(.5f,1.f);w.modulation.atmosphere=r.range(.3f,.9f);
 w.paletteOffset=r.range(0.f,1.f);w.paletteSpread=r.range(.5f,1.6f);
}

inline WorldSpec Generate(std::uint32_t seed,Family family,int quality){
 WorldSpec w;Rng r(std::uint64_t(seed)*131u+std::uint64_t(family)*7u+1u);
 w.seed=seed;w.family=family;w.budget=BudgetForQuality(quality);w.id=SpecId(family,seed,w.budget.quality);
 if(IsField(family)){GenerateField(w,r);return w;}
 auto dom=[&](DomainKind k,float a,float b,float c,std::uint8_t mask){if(w.domainCount<MaxDomainOps)w.domain[w.domainCount++]=DomainOp{k,a,b,c,mask};};
 auto prim=[&](PrimKind k,CombineKind cb,float sx,float sy,float sz,float ox,float oy,float oz,float shell,float smooth){if(w.primCount<MaxPrimitives){Primitive p;p.kind=k;p.combine=cb;p.sx=sx;p.sy=sy;p.sz=sz;p.ox=ox;p.oy=oy;p.oz=oz;p.shell=shell;p.smooth=smooth;w.prim[w.primCount++]=p;}};
 switch(family){
  case Family::PanelCascade:
   dom(DomainKind::RepeatCartesian,r.range(1.5f,2.6f),r.range(1.1f,2.f),r.range(1.6f,3.2f),7);
   if(r.chance(.5f))dom(DomainKind::Twist,r.range(.05f,.22f)*(r.chance(.5f)?1.f:-1.f),0,0,0);
   prim(PrimKind::Slab,CombineKind::Union,r.range(.55f,.95f),r.range(.32f,.8f),r.range(.03f,.07f),0,0,0,0,.1f);
   prim(PrimKind::Box,CombineKind::Subtract,r.range(.22f,.4f),r.range(.14f,.3f),.2f,0,0,0,0,.1f);
   if(r.chance(.6f))prim(PrimKind::Slab,CombineKind::Union,r.range(.2f,.4f),r.range(.1f,.2f),.05f,r.range(-.2f,.2f),r.range(-.2f,.2f),r.range(.1f,.4f),0,.1f);
   w.camera.mode=r.chance(.6f)?CameraMode::Dolly:CameraMode::Drift;w.motion.scroll=r.range(.2f,1.f);break;
  case Family::Monolith:
   dom(DomainKind::RepeatPolar,float(3+r.pick(7)),0,0,2);
   if(r.chance(.5f))dom(DomainKind::Displace,r.range(.6f,1.6f),r.range(.02f,.1f),0,0);
   prim(PrimKind::Monolith,CombineKind::Union,r.range(.25f,.5f),r.range(1.5f,3.f),r.range(.25f,.5f),r.range(2.f,3.6f),0,0,0,.1f);
   prim(PrimKind::Box,CombineKind::Subtract,r.range(.06f,.14f),r.range(.7f,1.6f),.6f,r.range(2.f,3.6f),r.range(-.4f,.4f),0,0,.1f);
   if(r.chance(.7f))prim(PrimKind::Prism,CombineKind::Union,r.range(.4f,.8f),r.range(.4f,.9f),r.range(.2f,.4f),0,r.range(-.2f,.6f),0,0,.1f);
   w.camera.mode=r.chance(.7f)?CameraMode::Orbit:CameraMode::LowSweep;break;
  case Family::Cathedral:
   dom(DomainKind::RepeatCartesian,r.range(2.8f,4.2f),2.f,r.range(3.2f,5.f),5);
   prim(PrimKind::Box,CombineKind::Union,r.range(.18f,.32f),r.range(1.8f,3.f),r.range(.18f,.32f),r.range(1.1f,1.8f),0,0,0,.1f);
   prim(PrimKind::Arch,CombineKind::SmoothUnion,r.range(1.2f,1.9f),r.range(.5f,1.f),r.range(.12f,.25f),0,r.range(1.4f,2.2f),0,0,r.range(.1f,.3f));
   prim(PrimKind::Slab,CombineKind::Union,6.f,.06f,6.f,0,-1.8f,0,0,.1f);
   w.camera.mode=CameraMode::Dolly;w.camera.height=r.range(-.2f,1.2f);w.motion.scroll=r.range(.2f,.8f);break;
  case Family::Conduit:
   dom(DomainKind::Twist,r.range(.05f,.25f)*(r.chance(.5f)?1.f:-1.f),0,0,0);
   dom(DomainKind::RepeatCartesian,2.f,2.f,r.range(1.6f,3.f),4);
   prim(PrimKind::Conduit,CombineKind::Union,r.range(1.1f,1.8f),1.f,r.range(.8f,1.4f),0,0,0,r.range(.06f,.14f),.1f);
   prim(PrimKind::Torus,CombineKind::SmoothUnion,r.range(1.1f,1.8f),r.range(.06f,.12f),1.f,0,0,0,0,.15f);
   w.camera.mode=r.chance(.7f)?CameraMode::Dolly:CameraMode::Drift;w.camera.height=0;w.camera.distance=2.f;w.motion.scroll=r.range(.4f,1.2f);break;
  case Family::FractalChamber:
   dom(DomainKind::Kaleidoscope,float(3+r.pick(2)),r.range(.5f,1.f),0,0);
   dom(DomainKind::Fold,float(2+r.pick(3)),r.range(.6f,1.f),r.range(.1f,.3f),0);
   prim(PrimKind::Box,CombineKind::Union,r.range(.3f,.6f),r.range(.3f,.6f),r.range(.3f,.6f),r.range(.2f,.8f),0,0,r.range(.08f,.16f),.1f);
   if(r.chance(.5f))prim(PrimKind::Gyroid,CombineKind::Intersect,r.range(1.4f,2.2f),r.range(.07f,.14f),1.f,0,0,0,0,.1f);
   prim(PrimKind::Prism,CombineKind::Union,r.range(.1f,.3f),r.range(.4f,.8f),r.range(.1f,.2f),0,r.range(-.3f,.3f),0,0,.1f);
   w.camera.mode=r.chance(.5f)?CameraMode::Orbit:CameraMode::Drift;w.camera.distance=r.range(4.5f,6.5f);break;
  case Family::OrbitalStructures:
   dom(DomainKind::RepeatPolar,float(5+r.pick(8)),0,0,2);
   if(r.chance(.4f))dom(DomainKind::Fold,2.f,r.range(.8f,1.4f),.15f,0);
   prim(PrimKind::Torus,CombineKind::Union,r.range(1.2f,2.6f),r.range(.06f,.16f),1.f,r.range(0.f,1.f),0,0,0,.1f);
   prim(PrimKind::Sphere,CombineKind::Union,r.range(.18f,.4f),1.f,1.f,r.range(1.6f,3.f),r.range(-.3f,.3f),0,r.chance(.5f)?r.range(.04f,.1f):0.f,.1f);
   prim(PrimKind::Sphere,CombineKind::SmoothUnion,r.range(.5f,.9f),1.f,1.f,0,0,0,r.range(.05f,.1f),.2f);
   w.camera.mode=r.chance(.5f)?CameraMode::Orbit:CameraMode::LowSweep;w.camera.distance=r.range(5.f,8.f);break;
  case Family::AbstractFields:
   dom(DomainKind::Displace,r.range(.6f,1.6f),r.range(.06f,.22f),0,0);
   prim(PrimKind::Gyroid,CombineKind::Union,r.range(1.1f,2.4f),r.range(.07f,.18f),1.f,0,0,0,0,.1f);
   prim(PrimKind::Sphere,CombineKind::Intersect,r.range(2.4f,3.6f),1.f,1.f,0,0,0,0,.1f);
   w.camera.mode=r.chance(.5f)?CameraMode::Drift:CameraMode::Orbit;w.camera.distance=r.range(6.5f,9.f);break;
  case Family::NestedArchitecture:default:
   if(r.chance(.6f))dom(DomainKind::Fold,2.f,r.range(.8f,1.3f),.1f,0);
   prim(PrimKind::Box,CombineKind::Union,r.range(1.4f,2.f),r.range(1.4f,2.f),r.range(1.4f,2.f),0,0,0,r.range(.05f,.1f),.1f);
   prim(PrimKind::Box,CombineKind::Union,r.range(.9f,1.2f),r.range(.9f,1.2f),r.range(.9f,1.2f),0,0,0,r.range(.04f,.08f),.1f);
   prim(PrimKind::Box,CombineKind::Subtract,r.range(.2f,.4f),3.f,r.range(.2f,.4f),0,0,0,0,.1f);
   if(r.chance(.6f))prim(PrimKind::Arch,CombineKind::Union,r.range(.5f,.8f),r.range(.3f,.5f),r.range(.08f,.14f),0,r.range(.4f,.8f),0,0,.1f);
   w.camera.mode=r.chance(.5f)?CameraMode::Orbit:CameraMode::Drift;w.camera.distance=r.range(7.f,10.f);break;
 }
 if(w.camera.mode==CameraMode::Orbit||w.camera.mode==CameraMode::LowSweep){w.camera.distance=std::max(w.camera.distance,3.f);w.camera.height=w.camera.mode==CameraMode::LowSweep?r.range(-.3f,.4f):r.range(.6f,2.f);}
 w.camera.fov=r.range(1.4f,1.9f);w.camera.speed=r.range(.12f,.4f);
 w.material.emission=r.range(.8f,1.6f);w.material.fresnel=r.range(2.f,4.5f);w.material.specular=r.range(10.f,40.f);w.material.edgeGlow=r.range(.15f,.7f);w.material.colorSpread=r.range(.6f,2.2f);
 w.lighting.azimuth=r.range(0.f,6.28f);w.lighting.elevation=r.range(.4f,1.2f);w.lighting.rim=r.range(.3f,.9f);w.lighting.fog=r.range(.02f,.09f);w.lighting.ambient=r.range(.1f,.25f);
 w.lighting.shadows=w.budget.quality>=2&&r.chance(.5f);
 w.motion.rotation=r.range(-.3f,.3f);w.motion.breathe=r.range(.02f,.1f);w.motion.sway=r.range(.05f,.3f);
 w.modulation.id=std::uint8_t(r.pick(8));w.modulation.pulse=r.range(.4f,.9f);w.modulation.displacement=r.range(.2f,.8f);w.modulation.warp=r.range(.2f,.8f);w.modulation.density=r.range(.2f,.7f);w.modulation.emission=r.range(.5f,1.f);w.modulation.atmosphere=r.range(.3f,.9f);
 w.paletteOffset=r.range(0.f,1.f);w.paletteSpread=r.range(.5f,1.6f);
 // Quality tiers are upper bounds: a composition that would exceed the hard limit gives up shadows and
 // march steps first, so every generated spec is renderable inside the budget.
 for(;ComplexityScore(w)>HardComplexity*.92f;){if(w.lighting.shadows){w.lighting.shadows=false;continue;}if(w.budget.marchSteps<=24)break;w.budget.marchSteps=std::uint8_t(w.budget.marchSteps-4);}
 return w;
}

// ---- persistence ----------------------------------------------------------------------------
// Plain key=value text so a saved favourite survives grammar changes and can be read by a human.
inline std::string Serialize(const WorldSpec&w){
 std::string out;char b[160];
 auto line=[&](const char*fmt,auto...a){std::snprintf(b,sizeof b,fmt,a...);out+=b;out+='\n';};
 line("rarestage-worldspec %u",w.schema);line("grammar %u",w.grammar);line("id %llu",(unsigned long long)w.id);line("seed %u",w.seed);line("family %d",int(w.family));
 line("budget %d %d",int(w.budget.marchSteps),int(w.budget.quality));
 line("domainCount %d",int(w.domainCount));for(int i=0;i<w.domainCount;++i){const auto&d=w.domain[size_t(i)];line("domain %d %.9g %.9g %.9g %d",int(d.kind),d.a,d.b,d.c,int(d.axisMask));}
 line("primCount %d",int(w.primCount));for(int i=0;i<w.primCount;++i){const auto&p=w.prim[size_t(i)];line("prim %d %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g",int(p.kind),int(p.combine),p.sx,p.sy,p.sz,p.ox,p.oy,p.oz,p.shell,p.smooth);}
 const auto&m=w.material;line("material %.9g %.9g %.9g %.9g %.9g",m.emission,m.fresnel,m.specular,m.edgeGlow,m.colorSpread);
 const auto&l=w.lighting;line("lighting %.9g %.9g %.9g %.9g %.9g %d",l.azimuth,l.elevation,l.rim,l.fog,l.ambient,int(l.shadows));
 const auto&c=w.camera;line("camera %d %.9g %.9g %.9g %.9g",int(c.mode),c.distance,c.fov,c.speed,c.height);
 const auto&mo=w.motion;line("motion %.9g %.9g %.9g %.9g",mo.rotation,mo.breathe,mo.scroll,mo.sway);
 const auto&mp=w.modulation;line("modulation %d %.9g %.9g %.9g %.9g %.9g %.9g",int(mp.id),mp.pulse,mp.displacement,mp.warp,mp.density,mp.emission,mp.atmosphere);
 line("palette %.9g %.9g",w.paletteOffset,w.paletteSpread);
{const auto&f=w.field;line("field %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d %.9g %d %d",f.v[0],f.v[1],f.v[2],f.v[3],f.v[4],f.v[5],f.v[6],f.v[7],int(f.accent),f.accentSize,int(f.phraseBeats),int(f.octaves));}
 return out;
}
// Returns false on any malformed line; the spec is then unusable and must be discarded.
inline bool Parse(const std::string&text,WorldSpec&w){
 WorldSpec o;int domainSeen=0,primSeen=0;bool header=false;size_t pos=0;
 while(pos<text.size()){
  size_t end=text.find('\n',pos);if(end==std::string::npos)end=text.size();const std::string ln=text.substr(pos,end-pos);pos=end+1;if(ln.empty())continue;
  unsigned u=0;unsigned long long ull=0;int i[4]{};float f[9]{};float g=0;
  if(std::sscanf(ln.c_str(),"rarestage-worldspec %u",&u)==1){o.schema=u;header=true;}
  else if(std::sscanf(ln.c_str(),"grammar %u",&u)==1)o.grammar=u;
  else if(std::sscanf(ln.c_str(),"id %llu",&ull)==1)o.id=ull;
  else if(std::sscanf(ln.c_str(),"seed %u",&u)==1)o.seed=u;
  else if(std::sscanf(ln.c_str(),"family %d",&i[0])==1){if(i[0]<0||i[0]>=FamilyCount)return false;o.family=Family(i[0]);}
  else if(std::sscanf(ln.c_str(),"budget %d %d",&i[0],&i[1])==2){o.budget.marchSteps=std::uint8_t(std::clamp(i[0],0,255));o.budget.quality=std::uint8_t(std::clamp(i[1],0,255));}
  else if(std::sscanf(ln.c_str(),"domainCount %d",&i[0])==1){if(i[0]<0||i[0]>MaxDomainOps)return false;o.domainCount=std::uint8_t(i[0]);}
  else if(std::sscanf(ln.c_str(),"domain %d %f %f %f %d",&i[0],&f[0],&f[1],&f[2],&i[1])==5){if(domainSeen>=MaxDomainOps||i[0]<0||i[0]>=int(DomainKind::Count))return false;o.domain[size_t(domainSeen++)]=DomainOp{DomainKind(i[0]),f[0],f[1],f[2],std::uint8_t(i[1])};}
  else if(std::sscanf(ln.c_str(),"primCount %d",&i[0])==1){if(i[0]<0||i[0]>MaxPrimitives)return false;o.primCount=std::uint8_t(i[0]);}
  else if(std::sscanf(ln.c_str(),"prim %d %d %f %f %f %f %f %f %f %f",&i[0],&i[1],&f[0],&f[1],&f[2],&f[3],&f[4],&f[5],&f[6],&f[7])==10){if(primSeen>=MaxPrimitives||i[0]<0||i[0]>=int(PrimKind::Count)||i[1]<0||i[1]>=int(CombineKind::Count))return false;Primitive p;p.kind=PrimKind(i[0]);p.combine=CombineKind(i[1]);p.sx=f[0];p.sy=f[1];p.sz=f[2];p.ox=f[3];p.oy=f[4];p.oz=f[5];p.shell=f[6];p.smooth=f[7];o.prim[size_t(primSeen++)]=p;}
  else if(std::sscanf(ln.c_str(),"material %f %f %f %f %f",&f[0],&f[1],&f[2],&f[3],&f[4])==5)o.material={f[0],f[1],f[2],f[3],f[4]};
  else if(std::sscanf(ln.c_str(),"lighting %f %f %f %f %f %d",&f[0],&f[1],&f[2],&f[3],&f[4],&i[0])==6)o.lighting={f[0],f[1],f[2],f[3],f[4],i[0]!=0};
  else if(std::sscanf(ln.c_str(),"camera %d %f %f %f %f",&i[0],&f[0],&f[1],&f[2],&f[3])==5){if(i[0]<0||i[0]>=int(CameraMode::Count))return false;o.camera={CameraMode(i[0]),f[0],f[1],f[2],f[3]};}
  else if(std::sscanf(ln.c_str(),"motion %f %f %f %f",&f[0],&f[1],&f[2],&f[3])==4)o.motion={f[0],f[1],f[2],f[3]};
  else if(std::sscanf(ln.c_str(),"modulation %d %f %f %f %f %f %f",&i[0],&f[0],&f[1],&f[2],&f[3],&f[4],&f[5])==7)o.modulation={std::uint8_t(std::clamp(i[0],0,255)),f[0],f[1],f[2],f[3],f[4],f[5]};
  else if(std::sscanf(ln.c_str(),"palette %f %f",&f[0],&f[1])==2){o.paletteOffset=f[0];o.paletteSpread=f[1];}
  else if(std::sscanf(ln.c_str(),"field %f %f %f %f %f %f %f %f %d %f %d %d",&f[0],&f[1],&f[2],&f[3],&f[4],&f[5],&f[6],&f[7],&i[0],&g,&i[1],&i[2])==12){if(i[0]<0||i[0]>=int(AccentKind::Count))return false;for(int k=0;k<8;++k)o.field.v[size_t(k)]=f[k];o.field.accent=AccentKind(i[0]);o.field.accentSize=g;o.field.phraseBeats=std::uint8_t(std::clamp(i[1],0,255));o.field.octaves=std::uint8_t(std::clamp(i[2],0,255));}
  else return false;
 }
 if(!header||domainSeen!=o.domainCount||primSeen!=o.primCount)return false;
 w=o;return true;
}
}

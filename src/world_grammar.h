#pragma once
#include "frame_constants.h"
#include "palette_shader.h"
#include "world_fields.h"
#include "world_spec.h"
#include <string>
// Deterministic HLSL emission for a validated WorldSpec. Only whitelisted library modules are used,
// every loop has a constant bound, and every number printed comes from the spec after validation.
// The generated source is a self-contained pixel shader (entry psGen) over the shared Frame cbuffer;
// it deliberately does not include the renderer's main shader, so it compiles in a fraction of a second.
namespace rarestage::gen {

inline std::uint64_t HashSource(const std::string&s){std::uint64_t h=14695981039346656037ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}return h;}

inline std::string Num(float v){if(!std::isfinite(v))v=0;if(std::fabs(v)<.0005f)v=0;char b[40];std::snprintf(b,sizeof b,"%.3f",double(v));return b;}
inline std::string Int(int v){return std::to_string(v);}

inline const char* PaletteRingHlsl(){
 static const char* ring=
#include "palette_ring_shader.h"
 ;
 return ring;
}

// Whitelisted building blocks. Every function is total: no division without a floor, no unbounded loop.
inline const char* LibraryHlsl(){
 return R"(
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
static float3 gCam;
float gHash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float2 gRot(float2 p,float a){float c=cos(a),s=sin(a);return float2(c*p.x-s*p.y,s*p.x+c*p.y);}
float P(int i){return g_Param[i>>2][i&3];}
float sdBox(float3 p,float3 b){float3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}
float sdCyl(float3 p,float r,float h){float2 d=abs(float2(length(p.xz),p.y))-float2(r,h);return min(max(d.x,d.y),0.)+length(max(d,0.));}
float sdTorus(float3 p,float R,float r){float2 q=float2(length(p.xz)-R,p.y);return length(q)-r;}
float sdPrism(float3 p,float2 h){float3 q=abs(p);return max(q.z-h.y,max(q.x*.866025+p.y*.5,-p.y)-h.x*.5);}
float gGyroid(float3 p,float s,float t){s=max(s,.1);p*=s;return (abs(dot(sin(p),cos(p.zxy)))-t)*.55/s;}
float gSmin(float a,float b,float k){k=max(k,.001);float h=saturate(.5+.5*(b-a)/k);return lerp(b,a,h)-k*h*(1.-h);}
float gSmax(float a,float b,float k){return -gSmin(-a,-b,k);}
float3 gRepC(float3 p,float3 c,float3 m){c=max(c,.05);float3 r=p-c*round(p/c);return lerp(p,r,m);}
float3 gRepP(float3 p,float n){n=max(n,2.);float s=6.2831853/n;float a=atan2(p.z,p.x);a=a-s*floor(a/s+.5);float r=length(p.xz);return float3(r*cos(a),p.y,r*sin(a));}
float3 gSafeNormalize(float3 v){return v*rsqrt(max(dot(v,v),1e-8));}
)";
}

struct Emitted{std::string source;std::uint64_t hash{0};float complexity{0};Reject reject{Reject::None};};

inline bool HasDollyAxis(const WorldSpec&w){for(int i=0;i<w.domainCount;++i)if(w.domain[size_t(i)].kind==DomainKind::RepeatCartesian&&(w.domain[size_t(i)].axisMask&4))return true;return false;}
inline Reject ValidateWorld(const WorldSpec&w){
 Reject r=Validate(w);if(r!=Reject::None)return r;
 // A dolly camera travels along z and needs repeated space along z to have anything to look at.
 if(w.camera.mode==CameraMode::Dolly&&!HasDollyAxis(w))return Reject::Incompatible;
 for(int i=0;i<w.domainCount;++i){const auto&d=w.domain[size_t(i)];if((d.kind==DomainKind::Fold)&&!InRange(d.c,0.f,1.5f))return Reject::Range;}
 return Reject::None;
}

inline std::string DomainCode(const WorldSpec&w){
 std::string c;
 for(int i=0;i<w.domainCount;++i){const auto&d=w.domain[size_t(i)];
  switch(d.kind){
   case DomainKind::RepeatCartesian:
    c+="  p=gRepC(p,float3("+Num(d.a)+","+Num(d.b)+","+Num(d.c)+")/dens,float3("+Int(d.axisMask&1?1:0)+","+Int(d.axisMask&2?1:0)+","+Int(d.axisMask&4?1:0)+"));\n";break;
   case DomainKind::RepeatPolar:c+="  p=gRepP(p,"+Num(d.a)+");\n";break;
   case DomainKind::Kaleidoscope:
    c+="  [unroll] for(int k=0;k<"+Int(int(std::lround(d.a)))+";++k){p.xz=abs(p.xz);p.xz=gRot(p.xz,"+Num(d.b)+"+k*.37);p.x-="+Num(.15f+d.b*.2f)+";}\n";break;
   case DomainKind::Fold:
    c+="  [unroll] for(int k=0;k<"+Int(int(std::lround(d.a)))+";++k){p=abs(p)-float3("+Num(d.b*.5f)+","+Num(d.b*.35f)+","+Num(d.b*.2f)+");p.xy=gRot(p.xy,"+Num(d.c)+"+k*.41);}\n";break;
   case DomainKind::Twist:c+="  p.xy=gRot(p.xy,p.z*("+Num(d.a)+"+warp*.3));\n";break;
   case DomainKind::Displace:c+="  p+=("+Num(d.b)+"+disp*.12)*sin(p.yzx*"+Num(d.a)+"+time*.35);\n";break;
   default:break;}
 }
 return c;
}

inline std::string PrimitiveCode(const Primitive&p,int index){
 const std::string q="q"+Int(index),d="d"+Int(index);
 std::string c="  float3 "+q+"=p-float3("+Num(p.ox)+","+Num(p.oy)+","+Num(p.oz)+");\n  float "+d+";\n";
 const std::string sx=Num(p.sx),sy=Num(p.sy),sz=Num(p.sz);
 switch(p.kind){
  case PrimKind::Box:case PrimKind::Slab:c+="  "+d+"=sdBox("+q+",float3("+sx+","+sy+","+sz+"));\n";break;
  case PrimKind::Prism:c+="  "+d+"=sdPrism(float3("+q+".x,"+q+".y/"+sy+","+q+".z),float2("+sx+","+sz+"))*min("+sy+",1.);\n";break;
  case PrimKind::Cylinder:c+="  "+d+"=sdCyl("+q+","+sx+","+sy+");\n";break;
  case PrimKind::Monolith:c+="  {float tp=1.-.25*saturate("+q+".y/"+sy+"*.5+.5);"+d+"=sdBox("+q+"*float3(1./tp,1.,1./tp),float3("+sx+","+sy+","+sz+"))*tp;}\n";break;
  case PrimKind::Torus:c+="  "+d+"=sdTorus("+q+","+sx+","+sy+");\n";break;
  case PrimKind::Sphere:c+="  "+d+"=length("+q+")-"+sx+";\n";break;
  case PrimKind::Gyroid:c+="  "+d+"=gGyroid("+q+","+sx+","+sy+");\n";break;
  case PrimKind::Conduit:c+="  "+d+"=length("+q+".xy)-"+sx+";\n";break;
  case PrimKind::Arch:c+="  "+d+"=gSmax(sdBox("+q+",float3("+sx+","+sy+","+sz+")),-(length("+q+".xy-float2(0.,"+Num(-p.sy*.2f)+"))-"+Num(p.sx*.75f)+"),.02);\n";break;
  default:c+="  "+d+"=1000.;\n";break;}
 if(p.shell>0)c+="  "+d+"=abs("+d+")-"+Num(p.shell)+";\n";
 return c;
}

inline std::string MapCode(const WorldSpec&w){
 std::string c="float map(float3 p){\n";
 c+="  float3 pw=p;\n";
 c+="  float disp=lerp(0.,P(1),"+Num(w.modulation.displacement)+");\n";
 c+="  float warp=lerp(0.,P(3),"+Num(w.modulation.warp)+");\n";
 c+="  float dens=max(lerp(1.,P(4),"+Num(w.modulation.density)+"),.4);\n";
 c+="  float sc=max(lerp(1.,P(0),"+Num(w.modulation.pulse)+")*(1.+"+Num(w.motion.breathe)+"*sin(time*.7)),.3);\n";
 c+="  p/=sc;\n";
 if(w.camera.mode==CameraMode::Dolly)c+="  p.xy=gRot(p.xy,sin(time*.2)*"+Num(w.motion.sway)+"*.3+P(8)*.4);\n";
 else c+="  p.xz=gRot(p.xz,time*"+Num(w.motion.rotation)+"+P(8)*.6);\n";
 c+=DomainCode(w);
 c+="  float d=1000.;\n";
 for(int i=0;i<w.primCount;++i){
  const auto&p=w.prim[size_t(i)];c+=PrimitiveCode(p,i);const std::string d="d"+Int(i);
  switch(p.combine){
   case CombineKind::Union:c+="  d=min(d,"+d+");\n";break;
   case CombineKind::Intersect:c+="  d=max(d,"+d+");\n";break;
   case CombineKind::Subtract:c+="  d=max(d,-"+d+");\n";break;
   default:c+="  d=gSmin(d,"+d+","+Num(p.smooth)+");\n";break;}
 }
 c+="  return max(d*sc,.9-length(pw-gCam));\n}\n";
 return c;
}

inline float StepScale(const WorldSpec&w){
 float s=.85f;for(int i=0;i<w.domainCount;++i){const auto k=w.domain[size_t(i)].kind;if(k==DomainKind::Twist||k==DomainKind::Displace)s=std::min(s,.65f);}
 for(int i=0;i<w.primCount;++i)if(w.prim[size_t(i)].kind==PrimKind::Gyroid)s=std::min(s,.6f);return s;
}

inline std::string CameraCode(const WorldSpec&w){
 const auto&c=w.camera;std::string s;
 s+="  float tt=time*"+Num(c.speed)+"*max(P(5),0.);\n";
 s+="  float camD="+Num(c.distance)+"+P(6)*"+Num(c.distance*.35f)+";\n";
 s+="  float3 ro,ta;\n";
 switch(c.mode){
  case CameraMode::Dolly:
   s+="  ro=float3(sin(tt*.7)*"+Num(w.motion.sway)+",1.*"+Num(c.height)+"+cos(tt*.5)*.2,tt*"+Num(2.f+std::fabs(w.motion.scroll)*2.f)+");\n  ta=ro+float3(sin(tt*.3)*.4,0.,1.);\n";break;
  case CameraMode::Drift:
   s+="  ro=float3(sin(tt*.31)*camD*.6,"+Num(c.height)+"+sin(tt*.23),cos(tt*.27)*camD*.6);\n  ta=float3(sin(tt*.19),0.,cos(tt*.17));\n";break;
  case CameraMode::LowSweep:
   s+="  ro=float3(sin(tt)*camD,"+Num(c.height)+",cos(tt)*camD);\n  ta=float3(0.,1.2,0.);\n";break;
  default:
   s+="  ro=float3(sin(tt)*camD,"+Num(c.height)+",cos(tt)*camD);\n  ta=float3(0.,0.,0.);\n";break;}
 return s;
}

inline std::string PixelCode(const WorldSpec&w){
 const auto&m=w.material;const auto&l=w.lighting;
 std::string s="float4 psGen(O i):SV_TARGET{\n";
 s+="  float2 q=float2(i.uv.x*2.-1.,1.-i.uv.y*2.);q.x*=resolution.x/max(resolution.y,1.);\n";
 s+=CameraCode(w);
 s+="  gCam=ro;\n";
 s+="  float3 fw=gSafeNormalize(ta-ro);float3 rt=gSafeNormalize(cross(float3(0.,1.,0.),fw));float3 up=cross(fw,rt);\n";
 s+="  float3 rd=gSafeNormalize(fw*"+Num(w.camera.fov)+"+rt*q.x+up*q.y);\n";
 s+="  float farD="+Num(w.camera.distance+16.f)+";float t=0.;bool hit=false;\n";
 s+="  [loop] for(int s=0;s<"+Int(w.budget.marchSteps)+";++s){float d=map(ro+rd*t)*"+Num(StepScale(w))+";if(d<.0012*(1.+t)){hit=true;break;}t+=d;if(t>farD)break;}\n";
 s+="  float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);\n";
 s+="  float3 bg=primary*.035*(1.+rd.y)+spectral*.015;\n";
 s+="  float3 col=bg;\n";
 s+="  if(hit){\n";
 s+="    float3 p=ro+rd*t;float e=.0015*(1.+t);\n";
 s+="    float3 n=gSafeNormalize(float3(1,-1,-1)*map(p+float3(e,-e,-e))+float3(-1,-1,1)*map(p+float3(-e,-e,e))+float3(-1,1,-1)*map(p+float3(-e,e,-e))+float3(1,1,1)*map(p+float3(e,e,e)));\n";
 s+="    float3 L=gSafeNormalize(float3(cos("+Num(l.azimuth)+")*cos("+Num(l.elevation)+"),sin("+Num(l.elevation)+"),sin("+Num(l.azimuth)+")*cos("+Num(l.elevation)+")));\n";
 s+="    float diff=saturate(dot(n,L)*.5+.5);diff*=diff;\n";
 s+="    float ndv=saturate(dot(n,-rd));float fres=pow(1.-ndv,"+Num(m.fresnel)+");\n";
 s+="    float spec=pow(saturate(dot(reflect(rd,n),L)),"+Num(m.specular)+");\n";
 s+="    float ao=saturate(.5+.5*map(p+n*.18)/.18);\n";
 if(l.shadows){
  s+="    float sh=1.;float st=.04;[loop] for(int k=0;k<14;++k){float h=map(p+n*.02+L*st);sh=min(sh,8.*h/st);st+=clamp(h,.04,.6);if(h<.001||st>8.)break;}sh=saturate(sh);\n";
 } else s+="    float sh=1.;\n";
 s+="    float cm=saturate(.5+.5*sin((p.y*.7+p.z*.35+p.x*.2)*"+Num(m.colorSpread)+"+"+Num(w.paletteOffset)+"*6.2831853+time*.1));\n";
 s+="    float3 base=lerp(primary,accent,cm);\n";
 s+="    float em=lerp(1.,max(P(2),0.),"+Num(w.modulation.emission)+");\n";
 s+="    float sparkle=lerp(0.,P(15),.8);\n";
 s+="    col=base*("+Num(l.ambient)+"+diff*(.35+.65*sh))*ao;\n";
 s+="    col+=spectral*fres*"+Num(l.rim)+"*ao;\n";
 s+="    col+=base*"+Num(m.emission)+"*em*pow(1.-ndv,2.)*"+Num(m.edgeGlow)+";\n";
 s+="    col+=spectral*spec*(.5+sparkle*2.)*sh;\n";
 s+="    float fogK="+Num(l.fog)+"*lerp(1.,max(P(9),.05)*3.,"+Num(w.modulation.atmosphere)+");\n";
 s+="    col=lerp(bg,col,exp(-fogK*t));\n";
 s+="  }\n";
 s+="  col=min(max(col,0.),4.);\n  if(!all(isfinite(col)))col=bg;\n";
 s+="  return float4(col,1.);\n}\n";
 return s;
}

// Source text only; the caller decides when to compile it.
inline Emitted Emit(const WorldSpec&spec){
 Emitted out;out.reject=ValidateWorld(spec);if(out.reject!=Reject::None)return out;
 out.complexity=ComplexityScore(spec);
 std::string src="// RareStage generated world id="+std::to_string((unsigned long long)spec.id)+" seed="+std::to_string(spec.seed)+" family="+FamilyNames[int(spec.family)]+" grammar="+std::to_string(GrammarVersion)+"\n";
 src+="\n";src+=RS_FRAME_CBUFFER_HLSL;src+="\n";src+=PaletteRingHlsl();src+=RS_PALETTE_FUNCS_HLSL;
 if(IsField(spec.family)){src+=FieldHeader(spec);src+=FieldLibraryHlsl();src+=FieldBody(spec);}
 else{src+=LibraryHlsl();src+=MapCode(spec);src+=PixelCode(spec);}
 if(src.size()>20000){out.reject=Reject::Complexity;return out;}
 out.hash=HashSource(src);out.source=std::move(src);return out;
}
}

#pragma once
#include "show_director.h"
#include <array>
#include <cmath>
namespace rarestage::show {
struct Rehearsal {float continuity{},estimatedMs{};bool measured{},native{};};
// Small analytic rehearsal; no extra graphics work is submitted from the live loop.
class RouteRehearsal {
 struct Cost {float ms{},pixels{};unsigned samples{};};
 std::array<Cost,SceneCount> costs_{};
public:
 void observe(int scene,float milliseconds,float pixels){
  if(scene<0||scene>=SceneCount||!std::isfinite(milliseconds)||milliseconds<=0||milliseconds>100||pixels<1)return;
  auto&c=costs_[scene];float normalized=milliseconds*8294400.f/pixels;
  c.ms=c.samples?c.ms+(normalized-c.ms)*.08f:normalized;c.pixels=pixels;++c.samples;
 }
 Rehearsal evaluate(int from,int to,float phase,float pixels)const{
  Rehearsal r;const auto&a=Scenes[from];const auto&b=Scenes[to];r.native=NativeBridge(from,to);
  float distance=0;
  // Compare the route's nominal anchor and velocity envelope over the next eight beats.
  for(int i=0;i<=8;++i){float t=phase+float(i);
   float ax=a.anchorX+std::sin(t*a.pace)*.04f,ay=a.anchorY+std::cos(t*a.pace)*.025f;
   float bx=b.anchorX+std::sin(t*b.pace)*.04f,by=b.anchorY+std::cos(t*b.pace)*.025f;
   distance+=std::hypot(ax-bx,ay-by)+std::abs(a.pace-b.pace)*.2f;
  }
  r.continuity=std::clamp(Compatibility(from,to)-distance/9.f-.08f*std::abs(a.scale-b.scale)+(r.native?.16f:0.f),0.f,1.f);
  const auto&ca=costs_[from];const auto&cb=costs_[to];r.measured=ca.samples>=8&&cb.samples>=8;
  // Unknown scenes keep a conservative estimate; they remain eligible to be discovered.
  float ma=ca.samples>=8?ca.ms:3.6f*a.relativeCost,mb=cb.samples>=8?cb.ms:3.6f*b.relativeCost;
  r.estimatedMs=(r.native?std::max(ma,mb)*1.18f:ma+mb+.55f)*pixels/8294400.f;return r;
 }
};
struct Theme {const wchar_t* name;std::array<float,4> shape;int motion;};
inline constexpr std::array<Theme,6> Themes{{
 {L"RIVER / travelling rows",{.80f,.10f,.00f,.10f},0},
 {L"CROWN / mirrored fans",{.10f,.85f,.05f,.75f},1},
 {L"ORBIT / turning arcs",{.10f,.20f,.85f,.35f},3},
 {L"PULSE / interlocking lanes",{-.70f,.30f,.00f,.90f},0},
 {L"ASCENT / rising structures",{.35f,-.75f,.20f,.55f},2},
 {L"TIDAL / broad waves",{.65f,.15f,-.65f,.20f},4}
}};
class ThemeMemory {
 int theme_{},visits_{},lastBoundary_{-1},sequence_{};bool held_{},requested_{},following_{};
 std::array<unsigned,6> uses_{{1,0,0,0,0,0}};
 std::array<float,4> from_{Themes[0].shape},value_{Themes[0].shape};float started_{-100};
public:
 int current()const{return theme_;}bool held()const{return held_;}int visits()const{return visits_;}
 const std::array<float,4>& value()const{return value_;}
 void follow(int id,const std::array<float,4>&value){theme_=id;value_=from_=value;requested_=false;following_=true;}
 void visit(){++visits_;}void hold(bool value){held_=value;}void request(){requested_=true;}
 void update(float beat,bool safe,bool automatic){
  int boundary=int(std::floor(beat/16));
  if(following_){from_=value_;started_=beat;lastBoundary_=boundary;following_=false;}
  if(safe&&std::fmod(beat,16.f)<.20f&&boundary!=lastBoundary_){lastBoundary_=boundary;
   if(requested_||(automatic&&!held_&&visits_>=3)){
    int next=-1;float best=-1e9f;
    for(int i=0;i<6;++i){if(i==theme_)continue;float score=-float(uses_[i])*2.f+float((i*7+sequence_*3)%11)*.01f;if(score>best){best=score;next=i;}}
    from_=value_;theme_=next;++uses_[theme_];++sequence_;visits_=0;started_=beat;requested_=false;
   }
  }
  float u=std::clamp((beat-started_)/8.f,0.f,1.f);u=u*u*u*(u*(u*6-15)+10);
  for(int i=0;i<4;++i)value_[i]=from_[i]+(Themes[theme_].shape[i]-from_[i])*u;
 }
};
struct Branch {Plan route;int section{-1};};
class Composer {
 std::array<Branch,3> branches_{};int source_{-1},category_{-1},revision_{-1};float builtAt_{-1000};
public:
 void invalidate(){source_=-1;}
 bool valid(int source,int category,int revision,float beat)const{return source==source_&&category==category_&&revision>=revision_&&revision<=revision_+1&&beat>=builtAt_&&beat-builtAt_<=32;}
 void prepare(const Director&director,Context context,int revision,float beat){
  source_=context.current;category_=context.category;revision_=revision;builtAt_=beat;
  const int sections[3]={context.section==2||context.section==3?4:context.section,2,3};
  for(int i=0;i<3;++i){branches_[i].section=sections[i];Context branch=context;branch.section=sections[i];
   if(branch.section!=context.section){branch.boundedNext=false;branch.imageAdjustment.fill(0);}
   float target=sections[i]==2?.75f:sections[i]==3?.28f:context.energy;
   for(int scene=0;scene<SceneCount;++scene)branch.fit[scene]+=std::abs(Scenes[scene].energy-context.energy)*.6f-std::abs(Scenes[scene].energy-target)*.9f;
   branches_[i].route=director.plan(branch);
  }
 }
 const std::array<Branch,3>& branches()const{return branches_;}
 int confirmed(int section,int source,int category,int revision,float beat)const{
  if(!valid(source,category,revision,beat))return -1;
  for(const auto&b:branches_)if(b.section==section&&b.route.next>=0&&(!category||Scenes[b.route.next].category==category-1))return b.route.next;
  return -1;
 }
};
}

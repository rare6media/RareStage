#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

namespace rarestage::show {
// Category is an operator filter. Motion describes geometry used by the handoff planner.
// 0 surface, 1 ribbon, 2 perspective, 3 radial, 4 atmospheric.
struct Scene { int category, motion; float energy, density; };
inline constexpr std::array<Scene,21> Scenes{{
 {0,0,.42f,.85f},{2,2,.54f,.50f},{3,3,.25f,.28f},{0,0,.68f,.90f},
 {1,1,.36f,.65f},{0,0,.58f,.35f},{1,1,.72f,.75f},{3,4,.35f,.38f},
 {1,1,.48f,.65f},{0,0,.64f,.85f},{1,1,.50f,.60f},{0,0,.60f,.85f},
 {2,2,.70f,.85f},{0,0,.66f,.85f},{2,2,.57f,.55f},{0,0,.66f,.85f},
 {2,2,.46f,.65f},{2,2,.40f,.40f},{1,1,.48f,.55f},{3,3,.56f,.50f},{2,2,.65f,.70f}
}};
inline float Compatibility(int from,int to){
 const auto&a=Scenes[from];const auto&b=Scenes[to];
 static constexpr float flow[5][5]={{1,.88f,.80f,.72f,.73f},{.88f,1,.82f,.84f,.88f},{.80f,.82f,1,.91f,.78f},{.72f,.84f,.91f,1,.86f},{.73f,.88f,.78f,.86f,1}};
 return std::clamp(flow[a.motion][b.motion]-.16f*std::abs(a.density-b.density),0.f,1.f);
}
inline int HandoffStyle(int from,int to){
 const int a=Scenes[from].motion,b=Scenes[to].motion;
 if(a==b)return a==0?0:a==1||a==4?1:a==3?2:3;
 if(a==2||b==2)return 3;
 if(a==3||b==3)return 2;
 return 1;
}
struct Context {
 int current{}, section{}, category{}, sequence{}, preferred{-1};
 float energy{}, trend{};
 std::array<float,21> fit{};
};
struct Plan {int next{-1},then{-1},alternative{-1};float score{-10000},runner{-10000};};
class Director {
 std::array<int,21> lastVisit_{};
 int visits_{},familyRun_{},lastMotion_{-1};
public:
 Director(){lastVisit_.fill(-100);}
 void visit(int scene){++visits_;lastVisit_[scene]=visits_;int motion=Scenes[scene].motion;familyRun_=motion==lastMotion_?familyRun_+1:1;lastMotion_=motion;}
 int familyRun()const{return familyRun_;}
 Plan plan(const Context&c)const{
  Plan result,incumbent;
  auto allowed=[&](int scene){return !c.category||Scenes[scene].category==c.category-1;};
  auto fatigue=[&](int scene){const int age=visits_-lastVisit_[scene];return age<3?float(3-age)*.85f:0.f;};
  const bool contrast=familyRun_>=2;
  // Compare complete next/develop-or-release routes, not only the best immediate scene.
  for(int next=0;next<int(Scenes.size());++next){
   if(next==c.current||!allowed(next))continue;
   const auto&n=Scenes[next];
   const bool same=n.motion==Scenes[c.current].motion;
   float immediate=c.fit[next]+Compatibility(c.current,next)*.65f-fatigue(next);
   immediate+=contrast?(same?-.78f:.38f):(same?.22f:0.f);
   if(c.section==1)immediate-=std::max(0.f,n.energy-.62f)*1.8f; // reserve the peak
   if(c.section==3||c.section==0)immediate-=n.density*.25f;
   if(c.section==2)immediate+=n.energy*.30f;
   float bestExit=-10000;int then=-1;
   const float futureEnergy=std::clamp(c.energy+c.trend*2.f,.12f,.85f);
   for(int following=0;following<int(Scenes.size());++following){
    if(following==next||following==c.current||!allowed(following))continue;
    float exit=Compatibility(next,following)*.60f-std::abs(Scenes[following].energy-futureEnergy)*.35f-fatigue(following)*.2f;
    // After contrast, develop the new geometry; after development, keep a clean exit.
    exit+=((Scenes[following].motion==n.motion)==contrast)?.15f:0.f;
    if(exit>bestExit){bestExit=exit;then=following;}
   }
   if(then<0){then=c.current;bestExit=0;}
   const float score=immediate+bestExit*.42f;
   if(next==c.preferred){incumbent.next=next;incumbent.then=then;incumbent.score=score;}
   if(score>result.score){result.runner=result.score;result.alternative=result.next;result.score=score;result.next=next;result.then=then;}
   else if(score>result.runner){result.runner=score;result.alternative=next;}
  }
  if(incumbent.next>=0&&incumbent.score>=result.score-.18f){if(incumbent.next==result.next)return result;incumbent.runner=result.score;incumbent.alternative=result.next;return incumbent;}
  return result;
 }
};
}

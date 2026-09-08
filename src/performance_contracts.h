#pragma once
#include "show_motion.h"
#include "show_memory.h"
// Deterministic tests of production memory and trajectory; no graphics device required.
inline bool PerformanceSelfTest(){
 bool passed=true;std::ofstream log("RareStage-performance-contracts.csv");log<<"check,passed,value\n";
 auto check=[&](const std::string&name,bool ok,double value=0){log<<name<<','<<ok<<','<<value<<'\n';passed&=ok;};
 rarestage::show::SetMemory memory;
 for(int world=0;world<21;++world){std::array<bool,64>seen{};for(int visit=0;visit<64;++visit){int p=memory.program(world,visit%5);seen[p]=true;}check("programs_before_reuse_"+std::to_string(world),std::count(seen.begin(),seen.end(),true)==64);}
 for(int kind=0;kind<2;++kind){std::array<bool,64>seen{};for(int visit=0;visit<64;++visit)seen[memory.identity(kind,visit%5)]=true;check("identity_pairs_before_reuse_"+std::to_string(kind),std::count(seen.begin(),seen.end(),true)==64);}
 for(int hz:{60,120,144}){
  CornerChoreography motion,reference;double maxSpeed=0,maxAcceleration=0;float prevX=0,prevY=0,prevVx=0,prevVy=0;bool exact=false,bounded=true;
  for(int frame=0;frame<=hz*12;++frame){float t=float(frame)/hz;if(frame==hz*2)motion.aim(t,6,true);motion.update(t,false,0,frame>=hz*4?1:0);reference.update(t,false,0,0);
   if(frame>0){maxSpeed=std::max(maxSpeed,double(std::hypot(motion.x-prevX,motion.y-prevY)*hz));if(frame>1)maxAcceleration=std::max(maxAcceleration,double(std::hypot(motion.vx-prevVx,motion.vy-prevVy)*hz));}
   bounded&=std::abs(motion.x)<=1.000001f&&std::abs(motion.y)<=1.000001f;
   if(frame==hz*6)exact=motion.arrived()&&std::abs(motion.x)==1&&std::abs(motion.y)==1&&motion.vx==0&&motion.vy==0;
   if(frame==hz*7)motion.release(t);
   prevX=motion.x;prevY=motion.y;prevVx=motion.vx;prevVy=motion.vy;
  }
  check("corner_exact_"+std::to_string(hz),exact&&bounded);check("speed_bounded_"+std::to_string(hz),maxSpeed<1,maxSpeed);check("acceleration_bounded_"+std::to_string(hz),maxAcceleration<1.5,maxAcceleration);
 }
 CornerChoreography a,b;double dropDifference=0;
 for(int f=0;f<2400;++f){float t=f/120.f;a.update(t,false,0,unsigned(f/137));b.update(t,false,0,0);dropDifference=std::max(dropDifference,double(std::hypot(a.x-b.x,a.y-b.y)));}
 check("unexpected_drops_do_not_move_mark",dropDifference==0,dropDifference);
 // Cancel a forecast / exit without teleporting or introducing an out-of-bounds step.
 CornerChoreography cancel;cancel.update(0,false,0,0);cancel.aim(0,4,true);cancel.update(1,false,0,0);const float cx=cancel.x,cy=cancel.y;cancel.release(1);cancel.update(1,false,0,0);check("cancel_continuity",cx==cancel.x&&cy==cancel.y);
 rarestage::show::Director director;rarestage::show::Context context;context.current=3;context.fit.fill(1);director.visit(3);
 std::array<bool,21>scenes{};std::array<std::array<bool,21>,21>routes{};int distinctRoutes=0;
 for(int song=0;song<20;++song)for(int cue=0;cue<8;++cue){context.section=cue%5;context.energy=context.section==2?.7f:.4f;context.trend=context.section==1?.08f:0;auto p=director.plan(context);if(p.next<0||p.next==context.current){passed=false;continue;}scenes[p.next]=true;if(!routes[context.current][p.next]){routes[context.current][p.next]=true;++distinctRoutes;}director.visit(p.next);context.current=p.next;}
 check("twenty_song_scene_coverage",std::count(scenes.begin(),scenes.end(),true)==21,std::count(scenes.begin(),scenes.end(),true));check("twenty_song_route_variety",distinctRoutes>=70,distinctRoutes);
 return passed&&log.good();
}

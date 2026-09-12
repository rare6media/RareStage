#pragma once
#include "show_motion.h"
#include "show_memory.h"
// Deterministic tests of production memory and trajectory; no graphics device required.
inline bool PerformanceSelfTest(){
 bool passed=true;std::ofstream log("RareStage-performance-contracts.csv");log<<"check,passed,value\n";
 auto check=[&](const std::string&name,bool ok,double value=0){log<<name<<','<<ok<<','<<value<<'\n';passed&=ok;};
 rarestage::show::SetMemory memory;
 for(int world=0;world<rarestage::show::SceneCount;++world){std::array<bool,64>seen{};for(int visit=0;visit<64;++visit){int p=memory.program(world,visit%5);seen[p]=true;}check("programs_before_reuse_"+std::to_string(world),std::count(seen.begin(),seen.end(),true)==64);}
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
 std::array<bool,rarestage::show::SceneCount>scenes{};std::array<std::array<bool,rarestage::show::SceneCount>,rarestage::show::SceneCount>routes{};int distinctRoutes=0;
 for(int song=0;song<20;++song)for(int cue=0;cue<8;++cue){context.section=cue%5;context.energy=context.section==2?.7f:.4f;context.trend=context.section==1?.08f:0;auto p=director.plan(context);if(p.next<0||p.next==context.current){passed=false;continue;}scenes[p.next]=true;if(!routes[context.current][p.next]){routes[context.current][p.next]=true;++distinctRoutes;}director.visit(p.next);context.current=p.next;}
 bool curatedCoverage=true;for(int scene=0;scene<rarestage::show::SceneCount;++scene)curatedCoverage&=scenes[scene]==rarestage::show::AutoScene(scene);
 check("twenty_song_curated_scene_coverage",curatedCoverage,std::count(scenes.begin(),scenes.end(),true));check("twenty_song_route_variety",distinctRoutes>=70,distinctRoutes);

 for(int i=0;i<rarestage::show::SceneCount;++i)check("catalog_key_"+std::to_string(i),(i>=24||rarestage::show::Scenes[i].key!=0)&&(rarestage::show::Scenes[i].key?rarestage::show::SceneForKey(rarestage::show::Scenes[i].key)==i:rarestage::show::SceneForKey(0)==-1));
 check("legacy_scene_keys",rarestage::show::SceneForKey('U')==14&&rarestage::show::SceneForKey('I')==15);
 // Composer routes must be conditional, fresh, category-safe and canceled by intervention.
 using namespace rarestage::show;
 Composer composer;context.current=3;context.section=1;context.category=0;context.energy=.45f;
 composer.prepare(director,context,7,96);int drop=composer.confirmed(2,3,0,8,104);
 check("confirmed_drop_branch",drop>=0&&drop<SceneCount&&drop!=3);
 check("unmatched_section_has_no_commit",composer.confirmed(0,3,0,7,100)==-1);
 check("expired_branch_rejected",composer.confirmed(2,3,0,8,129)==-1);
 check("changed_category_rejected",composer.confirmed(2,3,2,8,104)==-1);
 check("changed_scene_rejected",composer.confirmed(2,9,0,8,104)==-1);
 check("changed_revision_rejected",composer.confirmed(2,3,0,10,104)==-1);
 composer.invalidate();check("operator_interrupt_clears_branches",composer.confirmed(2,3,0,8,104)==-1);
 for(int category=1;category<=4;++category){context.category=category;composer.prepare(director,context,0,0);for(const auto&branch:composer.branches())check("branch_filter_"+std::to_string(category),branch.route.next>=0&&Scenes[branch.route.next].category==category-1);}
 RouteRehearsal rehearsal;check("native_bridge_contract",NativeBridge(3,21)&&NativeBridge(18,22)&&NativeBridge(22,23)&&!NativeBridge(3,22));
 check("unknown_cost_is_labelled",!rehearsal.evaluate(3,21,0,8294400).measured);
 for(int i=0;i<16;++i){rehearsal.observe(3,2,8294400);rehearsal.observe(21,3,8294400);rehearsal.observe(10,9,8294400);}
 auto native=rehearsal.evaluate(3,21,0,8294400),costly=rehearsal.evaluate(3,10,0,8294400);
 check("measured_route_cost",native.measured&&costly.measured&&native.estimatedMs<costly.estimatedMs);
 check("resolution_cost_scaling",std::abs(rehearsal.evaluate(3,21,0,2073600).estimatedMs*4-native.estimatedMs)<.0001);
 rehearsal.observe(3,std::nanf(""),8294400);check("invalid_measurement_ignored",std::isfinite(rehearsal.evaluate(3,21,0,8294400).estimatedMs));
 ThemeMemory theme;theme.visit();theme.visit();theme.visit();theme.hold(true);theme.update(16,true,true);check("held_theme_survives_boundary",theme.current()==0);
 theme.request();theme.update(17,true,true);check("theme_waits_for_boundary",theme.current()==0);
 const auto before=theme.value();theme.update(32,true,true);check("theme_start_continuous",theme.value()==before&&theme.current()!=0);
 theme.update(40,true,true);check("theme_arrives_at_target",theme.value()==Themes[theme.current()].shape);
 const int held=theme.current();theme.visit();theme.visit();theme.visit();theme.update(48,true,true);check("hold_survives_next_theme_request",theme.current()==held);
 theme.hold(false);theme.update(64,false,true);check("unsafe_theme_boundary_deferred",theme.current()==held);theme.update(65,true,true);check("deferred_theme_waits_next_boundary",theme.current()==held);theme.update(80,true,true);check("theme_auto_resumes",theme.current()!=held);
 ThemeMemory follower;const std::array<float,4> received{{.2f,-.1f,.3f,.4f}};follower.follow(3,received);follower.update(128,true,true);check("network_theme_local_resume_continuity",follower.value()==received);follower.update(136,true,true);check("network_theme_local_resume_settles",follower.value()==Themes[3].shape);
 std::array<bool,6>themeSeen{};themeSeen[theme.current()]=true;
 for(int n=1;n<=18;++n){theme.visit();theme.visit();theme.visit();theme.update(80+n*16,true,true);themeSeen[theme.current()]=true;}
 check("theme_callbacks_cover_all_themes",std::count(themeSeen.begin(),themeSeen.end(),true)==6);
 return passed&&log.good();
}

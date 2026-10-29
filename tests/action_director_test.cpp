#include "action_director.h"
#include "show_director.h"
#include "show_composer.h"
#include <iostream>
#include <limits>
using namespace rarestage::show;
int main(){
 bool passed=true;auto check=[&](const char* name,bool value){std::cout<<name<<": "<<(value?"PASS":"FAIL")<<'\n';passed&=value;};
 bool legal=true;for(int scene=0;scene<SceneCount;++scene)for(int program=0;program<ProgramCount(scene);++program)for(float beat:{17.f,101.5f,8192.f}){
  auto c=PatternContract(scene,program);float settled=beat+3;auto exit=PreferredExit(scene,program,beat,beat-10,settled,.8f,false);
  legal&=exit>=settled+c.payoff&&exit>=beat-10+c.establish&&std::abs(std::remainder(exit-c.exitOffset,c.exitStride))<.001f;
  auto uncertain=PreferredExit(scene,program,beat,beat-10,settled,.1f,false);legal&=uncertain>=exit&&uncertain-exit<=24;
 }check("all_pattern_exits_finish_establishment_and_payoff_on_beat",legal);
 check("stage_reports_payoff_before_departure",CurrentAct(38,1,110,0,108,true)==MotionAct::Payoff&&CurrentAct(38,1,120,0,108,true)==MotionAct::Departure);
 check("render_budget_requires_known_spare_capacity",RehearsalBudget(2,3,8,.1f,1)&&!RehearsalBudget(7,7,8,.1f,1)&&!RehearsalBudget(2,7.5f,8,.1f,1)&&!RehearsalBudget(2,3,8,1,1)&&!RehearsalBudget(2,3,8,.1f,.1f)&&!RehearsalBudget(0,0,8,.1f,1)&&!RehearsalBudget(std::numeric_limits<float>::quiet_NaN(),3,8,.1f,1));
 std::array<std::uint8_t,16*16*4>image{};for(size_t i=0;i<image.size();i+=4){image[i]=image[i+1]=image[i+2]=128;image[i+3]=255;}
 auto feature=MeasureComposition(image.data(),16,16,16*4);check("pixel_analysis_reads_composition",std::abs(feature.luma-128.f/255)<.001f&&feature.coverage==1&&std::abs(feature.centerX-.5f)<.001f&&std::abs(feature.centerY-.5f)<.001f);
 std::array<Composition,5> good;good.fill(feature);auto broken=good;broken[2]={};check("blank_intermediate_fixture_is_penalized",JudgeComposition(broken).penalty>JudgeComposition(good).penalty+.2f);
 std::array<Composition,5> sparse;for(auto&f:sparse){f.luma=.018f;f.coverage=.02f;f.tiles.fill(.018f);}check("intentional_dark_and_sparse_scenes_are_accepted",JudgeComposition(sparse).penalty==0);
 auto reveal=sparse;for(int i=0;i<5;++i){reveal[i].luma=.018f+i*.08f;reveal[i].coverage=.02f+i*.15f;reveal[i].tiles.fill(reveal[i].luma);}check("planned_brightness_development_is_accepted",JudgeComposition(reveal).penalty<.01f);
 VisualRehearsal rehearsal;RehearsalContext context{};context.source=38;context.program=1;context.visit=4;rehearsal.begin(context,64,40,41,2,3);unsigned stale=rehearsal.epoch();
 check("out_of_order_results_rejected",!rehearsal.accept(stale,0,2,feature));rehearsal.cancel();rehearsal.begin(context,65,40,41,2,3);check("cancelled_job_cannot_contaminate_replacement",!rehearsal.accept(stale,0,0,feature));
 for(int i=0;i<5;++i){rehearsal.accept(rehearsal.epoch(),0,i,good[i]);rehearsal.accept(rehearsal.epoch(),1,i,broken[i]);}
 check("actual_image_penalty_enters_candidate_adjustment",rehearsal.done()&&rehearsal.adjustment(40,context,70)==0&&rehearsal.adjustment(41,context,70)<-.2f&&rehearsal.entry(40,context,70)==2);
 auto changed=context;++changed.revision;check("changed_music_and_expired_images_cannot_affect_route",rehearsal.adjustment(41,changed,70)==0&&rehearsal.entry(40,context,100)==-1&&rehearsal.entry(40,changed,70)==-1);
 Context route;route.current=38;route.section=4;route.energy=.6f;route.boundedNext=true;route.shortlist[40]=route.shortlist[41]=true;route.fit.fill(100);route.fit[40]=1.05f;route.fit[41]=1;
 Director director;auto original=director.plan(route);route.imageAdjustment[40]=-.45f;auto judged=director.plan(route);
 check("rendered_finalists_bound_selection_and_image_evidence_can_change_it",original.next==40&&judged.next==41);
 Composer composer;composer.prepare(director,route,0,64);
 check("different_musical_outcomes_do_not_inherit_a_visual_shortlist",composer.branches()[0].route.next==41&&composer.branches()[1].route.next!=40&&composer.branches()[1].route.next!=41&&composer.branches()[2].route.next!=40&&composer.branches()[2].route.next!=41);
 return passed?0:1;
}

#include "phrase_conductor.h"
#include "play_poses.h"
#include <iostream>
#include <fstream>
#include <limits>
using namespace rarestage::show;
int main(){
 bool passed=true;std::ofstream log("RareStage-phrase-contracts.csv");log<<"check,passed\n";
 auto check=[&](const char*name,bool ok){log<<name<<','<<ok<<'\n';if(!ok)std::cerr<<name<<'\n';passed&=ok;};
 PhraseMemory memory;PhraseConductor conductor;conductor.begin(8,12,0);PhraseEvidence e;e.sectionAge=100;
 const auto initial=conductor.sample(0);
 for(int beat=0;beat<32;++beat)conductor.update(float(beat),e,memory);
 check("recognizable_groove_held_32_beats",conductor.sample(31).to==initial.to);
 conductor.update(32,e,memory);const auto answer=conductor.sample(32);
 check("answer_starts_without_pose_jump",answer.from==initial.to&&answer.blend==0&&answer.gain==initial.gain&&answer.spread==initial.spread);
 check("related_answer_selected",answer.to!=answer.from&&answer.action==float(PhraseAction::Answer));
 e.held=true;for(int beat=33;beat<96;++beat)conductor.update(float(beat),e,memory);
 check("operator_hold_prevents_new_choices",conductor.sample(96).to==answer.to&&conductor.sample(96).blend==1);
 e.held=false;e.safe=false;conductor.update(104,e,memory);check("identity_or_handoff_protects_program",conductor.sample(104).to==answer.to);
 e.safe=true;e.section=2;e.revision=9;e.sectionAge=1;conductor.update(112,e,memory);
 check("confirmed_drop_develops_on_boundary",conductor.sample(112).action==float(PhraseAction::Develop)&&conductor.sample(120).gain>1.1f);
 check("peak_reserved_across_worlds",!memory.peakReady(160)&&memory.peakReady(208));
 const auto mid=conductor.sample(115);PhraseConductor resumed;resumed.resume(8,mid,115);
 const auto restored=resumed.sample(115);check("network_return_preserves_current_pose",std::abs(restored.blend-mid.blend)<.00001f&&restored.from==mid.from&&restored.to==mid.to&&restored.gain==mid.gain);
 // Sparse updates cannot miss a boundary simply because no frame landed in a narrow window.
 PhraseConductor sparse;sparse.begin(10,4,0);PhraseMemory sparseMemory; e={};e.sectionAge=100;
 for(float beat:{7.8f,16.3f,24.2f,32.4f})sparse.update(beat,e,sparseMemory);
 check("missed_boundary_frame_still_answers",sparse.sample(32.4f).action==float(PhraseAction::Answer));
 // A short build/quiet fill never gets sufficient sustained evidence at the boundary.
 PhraseConductor clean,fill;PhraseMemory cleanMemory,fillMemory;clean.begin(12,20,0);fill.begin(12,20,0);
 bool same=true,bounded=true;float maxStep=0;PhraseFrame previous;
 for(int frame=0;frame<256*60;++frame){float beat=frame/60.f;PhraseEvidence steady;steady.sectionAge=beat;auto fleeting=steady;
  if(std::fmod(beat,8.f)<1){fleeting.section=frame%2?1:3;fleeting.sectionAge=.5f;fleeting.trend=.2f;}
  clean.update(beat,steady,cleanMemory);fill.update(beat,fleeting,fillMemory);auto a=clean.sample(beat),b=fill.sample(beat);
  same&=a.from==b.from&&a.to==b.to&&a.blend==b.blend;bounded&=ValidPhraseFrame(a);
  if(frame&&a.from==previous.from&&a.to==previous.to)maxStep=std::max(maxStep,std::abs(a.blend-previous.blend));previous=a;
 }
 check("short_fills_do_not_replace_program",same);check("smooth_bounded_frame_state",bounded&&maxStep<.005f);
 IdentitySpacing identity;int kind=-1;check("identity_waits_for_establishment",!identity.take(0,0,true,true,kind));
 check("first_identity_is_mark",identity.take(16,16,true,true,kind)&&kind==0);
 check("scene_change_does_not_reset_identity_spacing",!identity.take(48,16,true,true,kind));
 check("identity_deferred_when_busy",!identity.take(80,48,true,false,kind));
 check("next_identity_is_name",identity.take(96,64,true,true,kind)&&kind==1);
 bool everyFamily=true;for(int scene:{6,8,10,12,19,18,22,23}){PhraseMemory setMemory;PhraseConductor set;
  std::array<bool,6>seen{};for(int song=0;song<20;++song){float start=song*256.f;set.begin(scene,(song*13)%64,start);
   for(int bar=0;bar<32;++bar){PhraseEvidence songEvidence;songEvidence.section=bar<8?4:bar<12?1:bar<16?2:bar<24?4:3;songEvidence.revision=song*5+bar/4;songEvidence.sectionAge=16;songEvidence.trend=songEvidence.section==1?.06f:0;
    float beat=start+bar*8;set.update(beat,songEvidence,setMemory);seen[int(set.sample(beat).to)]=true;}}
  everyFamily&=std::count(seen.begin(),seen.end(),true)==6;
 }
 check("twenty_simulated_songs_visit_all_six_programs",everyFamily);
 PhraseFrame invalid;invalid.to=std::numeric_limits<float>::quiet_NaN();check("nonfinite_network_program_rejected",!ValidPhraseFrame(invalid));
 invalid={};invalid.from=2.5f;check("fractional_program_rejected",!ValidPhraseFrame(invalid));
 check("banks_have_native_bridge",NativeBridge(8,10)&&NativeBridge(10,8)&&!NativeBridge(8,18));
 bool expanded=true;for(int scene=24;scene<32;++scene){PhraseMemory m;PhraseConductor c;std::array<bool,MaxPhrasePrograms> seen{};
  for(int song=0;song<20;++song){float start=song*256.f;c.begin(scene,(song*13)%64,start);for(int bar=0;bar<32;++bar){PhraseEvidence e;e.sectionAge=16;e.section=bar<16?4:bar<24?2:3;e.revision=song*4+bar/8;float beat=start+bar*8;c.update(beat,e,m);auto f=c.sample(beat);expanded&=ValidPhraseFrame(f)&&f.to<ProgramCount(scene);seen[int(f.to)]=true;}}
  expanded&=std::count(seen.begin(),seen.end(),true)==10;
 }
 check("new_scenes_visit_all_ten_programs",expanded);
 check("zero_key_does_not_select_unbound_scene",SceneForKey(0)==-1);
 invalid={};invalid.to=10;check("out_of_catalog_program_rejected",!ValidPhraseFrame(invalid));
 check("fine_to_broad_grids_use_material_transport",!NativeBridge(11,24)&&!NativeBridge(24,11)&&!NativeBridge(11,25)&&!NativeBridge(25,11));
 check("new_geometry_bridges",NativeBridge(24,3)&&NativeBridge(25,24)&&NativeBridge(26,31)&&!NativeBridge(25,26));
 bool poses=true;double maximumSpeed=0;for(int world=26;world<32;++world)for(int mode=0;mode<10;++mode)for(int id=0;id<20;++id)for(int frame=0;frame<1920;++frame){float beat=frame/60.f;auto a=rarestage::play::playPose(float(world),float(mode),float(id),beat),b=rarestage::play::playPose(float(world),float(mode),float(id),beat+1.f/60);
  poses&=std::isfinite(a.center.x)&&std::isfinite(a.center.y)&&std::isfinite(a.angle)&&std::abs(a.center.x)<1.4f&&std::abs(a.center.y)<1.4f&&a.size.x>0&&a.size.y>0;
  maximumSpeed=std::max(maximumSpeed,double(std::hypot(b.center.x-a.center.x,b.center.y-a.center.y)*60));
 }
 check("all_object_programs_have_finite_bounded_motion",poses&&maximumSpeed<1.5);
 bool everyProgramMoves=true;
 for(int world=26;world<32;++world)for(int mode=0;mode<10;++mode){double travel=0;
  for(int id=0;id<20;++id)for(int frame=0;frame<1920;++frame){float beat=frame/60.f;auto a=rarestage::play::playPose(float(world),float(mode),float(id),beat),b=rarestage::play::playPose(float(world),float(mode),float(id),beat+1.f/60);
   travel+=std::hypot(b.center.x-a.center.x,b.center.y-a.center.y)+std::abs(b.angle-a.angle)*std::hypot(a.size.x,a.size.y)+std::hypot(b.size.x-a.size.x,b.size.y-a.size.y);
  }
  if(travel/20<=.10)std::cerr<<"Insufficient geometry travel: scene "<<world<<", program "<<mode<<", per piece "<<travel/20<<'\n';
  everyProgramMoves&=travel/20>.10;
 }
 check("every_object_program_moves_geometry_without_audio_or_lighting",everyProgramMoves);
 bool vocabulary=true;const int motif[]{0,0,1,8,1,9,0,8,2,2,3,10,3,11,2,10};
 for(int seed=0;seed<64;++seed)for(int step=0;step<256;++step)vocabulary&=rarestage::patterns::authoredMotif(float(step),float(seed))==float((seed+motif[step%16])%16);
 check("shared_wall_motif_sequence_preserved",vocabulary);
 bool fieldBounds=true;float seamDelta=0;
 for(int seed=0;seed<16;++seed)for(int id=0;id<20;++id)for(int step=0;step<128;++step){
  rarestage::patterns::float2 address{float(id%5-2),float(id/5-2)};float beat=step*8.f;
  auto a=rarestage::patterns::sample(float(seed),address,beat+.001f),b=rarestage::patterns::sample(float(seed),address,std::max(0.f,beat-.001f));
  fieldBounds&=std::isfinite(a.row)&&std::abs(a.row)<=2.21f&&std::abs(a.column)<=.86f&&a.signal>=0&&a.signal<=1;
  seamDelta=std::max(seamDelta,std::max(std::abs(a.row-b.row),std::max(std::abs(a.column-b.column),std::abs(a.signal-b.signal))));
 }
 check("shared_fields_remain_bounded_at_motif_joins",fieldBounds&&seamDelta<.01f);
 bool inheritedBounds=true;double inheritedSpeed=0,travelDifference=0;
 for(int world=26;world<32;++world)for(int mode=0;mode<10;++mode)for(int seed:{0,7,10,15})for(int id=0;id<20;++id)for(int frame=0;frame<480;++frame){
  float beat=frame/30.f;PhraseFrame state{float(mode),float(mode),1,1,1,1,0,0};
  auto a=rarestage::play::program(world,float(seed),state,id,beat),b=rarestage::play::program(world,float(seed),state,id,beat+1.f/30);
  auto base=rarestage::play::playPose(float(world),float(mode),float(id),beat);
  inheritedBounds&=std::isfinite(a.center.x)&&std::isfinite(a.center.y)&&std::abs(a.center.x)<1.4f&&std::abs(a.center.y)<1.4f&&a.signal>=0&&a.signal<=1;
  inheritedSpeed=std::max(inheritedSpeed,double(std::hypot(b.center.x-a.center.x,b.center.y-a.center.y)*30));
  travelDifference+=std::hypot(a.center.x-base.center.x,a.center.y-base.center.y);
 }
 check("wall_patterns_change_geometry_with_bounded_motion",inheritedBounds&&inheritedSpeed<1.8&&travelDifference>100);
 bool linkedRows=true;float rowTravel=0;
 for(int id=0;id<10;++id){auto base=rarestage::play::playPose(28,0,float(id),2);auto moved=rarestage::play::inheritStagePattern(base,28,id,0,2);float offset=moved.center.x-base.center.x;if(id==0)rowTravel=offset;linkedRows&=std::abs(offset-rowTravel)<.00001f;}
 check("domino_row_inherits_one_coordinated_wall_stroke",linkedRows&&std::abs(rowTravel)>.02f);
 auto longRun=rarestage::play::playPose(27,1,19,10000);check("long_sets_do_not_accumulate_blend_spins",std::abs(longRun.angle)<7);
 invalid={9,9,1,1,1,1,0,0};check("program_range_matches_network_scene",ValidPhraseForScene(invalid,24)&&!ValidPhraseForScene(invalid,8));
 return passed?0:1;
}

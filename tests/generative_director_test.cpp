#include "generative_director.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace rarestage::show;
static int failures=0;
static void check(const char*name,bool ok){if(!ok){std::printf("FAIL %s\n",name);++failures;}}

// Plays a 128 BPM set through the policy: 10 ms of context per step, sections in a fixed arc.
struct Run{
 GenerativeDirector d;float seconds{0},beats{0};int section{0};bool ready{true},automatic{true},frozen{false},held{false},enabled{true};
 std::vector<std::pair<float,GenDecision>> events;
 GenDecision step(float dt,bool take=false,bool rel=false,bool interrupt=false){
  seconds+=dt;beats+=dt*128.f/60.f;GenContext c;c.seconds=seconds;c.beats=beats;c.section=section;c.enabled=enabled;c.automatic=automatic;c.frozen=frozen;c.held=held;c.candidateReady=ready;
  c.takeRequested=take;c.releaseRequested=rel;c.operatorInterrupt=interrupt;auto r=d.update(c);if(r.action!=GenAction::None)events.push_back({seconds,r});return r;}
 void advance(float s){for(float t=0;t<s;t+=.01f)step(.01f);}
};

int main(){
 // Nothing happens while no candidate exists.
 {Run r;r.ready=false;r.section=2;r.advance(60.f);check("no_candidate_no_activation",r.events.empty());}
 // The first visit is a debut on the first phrase boundary, even in a quiet section; after that quiet means quiet.
 {Run r;r.section=0;r.advance(30.f);check("debut_arrives_in_a_quiet_section",!r.events.empty()&&std::string(r.events.front().second.reason)=="debut");
  if(!r.events.empty()){const float beat=r.events.front().first*128.f/60.f;const float phase=std::fmod(beat,GenerativeDirector::PhraseBeats);check("debut_is_on_a_phrase_boundary",phase<.6f||phase>GenerativeDirector::PhraseBeats-.6f);}}
 {Run r;r.section=0;r.advance(30.f);const size_t first=r.events.size();r.advance(400.f);check("quiet_open_does_not_activate_again",r.events.size()==first+1&&r.events.back().second.action==GenAction::Release);}

 // A drop with a ready candidate takes the stage at the onset, with a short fade.
 {Run r;r.ready=false;r.section=0;r.advance(10.f);r.ready=true;r.section=2;const auto d=r.step(.01f);
  check("drop_onset_activates",d.action==GenAction::Activate&&d.fadeSeconds<=.35f&&r.d.active());}

 // The same drop without a candidate is simply missed: it does not activate late.
 {Run r;r.ready=false;r.section=1;r.advance(10.f);r.section=2;r.step(.01f);r.ready=true;r.advance(1.f);
  check("missed_drop_is_not_made_up_mid_phrase",r.events.empty()||r.events.front().first>12.f);}

 // It arrives on a build or drive phrase boundary with a slow fade, never mid-phrase.
 {Run r;r.section=4;r.advance(60.f);check("activates_on_a_phrase_boundary",!r.events.empty()&&r.events.front().second.fadeSeconds>=2.f);
  if(!r.events.empty()){const float beat=r.events.front().first*128.f/60.f;const float phase=std::fmod(beat,GenerativeDirector::PhraseBeats);check("arrival_is_on_the_boundary",phase<.6f||phase>GenerativeDirector::PhraseBeats-.6f);}}

 // Lifetime: stays, then leaves on a boundary after about 96 beats.
 {Run r;r.section=2;r.step(.01f);r.section=2;r.advance(1.f);
  const bool began=r.d.active();float releasedAt=-1;
  for(int i=0;i<12000&&releasedAt<0;++i){const auto d=r.step(.01f);if(d.action==GenAction::Release)releasedAt=r.beats;}
  check("lifetime_ends_the_visit",began&&releasedAt>0&&releasedAt>=GenerativeDirector::LifeBeats);
  const float phase=std::fmod(releasedAt,GenerativeDirector::PhraseBeats);check("release_is_on_a_boundary",phase<.6f||phase>GenerativeDirector::PhraseBeats-.6f);}

 // Cooldown: it cannot come straight back.
 {Run r;r.section=0;r.advance(5.f);r.section=2;r.step(.01f);float releasedBeat=-1;
  for(int i=0;i<12000&&releasedBeat<0;++i){const auto d=r.step(.01f);if(d.action==GenAction::Release)releasedBeat=r.beats;}
  r.section=0;r.advance(2.f);r.section=2;const auto early=r.step(.01f);check("cooldown_blocks_a_quick_return",early.action==GenAction::None);
  r.section=0;r.advance(60.f);r.section=2;const auto later=r.step(.01f);check("cooldown_expires",later.action==GenAction::Activate&&r.beats-releasedBeat>=GenerativeDirector::CooldownBeats);}

 // Operator authority.
 {Run r;r.section=2;r.step(.01f);check("active_before_interrupt",r.d.active());const auto d=r.step(.01f,false,false,true);check("operator_action_releases_at_once",d.action==GenAction::Release&&d.fadeSeconds<=.6f&&!r.d.active());}
 {Run r;r.section=2;r.step(.01f);const auto d=r.step(.01f,false,true);check("release_button_releases",d.action==GenAction::Release);}
 {Run r;r.frozen=true;r.section=2;r.advance(30.f);check("frozen_show_never_activates",r.events.empty());}
 {Run r;r.held=true;r.section=2;r.advance(30.f);check("held_program_never_activates",r.events.empty());}
 {Run r;r.automatic=false;r.section=2;r.advance(30.f);check("manual_mode_never_activates",r.events.empty());}
 {Run r;r.enabled=false;r.section=2;r.advance(30.f);check("disabled_never_activates",r.events.empty());}
 {Run r;r.section=2;r.step(.01f);r.enabled=false;const auto d=r.step(.01f);check("disabling_releases",d.action==GenAction::Release);}
 {Run r;r.ready=false;r.section=0;const auto d=r.step(.01f,true);check("take_without_candidate_does_nothing",d.action==GenAction::None);
  r.ready=true;const auto t=r.step(.01f,true);check("take_works_in_a_quiet_section",t.action==GenAction::Activate&&t.fadeSeconds>=1.f);}
 {Run r;r.section=0;r.advance(100.f);const auto t=r.step(.01f,true);check("take_ignores_cooldown_but_needs_a_candidate",t.action==GenAction::Activate);}

 // A tempo reset in the middle of a visit does not release it or fire a phantom boundary.
 {Run r;r.section=2;r.step(.01f);r.advance(5.f);r.beats=2.f;r.advance(1.f);check("tempo_reset_keeps_the_visit",r.d.active());}

 // Fade is continuous when reversed.
 {Fade f;f.set(1.f,2.f);for(int i=0;i<60;++i)f.update(1.f/60);const float mid=f.raw();f.set(0.f,2.f);f.update(1.f/60);
  check("fade_reverses_from_where_it_is",std::abs(f.raw()-(mid-1.f/120.f))<1e-4f&&f.raw()>0.f&&f.raw()<mid);
  Fade g;g.set(1.f,1.f);float maxStep=0,last=g.value();for(int i=0;i<120;++i){g.update(1.f/60);maxStep=std::max(maxStep,std::abs(g.value()-last));last=g.value();}
  check("fade_reaches_target_smoothly",g.settled()&&g.value()==1.f&&maxStep<.06f);}

 // An hour-long set: the guest appears now and then, never flip-flops, and always leaves cleanly.
 {Run r;int activations=0,releases=0;float lastEvent=-1000;float minGap=1e9f;
  const int arc[8]{0,4,4,1,2,4,3,4};
  for(int tick=0;tick<360000;++tick){r.seconds+=.01f;r.beats+=.01f*128.f/60.f;r.section=arc[int(r.beats/32.f)%8];
   GenContext c;c.seconds=r.seconds;c.beats=r.beats;c.section=r.section;c.enabled=true;c.candidateReady=true;const auto d=r.d.update(c);
   if(d.action==GenAction::Activate){++activations;if(lastEvent>0)minGap=std::min(minGap,r.seconds-lastEvent);lastEvent=r.seconds;}
   if(d.action==GenAction::Release){++releases;lastEvent=r.seconds;}}
  check("hour_set_has_regular_visits",activations>=10&&activations<=40);check("every_visit_ends",releases==activations||releases==activations-1);
  check("hour_set_never_flip_flops",minGap>=GenerativeDirector::CooldownSeconds);
  std::printf("hour set: %d visits, shortest gap between a release and the next arrival %.0f s\n",activations,double(minGap));}

 if(failures==0)std::printf("generative director: all checks passed\n");
 return failures?1:0;
}

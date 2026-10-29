#pragma once
#include <algorithm>
#include <cmath>
// Decides when a qualified generated world takes the stage and when it gives it back. Pure logic with no GPU
// or window dependency, so the whole policy is covered by offline tests.
//
// Policy, in words:
//  - A generated world is a guest over the Director's show. The Director's own scene logic keeps running
//    underneath, so releasing a world always returns to a coherent show.
//  - It arrives on a musical event: the onset of a drop (a quick fade, nearly a cut) or a phrase boundary
//    during a build or a sustained drive (a slow fade). It never arrives mid-phrase.
//  - It is never activated if no qualified candidate is ready. A missed drop is simply missed; the next
//    boundary is the next chance.
//  - It stays about 96 beats (a few phrases) and leaves only on a phrase boundary.
//  - After it leaves there is a cooldown in beats and seconds, so the show cannot flip-flop.
//  - Any operator action ends it at once, and nothing starts while the operator holds the show.
//  - The first visit after the feature is switched on is a debut: it arrives at the first phrase boundary even in a
//    quiet section, so enabling generated worlds shows something without waiting for a drop.
namespace rarestage::show {

// Reversible 0..1 ramp. The raw value is continuous when the target flips mid-fade, so a world that is
// interrupted while arriving leaves from where it is rather than snapping.
class Fade{
 float v_{0},target_{0},seconds_{1};
public:
 void set(float target,float seconds){target_=std::clamp(target,0.f,1.f);seconds_=std::max(seconds,.05f);}
 void snap(float v){v_=target_=std::clamp(v,0.f,1.f);}
 void update(float dt){const float step=std::max(dt,0.f)/seconds_;v_+=std::clamp(target_-v_,-step,step);}
 float raw()const{return v_;}
 float value()const{return v_*v_*(3.f-2.f*v_);}
 float target()const{return target_;}
 bool settled()const{return v_==target_;}
};

enum class GenAction{None,Activate,Release};
struct GenContext{
 float seconds{0},beats{0};int section{0};
 bool enabled{false};          // the feature is on and nothing else (preview, sync group) has claimed the output
 bool automatic{true},frozen{false},blackout{false},held{false};
 bool candidateReady{false};
 bool takeRequested{false},releaseRequested{false},operatorInterrupt{false};
};
struct GenDecision{GenAction action{GenAction::None};float fadeSeconds{0};const char*reason{""};};

class GenerativeDirector{
public:
 static constexpr float LifeBeats=96.f,CooldownBeats=64.f,CooldownSeconds=40.f,PhraseBeats=16.f;
 bool active()const{return active_;}
 float ageBeats(float beats)const{return std::max(0.f,beats-activatedBeat_);}
 GenDecision update(const GenContext&c){
  const long phrase=long(std::floor(c.beats/PhraseBeats));
  // A tempo reset or a jump backwards is not a boundary; just re-anchor.
  const bool boundary=havePhrase_&&phrase>lastPhrase_&&phrase-lastPhrase_<=2;
  const bool dropOnset=c.section==2&&prevSection_!=2;
  if(c.beats<lastBeats_-1.f)activatedBeat_=std::min(activatedBeat_,c.beats);
  lastPhrase_=phrase;havePhrase_=true;prevSection_=c.section;lastBeats_=c.beats;
  GenDecision d;
  if(active_){
   if(!c.enabled||c.blackout)return release(c,.6f,"disabled");
   if(c.releaseRequested)return release(c,1.f,"released by operator");
   if(c.operatorInterrupt||c.frozen)return release(c,.5f,"operator took the show");
   const float age=ageBeats(c.beats);
   if(boundary&&age>=LifeBeats)return release(c,2.f,"lifetime reached");
   return d;
  }
  if(!c.enabled||c.blackout)return d;
  if(c.takeRequested)return c.candidateReady?activate(c,1.2f,"taken by operator"):d;
  if(!debutDone_&&c.automatic&&!c.frozen&&!c.held&&c.candidateReady&&boundary)return activate(c,2.5f,"debut");
  if(!c.automatic||c.frozen||c.held||!c.candidateReady)return d;
  const bool cool=c.beats-lastReleaseBeat_>=CooldownBeats&&c.seconds-lastReleaseSeconds_>=CooldownSeconds;
  if(!cool)return d;
  if(dropOnset)return activate(c,.3f,"drop");
  if(boundary&&(c.section==1||c.section==4))return activate(c,2.5f,c.section==1?"build phrase":"drive phrase");
  return d;
 }
private:
 bool active_{false},havePhrase_{false},debutDone_{false};float activatedBeat_{0},lastReleaseBeat_{-1e9f},lastReleaseSeconds_{-1e9f},lastBeats_{0};long lastPhrase_{0};int prevSection_{-1};
 GenDecision activate(const GenContext&c,float fade,const char*why){active_=true;debutDone_=true;activatedBeat_=c.beats;return {GenAction::Activate,fade,why};}
 GenDecision release(const GenContext&c,float fade,const char*why){active_=false;lastReleaseBeat_=c.beats;lastReleaseSeconds_=c.seconds;return {GenAction::Release,fade,why};}
};
}

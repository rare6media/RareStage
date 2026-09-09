#pragma once
#include "scene_programs.h"
#include <cmath>
#include <string>

namespace rarestage::show {
enum class PhraseAction { Establish, Repeat, Answer, Develop, Resolve };
inline constexpr const wchar_t* PhraseActions[]{L"ESTABLISH",L"HOLD THE GROOVE",L"ANSWER",L"DEVELOP",L"RESOLVE"};
inline int PhraseFamily(int scene) {
 if(PlayScene(scene))return 6+scene-24;
 switch(scene){case 6:return 0;case 8:return 1;case 10:return 2;case 12:return 3;case 19:return 4;case 18:case 22:case 23:return 5;default:return -1;}
}
inline const wchar_t* PhraseName(int scene,int mode) {
 if(PlayScene(scene))return PlayProgramNames[scene-24][std::clamp(mode,0,9)];
 static constexpr const wchar_t* names[6][6]={
  {L"COUNTERFLOW",L"CRYSTAL BRAID",L"OPEN STREAMS",L"LINKED TILT",L"PRISM CROWN",L"RETURNING WAVE"},
  {L"SCISSOR",L"INTERLOCK",L"TRAVELLING CUT",L"MIRRORED FAN",L"BLADE SWEEP",L"FOLDED ANSWER"},
  {L"TRAVELLING FINS",L"MIRRORED ARCH",L"BRAID",L"OPPOSING SWEEP",L"FIN FAN",L"RETURNING ARCH"},
  {L"LINKED SQUARES",L"ROW TURN",L"COLUMN TURN",L"DIAGONAL LOCK",L"COUNTER ROTATION",L"NESTED RESOLVE"},
  {L"THREE ROTORS",L"WING ANSWER",L"ORBITING CROWN",L"TIER EXCHANGE",L"OPEN ROTORS",L"RETURNING ORBIT"},
  {L"TRAVELLING FOLDS",L"OPPOSING WAVES",L"RIBBON FAN",L"MIRRORED FOLDS",L"CROSSING CURRENT",L"LONG RESOLVE"}};
 const int family=PhraseFamily(scene);return family>=0?names[family][std::clamp(mode,0,5)]:L"AUTHORED PATTERN";
}
inline float PhraseEase(float u){u=std::clamp(u,0.f,1.f);return std::clamp(u*u*u*(u*(u*6-15)+10),0.f,1.f);}
// Two aligned float4s also travel to the shader and network follower. The clock
// itself is never replaced: these values shape a gesture running on that clock.
struct PhraseFrame {
 float from{},to{},blend{1},enabled{};
 float gain{1},spread{1},answer{},action{};
};
static_assert(sizeof(PhraseFrame)==32);
inline bool ValidPhraseFrame(const PhraseFrame& f){
 return std::isfinite(f.from)&&f.from>=0&&f.from<MaxPhrasePrograms&&f.from==std::floor(f.from)&&
 std::isfinite(f.to)&&f.to>=0&&f.to<MaxPhrasePrograms&&f.to==std::floor(f.to)&&
 std::isfinite(f.blend)&&f.blend>=0&&f.blend<=1&&(f.enabled==0||f.enabled==1)&&
 std::isfinite(f.gain)&&f.gain>=.7f&&f.gain<=1.2f&&std::isfinite(f.spread)&&f.spread>=.9f&&f.spread<=1.06f&&
 std::isfinite(f.answer)&&std::abs(f.answer)<=.22f&&f.action>=0&&f.action<=4&&f.action==std::floor(f.action);
}
inline bool ValidPhraseForScene(const PhraseFrame& f,int scene){return ValidPhraseFrame(f)&&(!f.enabled||(f.from<ProgramCount(scene)&&f.to<ProgramCount(scene)));}
struct PhraseEvidence {
 int section{4},revision{};
 float energy{.4f},confidence{1},sectionAge{},trend{};
 bool safe{true},held{},departing{};
};
class PhraseMemory {
 std::array<std::array<unsigned,MaxPhrasePrograms*5>,SceneCount> uses_{};
 std::array<std::array<float,MaxPhrasePrograms>,SceneCount> last_{};
 float lastPeak_{-1000};unsigned sequence_{};
public:
 PhraseMemory(){for(auto& scene:last_)scene.fill(-1000);}
 bool peakReady(float beat)const{return beat-lastPeak_>=96;}
 void peak(float beat){lastPeak_=beat;}
 int choose(int scene,int current,PhraseAction action,float beat){
  const int family=PhraseFamily(scene);int chosen=current;float best=-1e9f;
  for(int mode=0;mode<ProgramCount(scene);++mode){if(mode==current)continue;
   float score=-float(uses_[scene][mode*5+int(action)])*.6f-std::max(0.f,48-(beat-last_[scene][mode]))*.05f;
   // Family-specific answers retain a related silhouette; development explores.
   const int related=(current+(family==3||family==4?2:1))%ProgramCount(scene);
   if(action==PhraseAction::Answer&&mode==related)score+=.8f;
   if(action==PhraseAction::Resolve&&(mode==0||mode==ProgramCount(scene)-1))score+=.65f;
   score+=float((mode*13+sequence_*7)%31)*.003f;
   if(score>best){best=score;chosen=mode;}
  }
  ++uses_[scene][chosen*5+int(action)];last_[scene][chosen]=beat;++sequence_;return chosen;
 }
};
class PhraseConductor {
 int scene_{-1},boundary_{-1},repeat_{},turn_{},lastSectionRevision_{-1};
 float began_{},changed_{-100},duration_{8};PhraseFrame state_{};
 std::array<float,3> source_{{1,1,0}},target_{{1,1,0}};
public:
 int scene()const{return scene_;}PhraseAction action()const{return PhraseAction(int(state_.action));}
 void begin(int scene,int seed,float beat){
  scene_=scene;boundary_=int(std::floor(beat/8));repeat_=turn_=0;lastSectionRevision_=-1;began_=beat;changed_=beat-8;
  state_={float((seed/4)%ProgramCount(scene)),float((seed/4)%ProgramCount(scene)),1,PhraseFamily(scene)>=0?1.f:0.f,1,1,0,0};source_=target_={1,1,0};
 }
 PhraseFrame sample(float beat)const{
  PhraseFrame f=state_;f.blend=PhraseEase((beat-changed_)/duration_);
  f.gain=source_[0]+(target_[0]-source_[0])*f.blend;f.spread=source_[1]+(target_[1]-source_[1])*f.blend;f.answer=source_[2]+(target_[2]-source_[2])*f.blend;return f;
 }
 void update(float beat,const PhraseEvidence& e,PhraseMemory& memory){
  const int boundary=int(std::floor(beat/8));
  if(boundary==boundary_)return;boundary_=boundary;
  if(!state_.enabled||e.held||!e.safe||beat-began_<16||beat-changed_<duration_)return;
  auto next=PhraseAction::Repeat;bool peak=false;
  const bool confirmed=e.confidence>=.30f&&e.sectionAge>=4;
  if(e.departing)next=PhraseAction::Resolve;
  else if(e.section==2&&e.revision!=lastSectionRevision_&&memory.peakReady(beat)){next=PhraseAction::Develop;peak=true;}
  else if(confirmed&&(e.section==0||e.section==3)){next=action()==PhraseAction::Resolve?PhraseAction::Repeat:PhraseAction::Resolve;}
  else if(confirmed&&e.section==1&&e.trend>.018f&&action()!=PhraseAction::Develop)next=PhraseAction::Develop;
  else if(repeat_<2)next=PhraseAction::Repeat;
  else {next=turn_%3==0?PhraseAction::Answer:turn_%3==1?PhraseAction::Develop:PhraseAction::Resolve;++turn_;}
  // A section can authorize one peak, not one peak every eight beats.
  if(e.section==2)lastSectionRevision_=e.revision;
  if(next==PhraseAction::Repeat){++repeat_;state_.action=float(next);return;}
  repeat_=0;const auto now=sample(beat);source_={now.gain,now.spread,now.answer};
  state_.from=state_.to;state_.to=float(memory.choose(scene_,int(state_.to),next,beat));state_.action=float(next);changed_=beat;
  target_=next==PhraseAction::Answer?std::array<float,3>{1,1,turn_%2?.18f:-.18f}:
          next==PhraseAction::Resolve?std::array<float,3>{.80f,.96f,0}:
          std::array<float,3>{peak?1.16f:1.07f,peak?1.04f:1.02f,0};
  if(peak)memory.peak(beat);
 }
 // On leaving network mode, finish the received blend before making local choices.
 void resume(int scene,const PhraseFrame& received,float beat){
  scene_=scene;state_=received;boundary_=int(std::floor(beat/8));began_=beat;repeat_=turn_=0;
  float lo=0,hi=1;for(int i=0;i<20;++i){float mid=(lo+hi)*.5f;if(PhraseEase(mid)<received.blend)lo=mid;else hi=mid;}
  changed_=beat-(lo+hi)*.5f*duration_;source_=target_={received.gain,received.spread,received.answer};
 }
};
class IdentitySpacing {
 float last_{-1000};int nextKind_{};
public:
 bool take(float beat,float sceneAge,bool enabled,bool safe,int&kind){
  if(!enabled||!safe||sceneAge<16||beat-last_<64)return false;
  last_=beat;kind=nextKind_;nextKind_=1-nextKind_;return true;
 }
};
}

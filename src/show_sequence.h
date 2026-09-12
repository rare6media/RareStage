#pragma once
#include "phrase_conductor.h"
namespace rarestage::show {
enum class Gesture { Travel, Counterflow, Turn, Fan, Gather, Fold };
inline constexpr const wchar_t* GestureNames[]{L"TRAVELLING",L"COUNTERFLOW",L"TURNING",L"FANNING",L"ASSEMBLING",L"FOLDING"};
inline constexpr const wchar_t* SequenceRoles[]{L"ESTABLISH",L"CARRY",L"DEVELOP",L"PAYOFF",L"RELEASE"};
inline Gesture ProgramGesture(int scene,int mode){
 static constexpr int graphic[3][10]={{0,4,1,0,5,3,1,2,4,4},{0,3,1,0,1,5,1,4,3,4},{2,4,1,2,0,3,2,2,4,4}};
 if(GraphicScene(scene))return Gesture(graphic[scene-35][std::clamp(mode,0,9)]);
 static constexpr int play[11][10]={
  {0,1,1,1,4,4,0,1,2,4},{0,0,1,1,0,3,1,1,0,4},
  {2,0,1,0,2,2,4,1,4,0},{3,3,1,2,2,3,0,3,4,4},
  {0,0,2,0,1,0,1,4,3,4},{0,0,1,0,3,0,1,5,3,5},
  {0,0,1,0,1,1,3,4,2,4},{4,3,4,4,2,4,0,4,2,4},
  {2,1,1,2,2,2,0,4,2,4},{0,1,0,0,1,1,0,0,2,4},
  {5,1,3,3,2,0,3,5,2,4}};
 if(PlayScene(scene))return Gesture(play[scene-24][std::clamp(mode,0,9)]);
 static constexpr int ensembles[6][6]={{1,0,3,1,3,0},{1,1,0,3,0,5},{0,3,1,1,3,5},{2,0,0,4,1,4},{2,3,2,1,3,2},{5,1,3,5,1,4}};
 int family=PhraseFamily(scene);if(family>=0)return Gesture(ensembles[family][std::clamp(mode,0,5)]);
 return Scenes[scene].motion==3?Gesture::Turn:Scenes[scene].motion==1?Gesture::Fold:Gesture::Travel;
}
inline int RelatedProgram(int scene,Gesture gesture,int variation=0){
 int selected=0,count=0;for(int m=0;m<ProgramCount(scene);++m)if(ProgramGesture(scene,m)==gesture)++count;
 if(!count)return variation%ProgramCount(scene);int index=variation%count;
 for(int m=0;m<ProgramCount(scene);++m)if(ProgramGesture(scene,m)==gesture&&index--==0){selected=m;break;}return selected;
}
// A sequence survives scene changes. Only committed visits and confirmed musical
// evidence advance it; forecasts cannot spend a payoff or rewrite its motif.
class ShowSequence {
 Gesture gesture_{Gesture::Travel};int visits_{},chapter_{},role_{},dropRevision_{-1};
 float lastPayoff_{-1000};bool primed_{};
public:
 Gesture gesture()const{return gesture_;}int role()const{return role_;}int chapter()const{return chapter_;}
 void reset(int scene,int program){gesture_=ProgramGesture(scene,program);visits_=0;role_=0;primed_=true;}
 void visit(int scene,int program,int section,int revision,float confidence,float age,float beat,bool directed){
  if(!directed||!primed_){reset(scene,program);return;}
  ++visits_;
  const bool confirmed=confidence>=.30f&&age>=1;
  if(confirmed&&section==2&&revision!=dropRevision_&&beat-lastPayoff_>=96){role_=3;lastPayoff_=beat;dropRevision_=revision;}
  else if(confirmed&&(section==0||section==3))role_=4;
  else role_=visits_<2?1:visits_<4?2:4;
  if(visits_>=5){++chapter_;visits_=0;role_=0;gesture_=Gesture((int(gesture_)+1+(chapter_%2))%6);}
 }
 int program(int scene)const{return role_==3||role_==4?RelatedProgram(scene,Gesture::Gather,chapter_):RelatedProgram(scene,gesture_,role_==2?chapter_+1:chapter_);}
 float affinity(int scene)const{for(int m=0;m<ProgramCount(scene);++m)if(ProgramGesture(scene,m)==gesture_)return .22f;return -.08f;}
 int entrance(int variant)const{static constexpr int styles[6][2]={{0,4},{4,7},{2,6},{1,5},{3,7},{5,1}};return styles[int(gesture_)][variant%2];}
};
}

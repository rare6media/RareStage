#pragma once
#include <array>
#include "scene_catalog.h"
#include <algorithm>
namespace rarestage::show {
class SetMemory {
 std::array<std::array<unsigned,64>,SceneCount> programs_{};
 std::array<std::array<unsigned,8>,SceneCount> palettes_{};
 std::array<std::array<unsigned,64>,2> identity_{};
 std::array<int,SceneCount> lastProgram_{};
 unsigned sequence_{};
public:
 SetMemory(){lastProgram_.fill(-1);}
 int program(int scene,int section){
  const int preferred=section==2?8:section==1?2:section==3?4:0;
  int chosen=0;float best=-1e9f;
  for(int p=0;p<64;++p){
   float score=-float(programs_[scene][p])*4.f+((p%16)/4==preferred/4?.35f:0.f);
   score+=float((p*13+sequence_*7)%67)*.001f;
   if(lastProgram_[scene]>=0&&p/16==lastProgram_[scene]/16)score-=.55f;
   if(score>best){best=score;chosen=p;}
  }
  ++programs_[scene][chosen];lastProgram_[scene]=chosen;++sequence_;return chosen;
 }
 int palette(int scene,int preferred,int current){
  int chosen=preferred;float best=-1e9f;
  for(int p=0;p<8;++p){float score=-float(palettes_[scene][p])*.8f+(p==preferred?.4f:0.f)-(p==current?.18f:0.f);if(score>best){best=score;chosen=p;}}
  ++palettes_[scene][chosen];return chosen;
 }
 int identity(int kind,int motion){
  int chosen=0;float best=-1e9f;
  for(int p=0;p<64;++p){int enter=p%8,leave=p/8;float score=-float(identity_[kind][p])*4.f+(enter!=leave?.5f:0.f)+float((p*17+sequence_*11)%71)*.001f;
   if((motion==0&&enter<2)||(motion==3&&enter==2)||(motion==1&&enter==6))score+=.12f;
   if(score>best){best=score;chosen=p;}}
  ++identity_[kind][chosen];++sequence_;return chosen;
 }
};
}

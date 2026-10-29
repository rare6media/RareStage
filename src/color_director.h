#pragma once
#include "palette_ring.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace rarestage::show {
// Owns the colour of the show. The position is a continuous coordinate on the palette ring;
// retargeting picks a new stop on phrase and act boundaries and a critically damped glide
// travels there, so colour never steps and never reverses when the target changes mid-glide.
class ColorDirector {
 static constexpr int History=7;
 std::array<int,History> recent_{};
 std::array<unsigned,PaletteCount> use_{};
 float position_{},velocity_{},target_{};unsigned sequence_{};float glideSeconds_{3.2f};
 // Act intent: how far to hop, and which side of the wheel is preferred.
 struct Intent {float hop,glide,warmth;};
 static Intent intentFor(int section){
  switch(section){
   case 0:return {4.f,4.2f,-.10f};  // open: a calm neighbour
   case 1:return {6.f,3.0f,0.f};    // build: a visible step away
   case 2:return {10.f,1.7f,.35f};  // drop: a hard contrast, warm and quick
   case 3:return {5.f,4.6f,-.35f};  // breakdown: slow and cool
   default:return {7.f,2.6f,.10f};  // drive
  }
 }
 // Warm stops sit around the red/gold end of the ring (0..7 and 21..23).
 static float warmthOf(int stop){return (stop<=7||stop>=21)?1.f:(stop>=11&&stop<=18)?-1.f:0.f;}
public:
 ColorDirector(){recent_.fill(-100);}
 void reset(float position){position_=target_=WrapPalette(position);velocity_=0;recent_.fill(-100);}
 void setTarget(float stop,float glideSeconds=3.2f){target_=WrapPalette(stop);glideSeconds_=std::clamp(glideSeconds,.4f,8.f);}
 float target()const{return target_;}
 // Chooses the next colour. Variety comes from three rules: stay well clear of the colours
 // used a moment ago, prefer stops seen least this set, and size the hop to the act.
 void retarget(int section,float energy){
  const Intent intent=intentFor(std::clamp(section,0,4));const int from=int(std::lround(target_))%PaletteCount;
  int best=from;float bestScore=-1e9f;
  for(int stop=0;stop<PaletteCount;++stop){
   const float hop=std::abs(PaletteDelta(float(from),float(stop)));
   if(hop<2.5f)continue;
   float score=-std::abs(hop-intent.hop*(.8f+.4f*std::clamp(energy,0.f,1.f)))*.55f;
   for(int age=0;age<History;++age)if(recent_[age]>=0)score-=std::max(0.f,5.f-float(PaletteDistance(stop,recent_[age])))*(.7f-age*.07f);
   score-=float(use_[stop])*.42f;score+=warmthOf(stop)*intent.warmth;
   // Golden-ratio jitter breaks ties without a visible pattern.
   score+=std::fmod(float(stop*37u+sequence_*61u),97.f)*.004f;
   if(score>bestScore){bestScore=score;best=stop;}
  }
  for(int i=History-1;i>0;--i)recent_[i]=recent_[i-1];recent_[0]=best;
  ++use_[best];++sequence_;setTarget(float(best),intent.glide);
 }
 // beat drives a slow breathing of the hue so a held colour still lives (about 5 degrees).
 void update(float dt){
  dt=std::clamp(dt,0.f,.1f);
  const float omega=2.f/glideSeconds_,x=omega*dt,decay=1.f/(1+x+.48f*x*x+.235f*x*x*x);
  float offset=PaletteDelta(target_,position_);
  const float temp=(velocity_+omega*offset)*dt;velocity_=(velocity_-omega*temp)*decay;offset=(offset+temp)*decay;
  position_=WrapPalette(target_+offset);
 }
 float position(float beat=0,float breathing=0)const{return WrapPalette(position_+std::sin(beat*.098175f)*.34f*breathing);}
 float velocity()const{return velocity_;}
 unsigned usage(int stop)const{return use_[size_t(std::clamp(stop,0,PaletteCount-1))];}
};
}

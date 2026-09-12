#pragma once
#include "phrase_conductor.h"
#include "stage_patterns_cpu.h"
#include <array>
#include <cmath>
namespace rarestage::play {
struct float2 {
 float x{},y{};float2()=default;float2(float v):x(v),y(v){}float2(float a,float b):x(a),y(b){}
 float2 operator+(float2 b)const{return {x+b.x,y+b.y};}float2 operator-(float2 b)const{return {x-b.x,y-b.y};}
 float2 operator*(float2 b)const{return {x*b.x,y*b.y};}
 float2 operator*(float b)const{return {x*b,y*b};}float2 operator/(float b)const{return {x/b,y/b};}
 float2&operator+=(float2 b){x+=b.x;y+=b.y;return *this;}float2&operator/=(float b){x/=b;y/=b;return *this;}
};
using std::sin;using std::cos;using std::floor;using std::abs;using std::max;
inline float fmod(float a,float b){return std::fmod(a,b);}
inline float lerp(float a,float b,float w){return a+(b-a)*w;}
inline float2 lerp(float2 a,float2 b,float w){return a+(b-a)*w;}
inline float2 rotate2(float2 p,float a){return {std::cos(a)*p.x-std::sin(a)*p.y,std::sin(a)*p.x+std::cos(a)*p.y};}
struct PlayPose {float2 center,size;float angle,roundness,fold;float signal{.5f};};
inline PlayPose playPose(float world,float mode,float id,float tempoPhase){
 PlayPose p;float u=(id+.5f)/20,side=id<10?-1:1,l=fmod(id,10),v=(l+.5f)/10;
 float t=tempoPhase*.19634954f,a=u*6.2831853f,w=sin(t),q=.5f-.5f*cos(t),st=sin(t-a);
 p.center=0;p.size=float2(.13f,.13f);p.angle=0;p.roundness=1;p.fold=0;
 if(world<26.5f){
  float orbit=a+t*.5f;
  p.center=float2(cos(orbit)*.79f,sin(orbit)*.72f);
  if(mode>.5f&&mode<1.5f)p.center=float2(sin(orbit)*.82f,sin(orbit*2)*.67f);
  else if(mode<2.5f&&mode>1.5f)p.center=float2(cos(v*6.283185f+side*t*.5f)*.42f+side*.42f,sin(v*6.283185f+side*t*.5f)*.68f);
  else if(mode<3.5f&&mode>2.5f){p.center=float2((cos(orbit)-cos(orbit*3)*.12f)*.76f,(sin(orbit)+sin(orbit*3)*.12f)*.66f);}
  else if(mode<4.5f&&mode>3.5f)p.center=float2(side*.42f+cos(v*6.283185f+t)*.36f,sin(v*6.283185f+t)*.34f+side*.32f);
  else if(mode<5.5f&&mode>4.5f)p.center=float2(cos(orbit)*(.52f+.30f*sin(t+a)),sin(orbit)*.73f);
  else if(mode<6.5f&&mode>5.5f)p.center=float2(cos(a+t*.25f)*(.34f+.45f*q),sin(a+t*.25f)*(.32f+.42f*q));
  else if(mode<7.5f&&mode>6.5f)p.center=float2(side*.39f*cos(t*.5f)+cos(a+side*t)*.39f,sin(a+side*t)*.62f);
  else if(mode<8.5f&&mode>7.5f)p.center=float2(cos(a+t)*(.25f+.55f*u),sin(a+t)*(.25f+.50f*u));
  else if(mode>8.5f)p.center=float2((u*2-1)*.89f,sin(t)*.57f+sin(a)*.16f);
  p.size=float2(.14f,.14f)*(.82f+.18f*cos(a-t));
 }else if(world<27.5f){
  float radius=.39f+.16f*sin(t+a*.0f),turn=a;
  p.size=float2(.27f,.095f);p.roundness=.65f;p.fold=.65f;
  if(mode<.5f)radius=.31f+.24f*q;
  else if(mode<1.5f){turn=v*6.283185f+side*sin(t)*.15f;radius=.26f;p.center.x=side*.43f;p.size=float2(.20f,.085f);}
  else if(mode<2.5f){radius=.41f+.16f*cos(id*3.141592f)*w;turn+=cos(id*3.141592f)*w*.25f;}
  else if(mode<3.5f){radius=.17f+u*.53f;turn+=w*.55f;p.size.x=.19f;}
  else if(mode<4.5f){radius=.44f;p.size.x=.16f+.18f*q;turn+=w*.3f;}
  else if(mode<5.5f){turn=(v-.5f)*2.6f+side*1.570796f+w*.25f;p.center.x=side*.18f;radius=.48f;}
  else if(mode<6.5f){radius=.30f+.24f*(.5f+.5f*sin(t-id*.4f));turn+=st*.15f;}
  else if(mode<7.5f){turn=floor(id/5)*1.570796f+(fmod(id,5)-2)*.15f+sin(t)*.10f*cos(floor(id/5)*3.141592f);radius=.28f+fmod(id,5)*.065f+.08f*sin(t-fmod(id,5)*.5f);}
  else if(mode<8.5f){radius=.63f-.25f*q;p.size.x=.23f;turn-=w*.45f;}
  else {radius=.30f+.25f*q;turn+=sin(t)*.12f;p.size.y=.07f+.045f*q;}
  p.center+=float2(cos(turn)*radius,sin(turn)*radius);p.angle=turn;
 }else if(world<28.5f){
  p.center=float2((v*2-1)*.87f,side*.40f);p.size=float2(.075f,.24f);p.roundness=.05f;p.fold=.4f;
  float tip=sin(t-l*.55f+side*.6f);p.angle=tip*.65f;
  if(mode>.5f&&mode<1.5f){p.center.y=side*.29f+sin(l*1.570796f)*.23f;p.angle=tip*.9f;}
  else if(mode<2.5f&&mode>1.5f){p.center=float2(cos(a)*.73f,sin(a)*.64f);p.angle=a+tip*.65f;}
  else if(mode<3.5f&&mode>2.5f){p.center.y=(u*2-1)*.62f;p.center.x=(v*2-1)*.85f;p.angle=.35f+tip*.4f;}
  else if(mode<4.5f&&mode>3.5f){p.center.x=side*(.15f+v*.68f);p.center.y=sin(v*3.141592f)*.42f;p.angle=side*tip*.8f;}
  else if(mode<5.5f&&mode>4.5f){p.center.y=side*.29f+sin(l*.55f-t)*.20f;p.angle=cos(l*.55f-t)*.7f;}
  else if(mode<6.5f&&mode>5.5f){p.center=float2((v*2-1)*.82f,side*(v*2-1)*.6f);p.angle=side*.6f+tip*.4f;}
  else if(mode<7.5f&&mode>6.5f){p.center=float2((fmod(id,5)-2)*.36f,(floor(id/5)-1.5f)*.42f);p.size=float2(.12f,.17f);p.angle=sin(t-floor(id/5)*.8f)*.65f;}
  else if(mode<8.5f&&mode>7.5f){p.center=float2(cos(a)*.55f,sin(a)*.53f);p.angle=a+w*.8f;p.size.y=.31f;}
  else if(mode>8.5f){p.center.y=side*(.28f+.17f*q);p.angle=tip*.8f*q;}
 }else if(world<29.5f){
  p.center=float2(side*.38f,(v*2-1)*.82f);p.size=float2(.51f,.063f);p.roundness=.1f;p.fold=.8f;
  p.angle=sin(t-l*.45f)*.20f;
  if(mode>.5f&&mode<1.5f){p.center=float2(p.center.y,p.center.x);p.size=float2(p.size.y,p.size.x);p.angle=cos(t-l*.5f)*.18f;}
  else if(mode<2.5f&&mode>1.5f){p.angle=side*.42f+sin(t-l*.4f)*.12f;p.center.x=side*.24f;}
  else if(mode<3.5f&&mode>2.5f){p.center.x=side*.24f+sin(t-l*.4f)*.25f;p.angle=side*.3f*cos(t-l*.4f);}
  else if(mode<4.5f&&mode>3.5f){p.angle=side*(.5f+v*.6f+.15f*w);p.center=float2(side*(.20f+.07f*q),(v*2-1)*.70f);}
  else if(mode<5.5f&&mode>4.5f){p.center.y=(v*2-1)*.68f+side*.12f*sin(t);p.angle=sin(t)*.35f;}
  else if(mode<6.5f&&mode>5.5f){p.angle=side*.6f;p.center.x=side*.22f+sin(t+side)*.18f;}
  else if(mode<7.5f&&mode>6.5f){p.angle=cos(l*3.141592f)*(.23f+.18f*w);p.size.x=.42f;}
  else if(mode<8.5f&&mode>7.5f){p.center.x=side*(.28f+.22f*q);p.angle=side*v*.75f*q;}
  else if(mode>8.5f){p.center.x=side*(.22f+.20f*q);p.size.x=.30f+.21f*q;p.angle=side*.23f*w;}
 }else if(world<30.5f){
  float swing=sin(t-l*.36f)*.30f;p.center=float2((v*2-1)*.85f+swing*.25f,side*.38f+cos(t-l*.36f)*.14f);p.size=float2(.14f,.18f);p.roundness=1;
  if(mode>.5f&&mode<1.5f)p.center=float2((v*2-1)*.78f+w*.20f,side*.40f+cos(t)*.14f);
  else if(mode<2.5f&&mode>1.5f)p.center.x=(v*2-1)*.75f+side*w*.18f;
  else if(mode<3.5f&&mode>2.5f)p.center.y=side*.36f+sin(t-l*.62f)*.25f;
  else if(mode<4.5f&&mode>3.5f){p.center.x=(v*2-1)*.78f*cos(t*.5f);p.center.y=side*.28f+sin(t*.5f)*(v*2-1)*.45f;}
  else if(mode<5.5f&&mode>4.5f){p.center=float2(side*.43f+sin(t-l*.4f)*.18f,(v*2-1)*.79f);}
  else if(mode<6.5f&&mode>5.5f){p.center.y=side*(.20f+.46f*sin(v*3.141592f))+cos(t-l*.3f)*.11f;}
  else if(mode<7.5f&&mode>6.5f){p.center.x=(v*2-1)*(.56f+.29f*q);p.center.y=side*(.23f+.31f*q);}
  else if(mode<8.5f&&mode>7.5f){p.center=float2(cos(a+w*.3f)*.76f,sin(a+w*.3f)*.69f);}
  else if(mode>8.5f){p.center.x=(v*2-1)*.83f+side*w*.12f*q;p.center.y=side*.4f+cos(t-l*.36f)*.14f*q;}
  p.angle=-swing;p.fold=.15f;
 }else{
  float gather=.5f-.5f*cos(t);float radius=.28f+.40f*gather;
  p.center=float2(cos(a)*radius,sin(a)*radius);p.size=float2(.19f,.11f);p.angle=a+sin(t)*.15f;p.roundness=.05f;p.fold=.8f;
  if(mode<.5f){float2 dock=float2((fmod(id,5)-2)*.31f,(floor(id/5)-1.5f)*.33f);p.center=lerp(dock,p.center,gather);p.angle=sin(t-id*.22f)*.45f*gather;p.size=float2(.155f,.15f);}
  else if(mode>.5f&&mode<1.5f){p.center=float2(side*(.2f+sin(v*3.141592f)*(.5f+.08f*w)),cos(v*3.141592f)*(.60f+.08f*q));p.angle=side*(v*3.141592f-1.570796f+.15f*w);}
  else if(mode<3.5f&&mode>1.5f){float2 square=float2(cos(a),sin(a));square/=max(abs(square.x),abs(square.y));p.center=square*(.43f+.25f*gather);p.angle=0;if(mode>2.5f){p.center=rotate2(p.center,.785398f)*.9f;p.angle=.785398f;}}
  else if(mode<4.5f&&mode>3.5f){p.center=float2(cos(a+t*.25f)*.64f,sin(a+t*.25f)*.61f);p.angle=a+w*.4f;}
  else if(mode<5.5f&&mode>4.5f){p.center=float2(cos(a+w)*(.17f+.58f*u),sin(a+w)*(.17f+.58f*u));p.angle=a;}
  else if(mode<6.5f&&mode>5.5f){p.center=float2((v*2-1)*.83f,side*(.18f+.38f*gather));p.angle=side*w*.24f;}
  else if(mode<7.5f&&mode>6.5f){float bank=floor(id/5)*1.570796f;float2 local=float2(.18f+(fmod(id,5)-2)*.10f,.25f+.25f*gather);p.center=rotate2(local,bank);p.angle=bank;}
  else if(mode<8.5f&&mode>7.5f){p.center=float2(side*.4f+cos(v*6.283185f+t*.4f)*.32f,sin(v*6.283185f+t*.4f)*.44f);p.angle=v*3.141592f;}
  else if(mode>8.5f){p.center=float2((fmod(id,5)-2)*(.27f+.09f*gather),(floor(id/5)-1.5f)*(.28f+.12f*gather));p.angle=side*w*.35f*gather;p.size=float2(.15f,.15f);}
 }
 return p;
}
inline PlayPose mixPlayPose(PlayPose a,PlayPose b,float w){PlayPose p;p.center=lerp(a.center,b.center,w);p.size=lerp(a.size,b.size,w);p.angle=lerp(a.angle,b.angle,w);p.roundness=lerp(a.roundness,b.roundness,w);p.fold=lerp(a.fold,b.fold,w);p.signal=lerp(a.signal,b.signal,w);return p;}

inline PlayPose inheritStagePattern(PlayPose p,int world,int id,float seed,float beat){
 // Stable addresses carry the wall's row/column vocabulary into each topology.
 // They belong to the pieces, so passing through another formation never reindexes them.
 patterns::float2 address{float(id%5-2),float(id/5-2)};
 if(world==28||world==30)address={float(id%10-5),float(id/10-1)};
 if(world==29)address={float(id/10-1),float(id%10-5)};
 const auto field=patterns::sample(seed,address,beat);p.signal=field.signal;
 if(world==26){p.center=rotate2(p.center,field.row*.10f);p.center=p.center*(.94f+.12f*field.signal);p.center.y+=field.column*.065f;}
 else if(world==27){const float turn=field.row*.10f+field.column*.12f;p.center=rotate2(p.center,turn)*(.95f+.10f*field.signal);p.angle+=turn;p.size.x*=.92f+.16f*field.signal;}
 else if(world==28){p.center+=float2(field.row*.075f,field.column*.075f);p.angle+=(field.signal-.5f)*.30f;}
 else if(world==29){p.center+=float2(field.row*.085f,field.column*.055f);p.angle+=field.column*.12f;p.size.x*=.93f+.14f*field.signal;}
 else if(world==30){p.center+=float2(field.row*.075f,field.column*.085f);p.angle+=field.column*.12f;}
 else {p.center+=float2(field.row*.075f,field.column*.075f);p.angle+=field.column*.14f;p.size=p.size*(.94f+.12f*field.signal);}
 return p;
}

inline PlayPose program(int world,float seed,const show::PhraseFrame&state,int id,float beat){
 float from=state.from,to=state.to,blend=state.blend;
 if(state.enabled<.5f){float clock=beat/16;from=std::fmod(std::floor(clock)+std::floor(seed/4),float(show::ProgramCount(world)));to=std::fmod(from+1,float(show::ProgramCount(world)));blend=show::PhraseEase((clock-std::floor(clock)-.5f)*2);}
 auto a=playPose(float(world),from,float(id),beat);
 return inheritStagePattern(from==to?a:mixPlayPose(a,playPose(float(world),to,float(id),beat),blend),world,id,seed,beat);
}
inline void prepare(int world,int previous,float seed,float oldSeed,const show::PhraseFrame&state,const show::PhraseFrame&old,float beat,float transition,float identity,std::array<std::array<float,4>,40>&data,std::array<float,20>&signals){
 for(int id=0;id<20;++id){auto p=program(world,seed,state,id,beat);
  if(previous>=26&&previous<32&&previous!=world)p=mixPlayPose(program(previous,oldSeed,old,id,beat),p,show::PhraseEase(transition));
  const float settle=show::PhraseEase(identity);float2 grid{(float(id%5)-2)*.39f,(float(id/5)-1.5f)*.49f};
  p.center=lerp(p.center,grid,settle);p.size=lerp(p.size,float2(.197f,.248f),settle);p.angle*=1-settle;p.roundness*=1-settle;
  data[id*2]={p.center.x,p.center.y,p.size.x,p.size.y};data[id*2+1]={std::cos(p.angle),std::sin(p.angle),p.roundness,p.fold};
  signals[id]=lerp(p.signal,.5f,settle);
 }
}
}

#pragma once
#include "phrase_conductor.h"
#include <array>
#include <cmath>

namespace rarestage::motion {
constexpr float Pi=3.14159265358979323846f;
struct Pose {float x{},y{},a{},b{},angle{},round{1},span{1},light{.5f};};
inline float mix(float a,float b,float u){return a+(b-a)*u;}
inline Pose mix(Pose a,Pose b,float u){return {mix(a.x,b.x,u),mix(a.y,b.y,u),mix(a.a,b.a,u),mix(a.b,b.b,u),mix(a.angle,b.angle,u),mix(a.round,b.round,u),mix(a.span,b.span,u),mix(a.light,b.light,u)};}
inline bool Scene(int s){return s>=38&&s<=41;}
inline bool Arcs(int s){return s==39||s==41;}
inline bool Correspondence(int a,int b){return Scene(a)&&Scene(b)&&Arcs(a)==Arcs(b);}
// Sixteen persistent pieces share addresses across each authored pair. All
// programs keep the same angular carrier: a long set cannot accumulate a spin
// mismatch when a new program or scene arrives.
inline Pose pose(int scene,int mode,int id,float beat,float aspect){
 const float t=beat*Pi/8,side=id<8?-1.f:1.f,k=float(id%8),u=(k+.5f)/8;
 const float xscale=aspect*.48f,unitScale=std::min(aspect,1.f);
 const float pulse=.5f-.5f*std::cos(t),wave=std::sin(t-k*Pi/4);
 Pose p;
 if(!Arcs(scene)){
  p.angle=id<8?0:Pi/2;p.a=.31f*unitScale;p.b=.038f*unitScale;
  if(scene==38){
   float along=std::sin(t*.5f+k*.22f)*.18f;
   p.x=id<8?along*xscale:(u*2-1)*xscale;
   p.y=id<8?(u*2-1)*.47f:along*.47f;
   p.a=id<8?xscale*.63f:.40f;p.b=.027f*unitScale;p.round=.75f;
   switch(mode){
    case 1:p.x+=side*std::sin(t)*xscale*.24f;p.y+=side*std::cos(t)*.11f;break;
    case 2:p.angle+=Pi/4;p.a*=.85f;p.x*=.8f;p.y*=.8f;break;
    case 3:p.a*=.48f+.48f*(.5f+.5f*wave);p.x+=wave*.18f*xscale;break;
    case 4:p.angle+=side*(.20f+.40f*pulse);p.x*=.88f;p.y*=.88f;break;
    case 5:p.x=(u*2-1)*xscale;p.y=side*.22f+wave*.09f;p.angle=side*.25f*wave;p.a=xscale*.23f;p.b=.052f*unitScale;break;
    case 6:p.angle+=wave*.32f;p.a*=.74f;p.x+=side*pulse*.15f*xscale;break;
    case 7:p.x=(u*2-1)*xscale;p.y=side*(.06f+.34f*std::abs(std::sin(t*.5f+k*.18f)));p.angle=side*.55f;p.a=xscale*.22f;p.b=.047f*unitScale;break;
    case 8:p.angle+=side*Pi/4;p.x*=.55f+.4f*pulse;p.y*=.55f+.4f*pulse;p.a*=.8f;break;
    case 9:p.a*=.55f+.4f*pulse;p.b*=1.4f;p.x+=side*std::sin(t*.5f)*xscale*.12f;break;
   }
  }else{
   const float theta=u*2*Pi+side*t*.5f;
   p.x=std::cos(theta)*xscale*.74f;p.y=std::sin(theta)*.37f+side*.06f;
   p.a=.086f*unitScale;p.b=.086f*unitScale;p.round=1;
   switch(mode){
    case 1:p.x=(u*2-1)*xscale;p.y=side*std::sin(u*2*Pi-t)*.30f;p.a=.13f*unitScale;p.b=.055f*unitScale;break;
    case 2:p.x=side*xscale*.45f+std::cos(theta)*xscale*.36f;p.y=std::sin(theta)*.32f;p.round=.05f;p.angle+=Pi/4;break;
    case 3:p.x=(u*2-1)*xscale;p.y=side*.27f+wave*.10f;p.a=.13f*unitScale;p.b=.058f*unitScale;p.angle+=wave*.6f;break;
    case 4:p.x=std::cos(theta)*xscale*(.35f+.5f*u);p.y=std::sin(theta)*(.18f+.28f*u);p.a*=.75f+u*.7f;p.b=p.a;break;
    case 5:p.x=(u*2-1)*xscale;p.y=side*(.09f+.29f*pulse);p.a=.08f*unitScale;p.b=.16f*unitScale;p.round=.85f;break;
    case 6:p.x=std::sin(theta)*xscale*.86f;p.y=std::sin(theta*2)*.34f;p.round=.12f;p.angle+=wave*.7f;break;
    case 7:p.x=(u*2-1)*xscale;p.y=side*(.37f-std::abs(u*2-1)*.28f)+wave*.04f;p.round=.05f;p.angle+=Pi/4;p.a=.075f*unitScale;p.b=.10f*unitScale;break;
    case 8:p.x=std::cos(theta)*xscale*(.26f+.52f*pulse);p.y=std::sin(theta)*(.13f+.26f*pulse);p.a=.15f*unitScale;p.b=.045f*unitScale;p.angle+=std::sin(t+u*2*Pi)*.6f;break;
    case 9:p.x=(u*2-1)*xscale;p.y=side*.23f+std::sin(t*.5f)*.07f;p.a=.10f*unitScale;p.b=.10f*unitScale;p.round=.5f+.5f*std::sin(t);break;
   }
  }
 }else{
  // a=radius, b=half thickness, span=half angular extent. Counter-running
  // banks retain their phase during every within-family pose interpolation.
  p.angle=u*2*Pi+side*beat*Pi/32;
  p.a=id<8?.37f:.18f;p.b=id<8?.058f:.028f;p.span=.20f;
  if(scene==39){
   const float theta=u*2*Pi+side*beat*Pi/32;
   float reach=id<8?.22f:.085f,twist=side*.65f;
   p.a=id<8?.25f:.125f;p.b=id<8?.036f:.020f;p.span=.85f;
   p.angle=theta+twist;
   switch(mode){
    case 1:reach*=.35f+.95f*pulse;p.a*=.8f+.2f*pulse;p.span=.7f+.5f*pulse;break;
    case 2:reach+=wave*.055f;p.angle+=side*.5f*std::sin(t);p.span=.60f;break;
    case 3:p.x=side*.25f*aspect;p.a=.145f;p.b=.024f;reach=.10f;p.span=1.0f;break;
    case 4:reach=.24f;p.a=.17f;p.b=.045f;p.angle+=side*.6f*wave;p.span=.55f;break;
    case 5:reach*=.55f+1.1f*pulse;p.a*=.80f;p.span=1.20f;p.angle-=side*.6f*pulse;break;
    case 6:p.x=(u*2-1)*xscale;p.y=side*.25f+wave*.055f;reach=0;p.a=.13f;p.b=.033f;p.span=1.2f;p.angle=side*beat*Pi/32+side*.25f*wave;break;
    case 7:p.x=side*.18f*aspect;p.y=side*.10f;p.a*=.75f;reach*=.72f;p.span=1.30f;break;
    case 8:p.x=(id%4<2?-1.f:1.f)*xscale*.52f;p.y=(id%2?1.f:-1.f)*.24f;reach=.035f;p.a=.115f;p.b=.021f;p.span=.6f;p.angle+=side*.6f;break;
    case 9:reach*=1.2f;p.a*=.90f;p.span=.55f+.18f*pulse;p.angle+=side*.35f;break;
   }
   p.x+=std::cos(theta)*reach*aspect;p.y+=std::sin(theta)*reach;
   p.a*=unitScale;p.b*=unitScale;
  }else{
   p.x=side*xscale*.36f;p.y=0;p.a=(.10f+k*.046f)*unitScale;p.b=.018f*unitScale;p.span=1.65f;
   // Each bank's nested arcs retain their order while its center travels.
   const float target=id<8?0:Pi;
   p.angle=side*beat*Pi/32+target+std::round((u*2*Pi-target)/(2*Pi))*2*Pi;
   switch(mode){
    case 1:p.x=side*xscale*(.12f+.55f*pulse);p.span=1.10f+.65f*pulse;break;
    case 2:p.x=side*xscale*.37f*std::cos(t*.5f);p.y=side*.18f*std::sin(t*.5f);p.angle+=side*.3f*wave;break;
    case 3:p.x=(u*2-1)*xscale;p.y=side*.24f;p.a=.09f*unitScale;p.b=.034f*unitScale;p.span=1.20f;p.angle+=wave*.3f;break;
    case 4:p.x=0;p.y=0;p.a=(.06f+id*.027f)*unitScale;p.b=.010f*unitScale;p.span=2.15f;break;
    case 5:p.x=side*xscale*.48f;p.y=side*.14f*std::sin(t);p.span=.8f+u*.8f;p.angle+=side*k*.16f;break;
    case 6:p.x=(u*2-1)*xscale;p.y=side*(.08f+.23f*pulse);p.a=.10f*unitScale;p.b=.027f*unitScale;p.span=.8f;p.angle+=side*.4f*wave;break;
    case 7:p.x=side*xscale*.20f;p.y=side*.19f;p.a*=.80f;p.angle+=side*std::sin(t)*.7f;p.span=2.30f;break;
    case 8:p.x=side*xscale*.42f;p.y=0;p.a*=.65f+.45f*pulse;p.span=1.0f+.9f*pulse;p.b*=1.22f;break;
    case 9:p.x=side*xscale*.43f;p.y=wave*.02f;p.span=2.45f;p.b*=.85f;break;
   }
  }
 }
 p.light=.5f+.5f*std::sin(t-k*.5f+side);return p;
}
inline Pose program(int scene,int id,const show::PhraseFrame& f,float beat,float aspect){
 return mix(pose(scene,int(f.from),id,beat,aspect),pose(scene,int(f.to),id,beat,aspect),f.blend);
}
inline show::PhraseFrame effective(show::PhraseFrame f,float beat,float seed){
 if(f.enabled)return f;const float clock=beat/16;int mode=(int(std::floor(clock))+int(seed)/4)%10;
 f={float(mode),float((mode+1)%10),show::PhraseEase((clock-std::floor(clock)-.75f)*4),1,1,1,0,0};return f;
}
inline void prepare(int scene,int previous,show::PhraseFrame f,show::PhraseFrame old,float beat,float aspect,float transition,std::array<std::array<float,4>,40>& data,float seed=0,float oldSeed=0){
 f=effective(f,beat,seed);old=effective(old,beat,oldSeed);
 for(int id=0;id<16;++id){auto p=program(scene,id,f,beat,aspect);
  if(previous!=scene&&Correspondence(scene,previous)&&transition<1)p=mix(program(previous,id,old,beat,aspect),p,show::PhraseEase(transition));
  data[id*2]={p.x,p.y,p.a,p.b};data[id*2+1]={std::cos(p.angle),std::sin(p.angle),Arcs(scene)?std::cos(p.span):p.round,Arcs(scene)?std::sin(p.span):p.span};
 }
}
inline float entryCost(int from,int to,const show::PhraseFrame& old,float beat,float aspect,int mode){
 float cost=0;show::PhraseFrame next{float(mode),float(mode),1,1,1,1,0,0};
 for(int id=0;id<16;++id){auto a=program(from,id,old,beat,aspect),b=program(to,id,next,beat,aspect);
  auto aa=program(from,id,old,beat+.125f,aspect),bb=program(to,id,next,beat+.125f,aspect);
  cost+=std::hypot(a.x-b.x,a.y-b.y)+std::abs(a.a-b.a)+std::abs(a.b-b.b)+
    (1-std::cos(a.angle-b.angle))*.08f+std::hypot((aa.x-a.x)-(bb.x-b.x),(aa.y-a.y)-(bb.y-b.y))*4;
 }return cost/16;
}
inline int matchEntry(int from,int to,const show::PhraseFrame& old,float beat,float aspect,int preferred){
 int result=preferred;float best=entryCost(from,to,old,beat,aspect,preferred);
 for(int mode=0;mode<10;++mode){float cost=entryCost(from,to,old,beat,aspect,mode);if(cost+.025f<best){best=cost;result=mode;}}return result;
}
}

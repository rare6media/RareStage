#pragma once
#include "frame_constants.h"
#include <string>
// The RareStage launch scene and the standby scene it settles into. One shader, driven by two numbers the
// renderer writes into pool slots 20 and 21 (g_Param[5].x, .y): seconds since the intro began, and the mode
// (1 intro, 2 standby). In standby the time is parked far past the end of the timeline, so the finished
// frame is steady; it keeps breathing with the audio and shows the seven analysis lanes, which makes it a
// useful test scene as well as a calm place to wait.
//
// Timeline (seconds): 0.0 dark and a nebula fades up. 0.8 to 3.6 a hyperspace run accelerates. 3.2 to 4.0 the
// streaks resolve back into stars. 3.7 arrival flash and shockwave. 3.6 to 5.4 the stars inside the Rare mark
// light up from the centre outward, forming a constellation. 4.6 to 5.9 the mark itself resolves, with a light
// sweep across it. 6.5 the sequence is complete.
namespace rarestage::gen {

inline constexpr float IntroSeconds=6.5f;

inline std::string IntroSource(){
 std::string s="\n";s+=RS_FRAME_CBUFFER_HLSL;
 s+=R"(
Texture2D logoTex:register(t0);SamplerState linearSampler:register(s0);
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
float h21(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float2 h22(float2 p){return frac(sin(float2(dot(p,float2(127.1,311.7)),dot(p,float2(269.5,183.3))))*43758.5453);}
float vnoise(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3.-2.*f);return lerp(lerp(h21(i),h21(i+float2(1,0)),f.x),lerp(h21(i+float2(0,1)),h21(i+float2(1,1)),f.x),f.y);}
float fbm(float2 p){float s=0.,a=.5;[unroll] for(int k=0;k<4;++k){s+=a*vnoise(p);p=float2(1.6*p.x+1.2*p.y,-1.2*p.x+1.6*p.y)+7.3;a*=.5;}return s/.9375;}
static const float LOGOSIZE=1.4;
float4 logoAt(float2 p){float2 uv=float2(p.x/LOGOSIZE+.5,.5-p.y/LOGOSIZE);float inside=step(0.,uv.x)*step(uv.x,1.)*step(0.,uv.y)*step(uv.y,1.);float4 t=logoTex.SampleLevel(linearSampler,saturate(uv),0);return float4(t.rgb,t.a*inside);}
float3 starLayer(float2 p,float scale,float seed,float travel,float warp,float3 tint){
 float rr=length(p)+1e-3;float an=atan2(p.y,p.x);float depth=-log(rr)*1.2+travel*(1.+seed*.5);
 float2 g=float2(an*scale*.95,depth*scale*.45);float2 id=floor(g);float2 f=frac(g)-.5;float2 o=h22(id+seed)-.5;
 float len=lerp(.12,.7+warp*1.8,saturate(warp));float on=step(.8,h21(id+seed+7.));
 float2 d=float2(f.x-o.x*.6,(f.y-o.y*.6)/len);
 return tint*on*smoothstep(.1,0.,length(d))*(.6+warp*1.8)*smoothstep(0.,.3,rr);
}
float4 psGen(O i):SV_TARGET{
 float asp=resolution.x/max(resolution.y,1.);float2 p=float2((i.uv.x*2.-1.)*asp,1.-i.uv.y*2.);
 float t=g_Param[5].x;float standby=step(1.5,g_Param[5].y);
 float lane[7];lane[0]=saturate(bandsA.x);lane[1]=saturate(bandsA.y);lane[2]=saturate(bandsA.z);lane[3]=saturate(bandsA.w);lane[4]=saturate(bandsB.x);lane[5]=saturate(bandsB.y);lane[6]=saturate(bandsB.z);
 float bass=saturate(bandsA.z),lvl=saturate(level);
 float3 cy=float3(.12,.78,1.),lm=float3(.55,1.,.25),vi=float3(.5,.25,1.),nv=float3(.008,.025,.08);
 float r=length(p);
 // nebula
 float born=saturate(smoothstep(0.,1.6,t)+standby);
 float2 np=p*.9+float2(t*.02,t*.012);float2 w1=float2(fbm(np+3.1),fbm(np+7.7));float n=fbm(np+w1*1.6+t*.03);
 float3 col=lerp(nv,cy*.35,pow(saturate(n*1.4-.2),2.))+vi*pow(saturate(n-.55)*2.,2.)*.5+lm*pow(saturate(fbm(np*2.2-w1)-.62)*3.,2.)*.15;
 col*=born*(.3+.55*smoothstep(0.,3.,t)+standby*.1);
 // hyperspace run: speed rises, holds, then the streaks resolve into stars
 float run=smoothstep(.8,2.4,t)*(1.-smoothstep(3.2,4.0,t));
 float travel=.4*t+6.*pow(smoothstep(.8,3.6,t),2.)+standby*t*.05;
 float3 st=starLayer(p,7.,1.,travel,run,cy*1.4+.4)+starLayer(p,12.,2.,travel,run,float3(1,1,1))*.9+starLayer(p,18.,3.,travel,run,lm*1.2+.3);
 col+=st*born*(1.-.5*standby);
 // arrival: flash and a shockwave ring
 float arrive=step(3.7,t);float since=max(t-3.7,0.);
 col+=float3(.7,.9,1.)*exp(-pow((t-3.75)*5.,2.))*.14*exp(-r*1.4);
 col+=cy*exp(-pow((r-since*2.2)*7.,2.))*arrive*exp(-since*2.)*1.4;
 // constellation: stars inside the mark light up from the centre outward
 float2 gp=p*95.;float2 gid=floor(gp);float2 gf=frac(gp)-.5;float2 jo=(h22(gid)-.5)*.6;float2 cpos=(gid+.5+jo)/95.;
 float inMark=step(.35,logoAt(cpos).a);float d01=saturate(length(cpos)/(LOGOSIZE*.6));
 float appear=saturate((t-3.6-d01*.9)/.7);
 float twinkle=.75+.25*sin(time*4.+h21(gid)*30.);
 col+=lerp(cy,float3(1,1,1),.5)*inMark*step(.45,h21(gid+9.))*smoothstep(.22,0.,length(gf-jo))*appear*twinkle*3.2*(1.-.75*standby);
 // the mark itself
 float la=saturate(smoothstep(4.6,5.9,t)+standby);float4 lg=logoAt(p);
 col*=1.-la*.7*smoothstep(1.0,.15,r);   // a dark cavity behind the mark so its navy body reads against the nebula
 float ga=0.;[unroll] for(int k=0;k<8;++k){float an=k*.7854;ga+=logoAt(p+float2(cos(an),sin(an))*(.035+.02*(k&1))).a;}ga*=.125;
 float breathe=1.+bass*.35+lvl*.15;
 col+=cy*ga*la*(.45+.15*sin(time*1.3))*breathe;
 col=lerp(col,lg.rgb*(1.1+bass*.25),lg.a*la);
 float sweep=exp(-pow((dot(p,normalize(float2(.8,-.6)))-(t-4.7)*1.8+1.4)*4.5,2.))*step(t,6.4)*(1.-standby);
 col+=lg.a*sweep*float3(1.2,1.3,1.4)*la;
 // standby: seven lane meters under the mark, plus a ring that follows the level
 if(standby>.5){
  float h=.02;float3 mcol=float3(0,0,0);
  [unroll] for(int j=0;j<7;++j){float cx=(float(j)-3.)*.085;float hh=.012+lane[j]*.2;float2 q=abs(float2(p.x-cx,p.y+.80-hh*.5))-float2(.026,hh*.5);float sd=length(max(q,0.))+min(max(q.x,q.y),0.);
   mcol+=lerp(cy,lm,float(j)/6.)*(1.-smoothstep(0.,.006,sd))*(.5+lane[j]*1.2)+lerp(cy,lm,float(j)/6.)*exp(-max(sd,0.)*60.)*.25*(.3+lane[j]);}
  col+=mcol;
  col+=cy*exp(-pow((r-(.72+lvl*.06))*26.,2.))*.12*(.4+lvl);
 }
 col*=1.-.45*smoothstep(.6,1.7,r);
 col*=saturate(t*2.5)+standby;
 col=min(max(col,0.),4.);if(!all(isfinite(col)))col=0.;
 return float4(col,1.);
}
)";
 return s;
}
}

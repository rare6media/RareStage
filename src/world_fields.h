#pragma once
#include "world_spec.h"
#include <string>
// Scene families: cosmic space, circuit board, virus spread and error horizon. Each one is a screen-space
// field that reads the musical clock from the shared Frame cbuffer directly, so its choreography does not
// depend on the Director switching anything: the build tightens it, the drop breaks it open, the breakdown
// lets it breathe, and the pattern changes on phrase boundaries with a smooth morph rather than a cut.
//
// Identity values (spec.field.v, all 0..1), fixed meaning per family:
//   Cosmic   v0 nebula scale   v1 star density  v2 galaxy arms   v3 planet size   v4 ring tilt   v5 warp  v6 swirl  v7 spare
//   Circuit  v0 cell scale     v1 layers        v2 connectivity  v3 chip density  v4 pulse speed v5 tilt  v6 glow   v7 spare
//   Virus    v0 cell scale     v1 spread speed  v2 spikiness     v3 spore density v4 seed count  v5 mutation v6 glow v7 tendril
//   Error    v0 band count     v1 block size    v2 split amount  v3 base damage   v4 sun size    v5 reflection v6 static v7 flip
namespace rarestage::gen {

inline std::string FNum(float v){if(!std::isfinite(v))v=0;char b[40];std::snprintf(b,sizeof b,"%.4f",double(v));return b;}

inline const char* FieldLibraryHlsl(){
 return R"(
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
float P(int i){return g_Param[i>>2][i&3];}
// Integrated motion clocks and slow followers, written by the engine into pool slots 20-26.
// g_Clock: x travel, y pulse (beats), z drift, w warp travel.  g_Clock2: x energy, y build, z drop (slow).
#define g_Clock g_Param[5]
#define g_Clock2 g_Param[6]
float h11(float x){return frac(sin(x*127.1+SEED*.173)*43758.5453);}
float h21(float2 p){return frac(sin(dot(p,float2(127.1,311.7))+SEED*.173)*43758.5453);}
float2 h22(float2 p){float a=dot(p,float2(127.1,311.7))+SEED*.173,b=dot(p,float2(269.5,183.3))+SEED*.173;return frac(sin(float2(a,b))*43758.5453);}
float2 rot(float2 p,float a){float c=cos(a),s=sin(a);return float2(c*p.x-s*p.y,s*p.x+c*p.y);}
float vnoise(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3.-2.*f);return lerp(lerp(h21(i),h21(i+float2(1,0)),f.x),lerp(h21(i+float2(0,1)),h21(i+float2(1,1)),f.x),f.y);}
float fbm(float2 p){float s=0.,a=.5,n=0.;[loop] for(int k=0;k<5;++k){if(k>=OCT)break;s+=a*vnoise(p);n+=a;p=float2(1.6*p.x+1.2*p.y,-1.2*p.x+1.6*p.y)+7.3;a*=.5;}return s/max(n,.01);}
struct Mus{float tempo,beat,bar,energy,build,drop,breakdown,kick,snare,flux,sub,bass,lowmid,mid,pres,air,level,section,eS,bS,dS;};
Mus music(){
 Mus m;m.tempo=tempoPhase;m.beat=frac(tempoPhase);m.bar=frac(tempoPhase*.25);m.energy=saturate(audioEnergy);
 float sec=songSection;
 m.build=(abs(sec-1.)<.5)?smoothstep(0.,1.,saturate(sectionProgress*1.2)):0.;
 float hold=(abs(sec-2.)<.5)?(1.-.55*saturate(sectionProgress)):0.;
 m.drop=max(saturate(dropPulse),hold*.7);
 m.breakdown=(abs(sec-3.)<.5)?1.:0.;
 m.kick=saturate(kick);m.snare=saturate(snare);m.flux=saturate(flux);
 m.sub=saturate(bandsA.x);m.bass=saturate(bandsA.z);m.lowmid=saturate(bandsA.w);m.mid=saturate(bandsB.x);m.pres=saturate(bandsB.y);m.air=saturate(bandsB.z);
 m.level=saturate(level);m.section=sec;
 m.eS=saturate(g_Clock2.x);m.bS=saturate(g_Clock2.y);m.dS=saturate(g_Clock2.z);return m;
}
float4 patA(float idx){return float4(h11(idx*1.31+3.7),h11(idx*2.17+9.1),h11(idx*3.91+5.3),h11(idx*5.37+1.9));}
float4 patvec(Mus m){float x=m.tempo/PHRASE;float i=floor(x);return lerp(patA(i),patA(i+1.),smoothstep(.82,1.,frac(x)));}
// Structural accent: one small monolith or ring, never more than a fraction of the frame.
void accentApply(inout float3 col,float2 p,float3 edge,Mus m){
 if(ACCENT==1){float2 c=float2(ACCX,-.12);float2 d=abs(p-c)-float2(ACCSIZE*.22,ACCSIZE*2.6);float sd=length(max(d,0.))+min(max(d.x,d.y),0.);
  col=lerp(col,col*.12,1.-smoothstep(0.,.006,sd));col+=edge*exp(-abs(sd)*80.)*(.22+m.energy*.4+m.kick*.5);}
 else if(ACCENT==2){float2 c=float2(ACCX,.1);float r=length((p-c)*float2(1.,2.6));float sd=abs(r-ACCSIZE*2.6)-.004;col+=edge*exp(-max(sd,0.)*80.)*(.4+m.energy*.8+m.drop);}
}
)";
}

// ---- cosmic space ---------------------------------------------------------------------------
inline const char* CosmicHlsl(){
 return R"(
float3 starsFlat(float2 q,float scale,float seed,float3 tint,Mus m){
 float2 g=q*scale;float2 id=floor(g);float2 f=frac(g)-.5;float2 o=h22(id+seed)-.5;
 float mag=h21(id+seed*3.1);float on=step(1.-DENS,h21(id+seed+7.));
 float tw=.65+.35*sin(time*(1.5+mag*4.)+mag*40.+m.air*6.);
 return tint*on*smoothstep(.09*(.4+mag),0.,length(f-o*.7))*tw*(.6+mag);
}
float3 starsStreak(float2 q,float scale,float seed,float warp,float3 tint){
 float r=length(q)+1e-3;float a=atan2(q.y,q.x);float depth=-log(r)*1.3+g_Clock.w*(1.+seed*.5);
 float2 g=float2(a*scale*.95,depth*scale*.45);float2 id=floor(g);float2 f=frac(g)-.5;float2 o=h22(id+seed)-.5;
 float len=lerp(.1,.55+warp*1.4,warp);float on=step(1.-DENS,h21(id+seed+7.));
 float2 d=float2(f.x-o.x*.6,(f.y-o.y*.6)/len);
 return tint*on*smoothstep(.1,0.,length(d))*(.5+warp*1.2)*smoothstep(0.,.3,r);
}
float4 psGen(O i):SV_TARGET{
 Mus M=music();float4 PV=patvec(M);
 float asp=resolution.x/max(resolution.y,1.);float2 p=float2((i.uv.x*2.-1.)*asp,1.-i.uv.y*2.);
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float T=g_Clock.z*SPD;float emis=lerp(1.,max(P(2),0.),W_EMIS);
 float2 q=rot(p,sin(T*.11)*.12*SWAY+PV.w*.3);
 float warp=saturate((M.bS*.9+M.dS*.9)*(.45+WARPAMT)+PV.w*.1*M.eS);
 // nebula: domain-warped noise, bass drives the warp, the drop lights it up
 float2 np=q*(.7+NEB*1.5+PV.z*.5)+float2(T*.05,T*.03);
 float2 w1=float2(fbm(np+3.1),fbm(np+7.7));
 float n=fbm(np+w1*(1.4+M.bass*.6+SWIRL*1.2));
 float dust=smoothstep(.35,.8,fbm(np*1.7-w1));
 float glow=.5+M.eS*.7+M.dS*.5+M.bS*.3+M.kick*.08-M.breakdown*.12;
 float3 col=lerp(primary*.05,accent*.5,pow(saturate(n*1.35-.12),2.2))+spectral*pow(saturate(n-.55)*2.2,2.)*(.4+M.mid*1.4);
 col*=(1.-dust*.75)*glow*.7;
 float rid=pow(1.-abs(2.*fbm(np*1.3+w1*1.2)-1.),5.);col+=lerp(accent,spectral,n)*rid*(.4+M.mid*.8)*.45;
 // galaxy arms
 float r=length(q);float a=atan2(q.y,q.x);
 float gw=smoothstep(.35,.75,PV.y)*(.5+.5*smoothstep(.2,.6,ARMS));
 float arms=pow(.5+.5*cos(ARMSN*(a-log(r+.08)*2.6)+T*.2),3.);
 float gal=(arms*exp(-r*1.3)*(.5+.5*fbm(q*5.))+exp(-r*5.)*.9)*gw;
 col+=lerp(accent,spectral,saturate(r))*gal*(.4+M.bass*.6+M.dS*.6);
 // stars: flat layers, blending into radial streaks as the build tightens
 float3 sf=starsFlat(q+float2(T*.03,0.),9.,1.,spectral,M)+starsFlat(q*1.2+float2(T*.06,0.),16.,2.,primary*1.4+.2,M)+starsFlat(q*.9+float2(T*.1,0.),26.,3.,float3(1,1,1),M)*.8;
 float3 ss=starsStreak(q,7.,1.,warp,spectral)+starsStreak(q,12.,2.,warp,float3(1,1,1))*.9+starsStreak(q,18.,3.,warp,primary*1.4+.2);
 col+=lerp(sf,ss,smoothstep(.05,.5,warp))*(1.0+M.kick*.25);
 // planet with atmosphere and a ring
 float pw=smoothstep(.2,.55,PV.x)*PLANET;
 float2 pc=float2((PV.z*2.-1.)*asp*.5,(PV.w-.5)*.55);float R=(.14+PLANET*.22)*(1.+M.sub*.08+M.kick*.04);
 float2 d=(p-pc)/max(R,.02);float rr2=dot(d,d);float inside=smoothstep(1.,.97,sqrt(rr2));
 float z=sqrt(max(1.-rr2,0.));float3 nrm=float3(d,z);float3 L=normalize(float3(cos(PV.x*6.28),sin(PV.x*6.28)*.6,.7));
 float diff=saturate(dot(nrm,L)*.9+.1);float bands=fbm(float2(d.y*4.+fbm(d*3.+T*.02)*1.6,d.x*1.2));
 float3 surf=lerp(primary*.5,accent*.9,bands)*diff*(.7+M.energy*.5);
 float rim=pow(1.-z,3.);surf+=spectral*rim*(.5+M.energy+M.drop);
 float2 rd=float2(d.x,d.y/(.22+RINGT*.45));float rg=length(rd);
 float ring=smoothstep(1.45,1.5,rg)*smoothstep(2.35,2.28,rg)*(.45+.55*sin(rg*70.+h21(floor(rg*9.))*6.))*step(.5,PLANET);
 bool front=rd.y<0.;
 col+=spectral*ring*.45*pw*(front?1.:(1.-inside))*(.6+M.energy);
 col=lerp(col,surf,inside*pw);
 col+=spectral*exp(-max(sqrt(rr2)-1.,0.)*7.)*.18*pw*(.5+M.energy);
 // drop shockwave and kick breath
 float shock=exp(-pow((r-(1.-M.drop)*2.4)*6.,2.))*M.drop;
 col+=spectral*shock*.9+accent*M.kick*.12;
 col*=1.-.4*smoothstep(.6,1.7,length(p));
 accentApply(col,p,spectral,M);
 col*=emis;col=min(max(col,0.),4.);if(!all(isfinite(col)))col=0.;
 return float4(col,1.);
}
)";
}

// ---- circuit board --------------------------------------------------------------------------
// A lit relief: routed nets are raised copper over a dark solder mask, chips are bevelled blocks, and the
// surface is shaded with a moving key light, a metallic highlight and a reflected horizon. Pulses ride the
// nets, ambient motes drift up through the light, and every beat launches a small burst of sparks. All motion
// comes from the integrated clocks (g_Clock), so a change in the music bends the speed instead of jumping it.
inline const char* CircuitHlsl(){
 return R"(
float sdSeg(float2 p,float2 a,float2 b,out float t){float2 pa=p-a,ba=b-a;t=saturate(dot(pa,ba)/max(dot(ba,ba),1e-5));return length(pa-ba*t);}
float edgeOn(float2 m,float layer){return step(1.-CONN,h21(m*1.37+layer*17.1));}
float edgeFlow(float2 m,float layer){return h21(m*2.71+layer*5.3);}
void traceCell(float2 id,float2 f,float layer,out float dist,out float flowU,out float seed,out float via){
 float eb[4];eb[0]=edgeOn(id*2.+float2(1,0),layer);eb[1]=edgeOn(id*2.+float2(0,1),layer);eb[2]=edgeOn(id*2.+float2(-1,0),layer);eb[3]=edgeOn(id*2.+float2(0,-1),layer);
 float fl[4];fl[0]=edgeFlow(id*2.+float2(1,0),layer);fl[1]=edgeFlow(id*2.+float2(0,1),layer);fl[2]=edgeFlow(id*2.+float2(-1,0),layer);fl[3]=edgeFlow(id*2.+float2(0,-1),layer);
 float2 dirs[4];dirs[0]=float2(1,0);dirs[1]=float2(0,1);dirs[2]=float2(-1,0);dirs[3]=float2(0,-1);
 float cnt=eb[0]+eb[1]+eb[2]+eb[3];float2 c=float2(.5,.5);
 dist=9.;flowU=0.;seed=h21(id+layer*3.3);via=0.;int ia=-1,ib=-1;
 [unroll] for(int i=0;i<4;++i){
  if(eb[i]>.5){
   float t;float d=sdSeg(f,c+dirs[i]*.5,c+dirs[i]*.2,t);
   bool outward=(fl[i]>.5)==((dirs[i].x+dirs[i].y)>0.);
   if(d<dist){dist=d;flowU=outward?1.-t:t;seed=h21(id+float(i)*1.7+layer);}
   if(ia<0)ia=i;else ib=i;
  }
 }
 if(cnt>2.5){
  [unroll] for(int j=0;j<4;++j){if(eb[j]>.5){float t;float d=sdSeg(f,c+dirs[j]*.2,c,t);if(d<dist){dist=d;flowU=1.-t;seed=h21(id+float(j)*2.3+layer);}}}
  via=exp(-length(f-c)*16.);
 }else if(cnt>1.5){
  float t;float d=sdSeg(f,c+dirs[ia]*.2,c+dirs[ib]*.2,t);if(d<dist){dist=d;flowU=t;seed=h21(id+7.7+layer);}
 }else if(cnt>.5){
  via=exp(-length(f-(c+dirs[ia]*.2))*20.);
 }
}
// Height of the board at a point, plus what is on it: copper mask, glowing pulses, vias, chip mask.
float boardAt(float2 uv,float aaU,float cells,Mus M,float act,float3 primary,float3 accent,float3 spectral,out float ridge,out float3 emit,out float via,out float chip){
 float h=0.;ridge=0.;emit=float3(0,0,0);via=0.;
 [loop] for(int layer=0;layer<2;++layer){
  if(float(layer)>=LAYERS)break;
  float sc=cells*(layer==0?1.:1.73);float2 g=uv*sc+float2(layer*13.1,layer*7.7);float2 id=floor(g);float2 f=frac(g);
  float d,u,sd,v;traceCell(id,f,float(layer),d,u,sd,v);
  float aa=aaU*sc;float lw=(.034+GLOW*.02)*(layer==0?1.:.75);
  float r=1.-smoothstep(lw*.5-aa,lw*.5+aa+.003,d);
  float soft=exp(-d*(14.-GLOW*5.))*.3;
  float on=step(h21(id*1.7+3.1+float(layer)),act);
  float ph=frac(g_Clock.y*(.25+PSPD*.5)+sd-u*.45);
  float pulse=smoothstep(0.,.07,ph)*smoothstep(.34,.1,ph);
  h+=r*(layer==0?.55:.4)+v*.5;ridge=max(ridge,r);via=max(via,v);
  float3 lc=layer==0?primary:accent;
  emit+=lc*soft*(.12+on*.4)+spectral*(r*pulse*on*(1.1+M.kick*.5+M.dS*.7)+v*(.3+on*.8+M.kick*.3));
 }
 float2 g4=uv*cells*.2;float2 id4=floor(g4);float2 f4=frac(g4);chip=step(h21(id4*3.7+1.3),CHIPS*(ACCENT==3?1.:.0));
 float hh=.2+.07*h21(id4);float2 cd=abs(f4-.5)-float2(.3,hh);float csd=length(max(cd,0.))+min(max(cd.x,cd.y),0.);
 float body=chip*(1.-smoothstep(0.,.02,csd));float bevel=smoothstep(-.06,0.,csd);
 float pins=step(.5,frac(f4.x*18.))*step(abs(abs(f4.y-.5)-(hh+.03)),.016)*step(abs(f4.x-.5),.27)*chip;
 h=lerp(h,1.15-bevel*.35+(vnoise(f4*40.)-.5)*.04,body)+pins*.4;
 float die=smoothstep(.02,0.,max(abs(f4.x-.5)-.15,abs(f4.y-.5)-.075));float dot1=smoothstep(.03,.0,length(f4-float2(.5-.24,.5+hh-.05)));
 emit+=spectral*(exp(-abs(csd)*45.)*chip*(.3+M.eS*.45)+pins*.5+dot1*body*.8)+accent*die*body*(.35+M.kick*.6+M.dS*.6);
 chip=max(body,pins);
 h+=(vnoise(uv*70.)-.5)*.05+(vnoise(uv*22.)-.5)*.06;
 return h;
}
float4 psGen(O i):SV_TARGET{
 Mus M=music();float4 PV=patvec(M);
 float asp=resolution.x/max(resolution.y,1.);float2 p=float2((i.uv.x*2.-1.)*asp,1.-i.uv.y*2.);
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float emis=lerp(1.,max(P(2),0.),W_EMIS);
 float act=saturate(.25+M.eS*.45+M.bS*.45+M.dS*.5-M.breakdown*.2);
 float2 uv;float fade=1.;float hor=.55;float sky=0.;float3 V=float3(0,0,1);
 if(TILT>.5){float dep=1./max(hor-p.y,.07);sky=step(hor,p.y);uv=float2(p.x*dep*.6+sin(g_Clock.z*.2)*.4,dep+g_Clock.x*.9+PV.w*3.);fade=smoothstep(0.,.35,hor-p.y)*saturate(1.2-dep*.07);V=normalize(float3(p.x*.35,-(hor-p.y)*.3-.3,1.));}
 else{uv=rot(p,sin(g_Clock.z*.1)*.12+PV.z*.3)*(1.+.1*sin(g_Clock.z*.07))+float2(g_Clock.x*.15,g_Clock.x*.06);}
 float cells=CELLS*(.85+PV.x*.4);float aaU=fwidth(uv.x)+fwidth(uv.y);
 float ridge,via,chip;float3 emit;float h0=boardAt(uv,aaU,cells,M,act,primary,accent,spectral,ridge,emit,via,chip);
 float r1,v1,c1;float3 e1;float eps=max(aaU,.0015);
 float h1=boardAt(uv+float2(eps,0),aaU,cells,M,act,primary,accent,spectral,r1,e1,v1,c1);
 float h2=boardAt(uv+float2(0,eps),aaU,cells,M,act,primary,accent,spectral,r1,e1,v1,c1);
 float bump=(.02*BUMP)/eps;float3 N=normalize(float3(-(h1-h0)*bump,-(h2-h0)*bump,1.));
 // a key light that wanders slowly with the drift clock, plus a reflected horizon for the shine
 float la=g_Clock.z*.3+PV.z*6.28;float3 L=normalize(float3(cos(la)*.8,sin(la)*.6,.55));float3 H=normalize(L+V);
 float ndl=saturate(dot(N,L)*.7+.3);
 float spec=pow(saturate(dot(N,H)),70.)*(.25+ridge*1.6+chip*2.4);float spec2=pow(saturate(dot(N,H)),14.)*(.05+ridge*.3);
 float3 Rv=reflect(-V,N);float env=pow(saturate(Rv.y*.5+.5),3.)*(.3+ridge*1.1+chip*1.5);float fres=pow(1.-saturate(dot(N,V)),4.);
 float3 mask=lerp(primary*.03+float3(0,.012,.02),primary*.09,saturate(h0*.6));
 float3 metal=lerp(primary,accent,saturate(h0*.8))*.55;
 float3 albedo=lerp(mask,metal,ridge);albedo=lerp(albedo,float3(.012,.014,.02)+primary*.02,chip);
 float3 col=albedo*ndl*(1.+M.eS*.4);
 col+=lerp(spectral,float3(1,1,1),.35)*(spec*(.7+M.bass*.5+M.dS*.6)+spec2);
 col+=lerp(accent,spectral,.5)*env*(.25+fres*1.2)*(.6+M.eS*.5);
 col+=emit;
 float wave=exp(-pow((length(p)-M.beat*2.3)*6.,2.))*M.kick;float flood=exp(-pow((length(p)-(1.-M.drop)*2.8)*5.,2.))*M.drop;
 col+=emit*(wave*.9+flood*1.2)+spectral*(wave*.08+flood*.12)*ridge;
 col*=fade;
 float3 bg=lerp(primary*.02,accent*.16,smoothstep(1.,hor,p.y));
 col=lerp(col+bg*.3,bg+spectral*exp(-abs(p.y-hor)*14.)*.6*(.4+act),sky);
 if(TILT<=.5)col+=bg*.4;
 // ambient motes rising through the light, in three depth layers
 float3 sparks=float3(0,0,0);
 [unroll] for(int k=0;k<3;++k){
  float sc2=5.+float(k)*4.5;float2 gp=p*sc2+float2(0.,-g_Clock.x*(.25+float(k)*.18));float2 gid=floor(gp);float2 gf=frac(gp)-.5;
  float2 o=(h22(gid+float(k)*9.)-.5)*.7;float on2=step(1.-MOTES,h21(gid+float(k)*3.3));
  float tw=.5+.5*sin(time*(1.5+h21(gid)*3.)+h21(gid)*20.);float dd=length(gf-o);
  sparks+=lerp(spectral,float3(1,1,1),.4)*on2*tw*exp(-dd*(26.-float(k)*4.))*(.55-float(k)*.12)*(.35+M.eS*.7+M.air*.4);
 }
 // every beat launches a burst from a new spot; the kick and the drop make it bigger
 float slot=floor(M.tempo);float age=M.beat;float2 bc=(h22(float2(slot,3.))*2.-1.)*float2(asp*.8,.7);float strength=saturate(.18+M.kick*.7+M.dS*.8);
 [unroll] for(int j=0;j<10;++j){
  float a=h21(float2(slot,float(j)))*6.2832;float spd=.22+h21(float2(float(j),slot+1.))*.55;
  float2 sp=bc+float2(cos(a),sin(a))*spd*age*(1.+M.dS*.7)+float2(0.,-.22*age*age);
  sparks+=spectral*exp(-length(p-sp)*55.)*pow(1.-age,2.)*strength*1.2;
 }
 col+=sparks*(sky>.5?.5:1.);
 accentApply(col,p,spectral,M);
 col*=emis;col=min(max(col,0.),4.);if(!all(isfinite(col)))col=0.;
 return float4(col,1.);
}
)";
}

// ---- virus spread ---------------------------------------------------------------------------
inline const char* VirusHlsl(){
 return R"(
void voro(float2 x,out float d1,out float d2,out float2 center){
 float2 n=floor(x),f=frac(x);d1=8.;d2=8.;center=n;
 [unroll] for(int j=-1;j<=1;++j){[unroll] for(int k=-1;k<=1;++k){
  float2 gg=float2(k,j);float2 o=h22(n+gg);o=.5+.42*sin(time*.35*(.4+h21(n+gg))+6.2831*o);
  float2 r=gg+o-f;float d=dot(r,r);
  if(d<d1){d2=d1;d1=d;center=n+gg+o;}else if(d<d2)d2=d;}}
 d1=sqrt(d1);d2=sqrt(d2);
}
float4 psGen(O i):SV_TARGET{
 Mus M=music();float4 PV=patvec(M);
 float asp=resolution.x/max(resolution.y,1.);float2 p=float2((i.uv.x*2.-1.)*asp,1.-i.uv.y*2.);
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float T=g_Clock.z*SPD;float emis=lerp(1.,max(P(2),0.),W_EMIS);
 float2 pp=rot(p,sin(T*.09)*.1)*(1.+.07*sin(T*.08));float2 x=pp*CELLS+float2(T*.05,T*.03);
 float d1,d2;float2 cen;voro(x,d1,d2,cen);float2 cw=(cen-float2(T*.05,T*.03))/CELLS;
 // infection front: grows through the phrase, surges on the drop, heals before the next phrase
 float ph=frac(tempoPhase/PHRASE);float heal=smoothstep(.86,1.,ph);
 float R=ph*(1.5+SPREAD*1.4)*(.5+M.eS*.7)+M.dS*1.2+M.bS*.4;
 float2 s0=(PV.xy*2.-1.)*float2(asp*.75,.7);float2 s1=(PV.zw*2.-1.)*float2(asp*.75,.7);float2 s2=float2(-s0.x,s1.y*.6);
 float dist=length(cw-s0);
 if(SEEDS>1.)dist=min(dist,length(cw-s1)+.12);
 if(SEEDS>2.)dist=min(dist,length(cw-s2)+.2);
 dist+=(fbm(cw*3.+T*.2)-.5)*(.35+TENDRIL*.7);
 float inf=smoothstep(R+.18,R-.18,dist)*(1.-heal);
 // cell body, nucleus and membrane
 float hc=h21(floor(cen*7.));float edge=d2-d1;
 float membrane=smoothstep(.09+.07*inf*SPIKY,0.,edge);
 float nuc=exp(-d1*(5.-inf*2.))*(.5+.5*sin(time*(1.+hc*2.)+hc*30.+M.bass*1.5));
 float mut=fbm(pp*5.+hc*10.+T*.3*MUT*3.);
 float3 healthy=primary*(.05+nuc*.25+membrane*.5);
 float3 sick=lerp(accent,spectral,mut)*(.2+nuc*(1.1+M.kick*.9+M.dS*.6)+membrane*(1.0+M.snare*.6));
 float3 col=lerp(healthy,sick,inf);
 col+=spectral*membrane*inf*(M.pres*1.2)*step(.55,hc);
 // spores drifting over the field
 float2 sg=pp*(4.+SPORES*5.)+float2(T*.15,-T*.1);float2 sid=floor(sg);float2 sf=frac(sg)-.5;
 float2 so=(h22(sid+3.)-.5)*.5;float sr=length(sf-so);float sa=atan2(sf.y-so.y,sf.x-so.x);
 float rr=(.09+.05*h21(sid))*(1.+SPIKY*.5*cos(sa*11.+time*.8));float spore=smoothstep(rr,rr-.02,sr)*step(1.-SPORES*.45,h21(sid+9.));
 col=lerp(col,col+spectral*(.5+M.kick),spore*.8);
 // waves: snare flicker new infection nodes; drop shock
 float shock=exp(-pow((length(p)-(1.-M.drop)*2.6)*5.,2.))*M.drop;col+=accent*shock*.8;
 col*=1.+M.kick*.2*inf;
 float vig=1.-.35*dot(p,p)*.25;col*=vig;
 accentApply(col,p,spectral,M);
 col*=emis;col=min(max(col,0.),4.);if(!all(isfinite(col)))col=0.;
 return float4(col,1.);
}
)";
}

// ---- error horizon --------------------------------------------------------------------------
inline const char* ErrorHlsl(){
 return R"(
float3 cleanScene(float2 p,Mus m,float3 pr,float3 ac,float3 sp){
 float hy=-.08+SUNPOS*.2;float2 q=p;float refl=0.;
 if(q.y<hy){q.y=hy+(hy-q.y)*1.25;q.x+=sin(q.y*34.+time*2.)*.012*(1.+m.bass*3.);refl=1.;}
 float h=q.y-hy;
 float3 col=lerp(ac*.42,pr*.03,smoothstep(0.,1.1,h));
 float R=.28+SUNR*.3+m.sub*.05;float2 sc=float2(0.,hy+R*.62);float sd=length(q-sc);
 float slit=step(.5,frac((q.y-hy)*(11.+m.energy*8.)-time*.4))*smoothstep(0.,R*1.2,q.y-hy)+step(R*.55,q.y-hy);
 float sun=smoothstep(R,R-.012,sd)*saturate(slit);
 col=lerp(col,lerp(sp*1.4,ac*1.1,saturate((q.y-hy)/R)),sun);
 col+=sp*exp(-max(sd-R,0.)*6.)*.22*(.5+m.energy+m.drop);
 col+=pr*exp(-abs(h)*26.)*1.1*(.5+m.energy);
 float2 sg=q*float2(28.,18.);float sid=h21(floor(sg));float star=step(.985,sid)*smoothstep(.2,0.,length(frac(sg)-.5))*step(.05,h);
 col+=float3(1,1,1)*star*.6*(.5+.5*sin(time*3.+sid*50.));
 if(ACCENT==1){float2 d=abs(q-float2(ACCX,hy+ACCSIZE*1.6))-float2(ACCSIZE*.3,ACCSIZE*1.7);float msd=length(max(d,0.))+min(max(d.x,d.y),0.);col=lerp(col,pr*.03,1.-smoothstep(0.,.006,msd));col+=sp*exp(-abs(msd)*60.)*.5;}
 return col*(refl>.5?.55:1.)*(refl>.5?(.7+.3*sin((p.y-hy)*140.)):1.);
}
float4 psGen(O i):SV_TARGET{
 Mus M=music();float4 PV=patvec(M);
 float asp=resolution.x/max(resolution.y,1.);float2 p=float2((i.uv.x*2.-1.)*asp,1.-i.uv.y*2.);
 float3 pr,ac,sp;rarePalette(palette,pr,ac,sp);
 float emis=lerp(1.,max(P(2),0.),W_EMIS);
 // damage: a floor from the identity, plus the build, the drop and the snare. A clean breath lands on every bar.
 float breath=smoothstep(.16,0.,M.bar)*(1.-saturate(M.drop*1.5));   // a drop landing on the downbeat is never softened
 float I=saturate((BASED*.22+M.eS*.2+M.build*.55+M.drop*.9+M.snare*.15)*(1.-.85*breath)*(1.-.5*M.breakdown));
 float slot=floor(M.tempo*4.),slot2=floor(M.tempo);
 float wT=.25+.75*PV.x,wB=.25+.75*PV.y,wS=.25+.75*PV.z,wF=.25+.75*PV.w;
 float2 uv=p;
 // scanline tear: bands slide sideways, quantised so they read as digital
 float bands=lerp(16.,64.,BANDS);float band=floor((uv.y+1.)*.5*bands);
 float tev=step(h21(float2(band,slot)),I*(.2+.55*wT));
 float off=floor((h21(float2(band*1.7,slot+9.))-.5)*26.)/26.*I*(.35+wT)*.9;
 uv.x+=tev*off;
 // macroblock displacement and stuck rows
 float bs=lerp(.09,.38,BLOCK);float2 bid=floor((p+float2(6.,1.))/bs);float bev=step(h21(bid+slot2*.37),I*.5*wB);
 uv=lerp(uv,uv+(h22(bid+slot2)-.5)*bs*3.5*I,bev);
 uv.y=lerp(uv.y,(floor(uv.y/bs)+.5)*bs,bev*step(.55,h21(bid+2.)));
 // pixel-sort style streaks
 float sev=step(1.-.3*I*wS,h21(float2(band*.5,slot2+3.)));float K=lerp(5.,16.,h21(float2(band,7.)));
 uv.x=lerp(uv.x,floor(uv.x*K)/K,sev);
 // channel split along a direction that changes every sixteenth
 float sa=slot*2.4;float spl=(SPLIT*.04+.012)*I*(.3+wS);float2 dir=float2(cos(sa),sin(sa)*.35);
 float3 col=float3(cleanScene(uv+dir*spl,M,pr,ac,sp).r,cleanScene(uv,M,pr,ac,sp).g,cleanScene(uv-dir*spl,M,pr,ac,sp).b);
 // bit flips
 float fev=step(1.-FLIP*.28*I*wF,h21(bid*2.1+slot2));
 col=lerp(col,lerp(1.-saturate(col),col.gbr,step(.5,h21(bid+5.))),fev*.85);
 // static and scan flutter
 float nz=h21(float2(floor(p.x*160.),floor(p.y*90.)+slot))-.5;col+=nz*STATIC*I*.4;col*=1.-.12*I*sin(p.y*resolution.y*1.1);
 col*=1.+M.kick*.25;
 accentApply(col,p,sp,M);
 col*=emis;col=min(max(col,0.),4.);if(!all(isfinite(col)))col=0.;
 return float4(col,1.);
}
)";
}

// Identity constants are printed into the source as compile-time values; the same spec gives the same text.
inline std::string FieldHeader(const WorldSpec&w){
 const auto&f=w.field;const auto&v=f.v;
 std::string s="static const float SEED="+FNum(float(w.seed%997u))+";\nstatic const float PHRASE="+FNum(float(f.phraseBeats))+";\nstatic const int OCT="+std::to_string(int(f.octaves))+";\n";
 s+="static const int ACCENT="+std::to_string(int(f.accent))+";\nstatic const float ACCSIZE="+FNum(f.accentSize)+";\nstatic const float ACCX="+FNum((v[7]*2.f-1.f)*.9f)+";\n";
 s+="static const float SPD="+FNum(.6f+w.camera.speed*1.5f)+";\nstatic const float SWAY="+FNum(w.motion.sway*3.f)+";\nstatic const float W_EMIS="+FNum(w.modulation.emission)+";\n";
 switch(w.family){
  case Family::CosmicSpace:
   s+="static const float NEB="+FNum(v[0])+",DENS="+FNum(.05f+v[1]*.14f)+",ARMS="+FNum(v[2])+",ARMSN="+FNum(std::round(3.f+v[2]*3.f))+",PLANET="+FNum(.35f+v[3]*.65f)+",RINGT="+FNum(v[4])+",WARPAMT="+FNum(v[5])+",SWIRL="+FNum(v[6])+";\n";break;
  case Family::CircuitBoard:
   s+="static const float CELLS="+FNum(3.f+v[0]*4.f)+",LAYERS="+FNum(v[1]>.45f?2.f:1.f)+",CONN="+FNum(.42f+v[2]*.2f)+",CHIPS="+FNum(.15f+v[3]*.45f)+",PSPD="+FNum(v[4])+",TILT="+FNum(v[5])+",GLOW="+FNum(v[6])+",MOTES="+FNum(.05f+v[7]*.12f)+",BUMP="+FNum(.8f+v[6]*.5f)+";\n";break;
  case Family::VirusSpread:
   s+="static const float CELLS="+FNum(2.5f+v[0]*3.5f)+",SPREAD="+FNum(v[1])+",SPIKY="+FNum(v[2])+",SPORES="+FNum(v[3])+",SEEDS="+FNum(std::round(1.f+v[4]*2.f))+",MUT="+FNum(v[5])+",GLOW="+FNum(v[6])+",TENDRIL="+FNum(v[7])+";\n";break;
  default:
   s+="static const float BANDS="+FNum(v[0])+",BLOCK="+FNum(v[1])+",SPLIT="+FNum(v[2])+",BASED="+FNum(v[3])+",SUNR="+FNum(v[4])+",SUNPOS="+FNum(v[5])+",STATIC="+FNum(v[6])+",FLIP="+FNum(v[7])+";\n";break;}
 return s;
}
inline std::string FieldBody(const WorldSpec&w){
 switch(w.family){case Family::CosmicSpace:return CosmicHlsl();case Family::CircuitBoard:return CircuitHlsl();case Family::VirusSpread:return VirusHlsl();default:return ErrorHlsl();}
}
}

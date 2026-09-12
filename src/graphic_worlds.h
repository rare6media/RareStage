// Filled 2D fields: bounded analytic work, no per-object allocation or texture history.
R"HLSL(
struct GraphicInk {float distance;float3 color;};
bool isGraphicWorld(float world){return world>=35&&world<38;}
struct GraphicFold {float2 sourcePoint;float2 destinationPoint;float weight;};
GraphicFold graphicFold(float2 p){
 GraphicFold fold;float aspect=resolution.x/resolution.y,w=liquidEase(transition),arc=sin(w*3.141592);
 float front=lerp(-aspect*.70,aspect*.70,w)+sin(p.y*4+tempoPhase*.10)*.10*arc;
 float offset=p.x-front,roll=exp(-offset*offset/ .055)*arc;
 fold.sourcePoint=p+float2(roll*.13,sin(p.y*3+tempoPhase*.10)*roll*.035);
 fold.destinationPoint=p-float2(roll*.13,sin(p.y*3+tempoPhase*.10)*roll*.035);
 fold.weight=1-smoothstep(-.012,.012,offset);
 return fold;
}
float graphicStep(float clock){return floor(clock)+liquidEase((frac(clock)-.10)/.75);}
float3 graphicModes(bool old){
 float4 f=old?oldPhraseProgram:phraseProgram;
 float seed=old?previousPatternSeed:patternSeed,clock=tempoPhase/16;
 return f.w>.5?f.xyz:float3(fmod(floor(clock)+floor(seed/4),10),fmod(floor(clock)+1+floor(seed/4),10),liquidEase((frac(clock)-.75)*4));
}
float3 graphicColor(float lane,float position){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float slot=fmod(abs(lane),4);
 float3 c=slot<.5?primary:slot<1.5?lerp(primary,spectral,.65):slot<2.5?accent:lerp(accent,spectral,.55);
 return c*(.72+.16*saturate(position));
}
float contourField(float2 p,int mode){
 float aspect=resolution.x/resolution.y,x=p.x/max(aspect*.5,.2),y=p.y,t=tempoPhase*.392699;
 float wave=sin(x*2.2-t)*.16,detail=sin(x*4.1+t*.6)*.035;
 if(mode==0)return y-wave-detail;
 if(mode==1)return (y-sin(x*2-t*.45)*.08)/(.48+1.05*pow(x*x+.02,.44));
 if(mode==2)return y-wave*sin(y*3.0)-detail;
 if(mode==3)return rotate2(float2(x*.65,y),.38+.10*sin(t*.25)).y-wave*.5;
 if(mode==4)return y-sqrt(x*x+.018)*.30+.10*sin(t-x*2);
 if(mode==5)return (y-wave*.35)/(.38+.86*pow(sin(x*2.7+sin(t*.3)*.3),2));
 if(mode==6)return (y+.06*sin(x*3+t))/(.45+.70*sqrt(pow(cos(x*1.6-t*.2),2)+.025));
 if(mode==7){float2 q=float2(x*.72,y)-float2(.10*sin(t*.3),.04*cos(t*.4));return length(q)-.35-.035*sin(atan2(q.y,q.x)*4+t);}
 if(mode==8)return sqrt(x*x+.014)*.46+sqrt(y*y+.006)-.32-.035*sin(t-x*3);
 return y/(.85+.12*cos(t*.5))-.045*sin(x*2.5-t);
}
GraphicInk graphicContours(float2 p,float3 modes){
 GraphicInk ink;
 float f=lerp(contourField(p,(int)modes.x),contourField(p,(int)modes.y),modes.z);
 float lane=f*15,cell=floor(lane),local=frac(lane);
 ink.distance=(abs(local-.5)-.455)/15;
 ink.color=graphicColor(cell,.5+p.x/(resolution.x/resolution.y))*(.91+.09*sin(local*3.141592));
 return ink;
}
float4 ribbonPose(float x,int id,int mode){
 float t=tempoPhase*.448799,xn=x/(resolution.x/resolution.y)+.5;
 float phase=xn*7.16-t+id*1.570796,env=pow(sin(saturate(xn)*3.141592),2);
 float center=0,amplitude=.28,width=.034,depth=cos(phase);
 if(mode==1){amplitude=.16+.21*env;center=.05*sin(xn*3.141592+t*.2);width=.033+env*.009;}
 else if(mode==2){amplitude=.10+.13*(1-env);center=(id<2?-1:1)*.17*env;}
 else if(mode==3){center=(id-1.5)*.14;phase=xn*5.0-t+id*.10;amplitude=.095;depth=id*.2;}
 else if(mode==4){center=(id<2?-1:1)*.23;phase=xn*8.5-t+(id%2)*3.141592;amplitude=.14;width=.030;depth=cos(phase);}
 else if(mode==5){phase=xn*9.0-t+id*1.570796;amplitude=.23;width=.033;depth=cos(phase);}
 else if(mode==6){center=(id-1.5)*.10;phase=xn*4.8-t+(id%2)*3.141592;amplitude=.17;width=.032;depth=cos(phase);}
 else if(mode==7){phase=xn*10.0-t+id*1.570796;amplitude=.12+.21*env;width=.037;depth=cos(phase);}
 else if(mode==8){center=(id<2?-1:1)*(.055+.30*abs(xn*2-1));amplitude=.055;width=.033;phase=xn*5-t+(id%2)*3.141592;}
 else if(mode==9){center=(id-1.5)*(.115+.025*sin(t*.25));amplitude=.08;phase=xn*4.2-t+id*.16;depth=id*.2;}
 float curve=sin(phase);
 if(mode==5)curve=asin(sin(phase)*.96)*.72;
 return float4(center+curve*amplitude,depth,width*(.92+.12*depth),phase);
}
GraphicInk graphicRibbons(float2 p,float3 modes){
 GraphicInk ink;ink.distance=100;ink.color=0;float nearest=100,front=-100;bool inside=false;
 [unroll]for(int id=0;id<4;++id){
  float4 pose=lerp(ribbonPose(p.x,id,(int)modes.x),ribbonPose(p.x,id,(int)modes.y),modes.z);
  float d=abs(p.y-pose.x)-pose.z;
  float face=(p.y-pose.x)/max(pose.z,.001);
  float3 c=graphicColor(id,.55+pose.y*.2)*(.83+.13*cos(face*1.7));
  c+=graphicColor(id,1)*exp(-abs(face+.77)*18)*.08;
  if(d<0){if(!inside||pose.y>front){ink.distance=d;ink.color=c;front=pose.y;}inside=true;}
  else if(!inside&&d<nearest){ink.distance=d;ink.color=c;nearest=d;}
 }
 return ink;
}
float4 radialSetup(int mode,int ring,float2 p){
 float t=tempoPhase*.1963495,side=ring==0?1:-1,speed=ring==0?.11219975:.15707975;
 float rotation=side*graphicStep((tempoPhase+(ring==0?0:2))/4)*(ring==0?.448799:.628319);
 float inner=ring==0?.23:.055,outer=ring==0?.61:.20,span=.80;
 if(mode==1){float open=.5-.5*cos(t);inner+=open*(ring==0?.10:.025);outer+=open*(ring==0?.13:.03);span*=1-.32*open;}
 else if(mode==2){rotation+=sin(t)*sin(atan2(p.y,p.x))*side*.25;outer*=.88+.12*cos(t);span=.66;}
 else if(mode==3){rotation=side*tempoPhase*speed+side*.24*sin(t*.5);span=.62;inner*=.75;}
 else if(mode==4){inner*=1.1;outer*=.98;rotation+=side*.18*sin(t*2);span=.53+.22*(.5+.5*cos(t));}
 else if(mode==5){outer*=.73;inner*=.73;span=.87;}
 else if(mode==6){rotation=side*graphicStep((tempoPhase+ring)/2)*speed*2;outer*=.95;span=.48;}
 else if(mode==7){float clock=(tempoPhase+ring*2)/8;rotation=side*(tempoPhase*speed+.785398*(graphicStep(clock)-clock));span=.72;}
 else if(mode==8){float pressure=.5-.5*cos(t);inner*=.85+.35*pressure;outer*=.83+.23*pressure;span=.92;}
 else if(mode==9){inner*=1.25;outer*=1.08;rotation=side*tempoPhase*speed+side*.16*sin(t*.25);span=.72;}
 return float4(rotation,inner,outer,span);
}
GraphicInk graphicRadial(float2 p,float3 modes){
 GraphicInk ink;ink.distance=100;ink.color=0;
 float twin=lerp(modes.x==5?1:0,modes.y==5?1:0,modes.z);
 float2 q=p;q.x*=.82;q.x-=sign(q.x)*min(.34,resolution.x/resolution.y*.24)*twin;
 float radius=length(q),angle=atan2(q.y,q.x);
 [unroll]for(int ring=0;ring<2;++ring){
  float4 shape=lerp(radialSetup((int)modes.x,ring,q),radialSetup((int)modes.y,ring,q),modes.z);
  if(ring==1){float rim=lerp(radialSetup((int)modes.x,0,q).y,radialSetup((int)modes.y,0,q).y,modes.z);shape.z=min(shape.z,rim-.018);shape.y=min(shape.y,shape.z-.035);}
  float count=ring==0?14:10,unit=6.283185/count;
  float bend=(ring==0?1:-1)*.30*saturate((radius-shape.y)/max(.001,shape.z-shape.y));
  float cell=(angle-shape.x-bend)/unit,sector=floor(cell+.5),local=abs(frac(cell+.5)-.5);
  float d=max(max(shape.y-radius,radius-shape.z),(local-shape.w*.5)*unit*max(radius,.02));
  if(d<ink.distance){ink.distance=d;float address=sector-floor(sector/count)*count;ink.color=graphicColor(floor(address*.5)+ring,saturate((radius-shape.y)/(shape.z-shape.y)))*(.83+.15*saturate(radius/shape.z));}
 }
 return ink;
}
GraphicInk graphicField(float2 p,float world,bool old){
 float3 modes=graphicModes(old);
 if(world<35.5)return graphicContours(p,modes);
 if(world<36.5)return graphicRibbons(p,modes);
 return graphicRadial(p,modes);
}
float3 shadeGraphic(GraphicInk ink){
 float aa=max(1.05/resolution.y,min(.006,fwidth(ink.distance)*.65));
 float cover=1-smoothstep(-aa,aa,ink.distance);
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float3 background=float3(.0015,.0025,.006)+spectral*.006;
 // Audio lights the moving composition; the path itself follows the stable clock.
 float edge=exp(-abs(ink.distance)*145)*cover;
 float accentLight=saturate(kick*.45+snare*.30+percussion*.18+dropPulse*.35);
 return max(lerp(background,ink.color,cover)+graphicColor(2,.8)*edge*accentLight*.085,0);
}
float3 graphicWorld(float2 p,float world){
 float3 result;
 if(isGraphicWorld(previousScene)&&previousScene!=world&&transition<1){
  GraphicFold fold=graphicFold(p);
  // A narrow moving fold preserves connected silhouettes through the handover.
  // Intersecting two unrelated distance fields would fragment both compositions.
  result=lerp(shadeGraphic(graphicField(fold.sourcePoint,previousScene,true)),shadeGraphic(graphicField(fold.destinationPoint,world,false)),fold.weight);
 }else result=shadeGraphic(graphicField(p,world,false));
 float formation=liquidEase(identityWeight);
 if(formation>.00001){
  float2 identityPoint=p;float style=fmod(floor(phraseProgram.y),3),travel=1-formation;
  float rank=saturate(p.x/(resolution.x/resolution.y)+.5);
  if(style<.5){identityPoint.x+=sin(p.y*9+tempoPhase*.14)*travel*.14;rank=saturate(p.y+.5);}
  else if(style<1.5){identityPoint=rotate2(p,travel*.65);rank=saturate(length(p)*1.5);}
  else {identityPoint*=1+travel*.5;rank=1-saturate(abs(p.x)*1.5);}
  float mark=sampleChoreographyIdentity(identityPoint,0,.88*logoScale,0);
  float reveal=liquidEase((formation-rank*.65)/.35);
  // The identity inherits moving band colors while its surrounding world continues.
  float lane=contourField(p,0)*10+tempoPhase*.08;
  float3 material=lerp(graphicColor(floor(lane),.8),graphicColor(floor(lane)+1,.8),smoothstep(.82,1,frac(lane)))*1.10;
  result=lerp(result*(1-formation*.80),material,mark*reveal);
 }
 return result;
}
)HLSL"

// Prisms, blades and square mechanisms use different silhouettes and shared clocks.
R"HLSL(
float3 prismEnsemble(float2 p,float pressure,float snareHit,float hatHit){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float aspect=resolution.x/resolution.y,phase=tempoPhase*.19634954,variant=sceneVariant();
 float3 modes=phraseModes(patternSeed,false),shaping=phraseShapeFor(false);
 float blend=modes.z,mode=modes.x,nextMode=modes.y;
 float bank=lerp(sin(mode*1.047198),sin(nextMode*1.047198),blend);
 float3 c=ensembleBackdrop(p,primary,spectral,phase*.5);
 // Inverse row lookup keeps an infinite descending stream continuous at recycling.
 // Each stream has a different phase, while all crystals share a formation gesture.
 [loop]for(int stream=0;stream<3;++stream){
  float lane=stream-1,direction=stream==1?-1:1;
  float travel=tempoPhase*.034*direction;
  float offset=travel+lane*.085+.035*sin(phase+lane);
  float row=floor((p.y-offset)/.255);
  [loop]for(int neighbor=-2;neighbor<=2;++neighbor){
   float index=row+neighbor,cy=(index+.5)*.255+offset;
   float wave=sin(cy*4-phase+lane*.6);
   float answer=lerp(cos(mode*1.047198),cos(nextMode*1.047198),blend);
   float cx=lane*aspect*.35*shaping.y+wave*.055*shaping.x+bank*lane*.045+answer*.025*sin(cy*5-phase)+shaping.z*.12*lane;
   float turn=.20*sin(phase+lane*.7)+bank*.28+direction*.14*sin(cy*3-phase);
   float2 q=rotate2(p-float2(cx,cy),turn);
   float2 scale=float2(aspect*.155,.153+.008*variant);
   q/=scale;
   float d=max(abs(q.y),dot(abs(q),float2(.866025,.5)))-1;
   float aa=max(.008,fwidth(d)*1.1),face=1-smoothstep(-aa,aa,d);
   float shadow=exp(-max(d,0)*10)*(1-face);c*=1-shadow*.30;
   float3 hue=stream==0?primary:stream==1?spectral:accent;
   float facet=q.x>0?(q.y>q.x*.57735?.32:.64):.20;
   float bevel=1-smoothstep(.02,.11,-d);
   float seam=exp(-abs(q.x)*80)+exp(-abs(q.y-q.x*.57735)*65)*step(0,q.x);
   float chase=.5+.5*cos(cy*7-phase*2+lane*1.570796);
   float3 crystal=hue*(facet+.04*chase+.008*pressure)+hue*seam*.045;
   crystal+=lerp(hue,accent,.12)*bevel*(.06+.035*snareHit+.025*hatHit);
   c=lerp(c,crystal,face);
  }
 }
 return c+materialIdentity(p,0,.82*logoScale,primary,accent,6);
}
float3 bladePose(float mode,float lane,float side,float span){
 float phase=tempoPhase*.19634954;
 float2 anchor=float2(side*span*.24,lane*.119);float angle=side*.30;
 if(mode<.5){anchor.y+=sin(phase+side+lane*.55)*.035;angle=side*(.35+.22*sin(phase));}
 else if(mode<1.5){anchor.x+=side*cos(lane*.55-phase)*span*.09;angle=side*(.55-lane*.12);}
 else if(mode<2.5){anchor.y+=side*.045*sin(phase);angle=side*.50+sin(lane*.5-phase)*.19;}
 else if(mode<3.5){anchor.y=lane*.11+side*.06*cos(phase);angle=-side*lane*.20;}
 else if(mode<4.5){anchor.x+=sin(phase+lane*.65)*span*.07;angle=side*.65+.14*sin(phase);}
 else {anchor.y+=sin(abs(lane)*.75-phase)*.06;angle=side*.25+lane*.10*sin(phase);}
 return float3(anchor,angle);
}
float3 vaultEnsemble(float2 p,float pressure,float snareHit,float hatHit){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float aspect=resolution.x/resolution.y,phase=tempoPhase*.19634954,variant=sceneVariant();
 float3 modes=phraseModes(patternSeed,false),shaping=phraseShapeFor(false);
 float blend=modes.z,mode=modes.x,nextMode=modes.y;
 float lock=lerp(cos(mode*1.047198),cos(nextMode*1.047198),blend);
 float counter=lerp(sin(mode*1.047198),sin(nextMode*1.047198),blend);
 float3 c=ensembleBackdrop(p,spectral,primary,phase*.5);
 // Different-sized solid square frames turn as a linked mechanism. No tunnel
 // recycling, thin wireframe outlines or dependence on individual beat kicks.
 [loop]for(int element=0;element<9;++element){
  int index=element==8?4:element<4?element:element+1;
  float column=index%3-1,row=index/3-1;
  float center=element==8?1:0,side=fmod(index,2)<.5?-1:1;
  float2 anchor=float2(column*aspect*.35,row*.34);
  anchor*=shaping.y;anchor+=float2(row*.035*sin(phase),column*.035*cos(phase))*shaping.x;
  anchor.x+=row*shaping.z*.10*sin(phase);
  float angle=side*phase*.25+(row+column)*.22*lock+side*.30*shaping.x*sin(phase+row*.55)+counter*.16*sin(phase+column*.8);
  float size=min(.24,aspect*.31)*(1+.12*sin(index*1.570796))*(1+.10*center);
  float2 q=rotate2(p-anchor,angle);
  float distance=max(abs(q.x),abs(q.y)),thickness=size*(.24+.025*variant);
  float d=abs(distance-size)-thickness;
  float aa=max(.0012,1.3/resolution.y),face=1-smoothstep(-aa,aa,d);
  c*=1-exp(-max(d,0)*65)*(1-face)*.24;
  float3 hue=center>.5?primary:side<0?accent:spectral;
  float rim=saturate((distance-size+thickness)/(2*thickness));
  float facet=lerp(.23,.65,rim)*lerp(.75,1,step(abs(q.y),abs(q.x)));
  float edge=1-smoothstep(aa,aa+.006,-d);
  float chase=.5+.5*cos((q.x+q.y)*9-phase*2+index*.7);
  float3 surface=hue*(facet+.035*chase+.008*pressure)+hue*edge*(.04+.035*snareHit+.025*hatHit);
  c=lerp(c,surface,face);
  if(center>.5){
   float2 inner=rotate2(p-anchor,-angle*1.4);
   float innerD=abs(max(abs(inner.x),abs(inner.y))-size*.45)-size*.10;
   float innerFace=1-smoothstep(-aa,aa,innerD);
   c=lerp(c,accent*(.32+.07*chase),innerFace);
  }
 }
 return c+materialIdentity(p,0,.79*logoScale,primary,accent,12);
}
)HLSL"

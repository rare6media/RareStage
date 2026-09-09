// Full-field ensembles: opaque fins and segmented rotors, with phrase-led poses.
// All structural movement uses the persistent musical clock; hits only light edges.
R"HLSL(
float ensembleBlend(float clock){return liquidEase(saturate((frac(clock)-.5)*2));}
float ensembleMode(float stepIndex,float seed){return fmod(stepIndex+floor(seed/4),6);}
float3 ensembleBackdrop(float2 p,float3 primary,float3 accent,float phase){
 float depth=.5+.5*cos(p.y*7+sin(p.x*3-phase)*1.4);
 float3 c=float3(.004,.007,.019)+primary*(.006+.014*depth);
 float broad=.5+.5*cos(p.y*16+sin(p.x*2-phase)*2.8);
 c+=lerp(primary,accent,.28)*pow(broad,5)*.025;
 return c;
}
float4 finPose(float mode,float u,float side,float span){
 float phase=tempoPhase*.19634954;
 float2 anchor=float2(u*span,side*.23);float angle=side*.34,stretch=1;
 if(mode<.5){anchor.y+=sin(u*3-phase+side*.8)*.105;angle+=cos(u*3-phase+side*.8)*.30;}
 else if(mode<1.5){anchor.y=side*(.12+.23*(1-u*u));angle=-side*u*.95;stretch=1.1;}
 else if(mode<2.5){anchor.y=side*.20+sin(u*4-phase)*.13;anchor.x+=side*sin(phase)*.045;angle=side*.55+cos(u*4-phase)*.22;}
 else if(mode<3.5){anchor.y=side*(.22+.07*cos(u*3.141592-phase));angle=side*(.55+.42*sin(phase+u*2));stretch=.9;}
 else if(mode<4.5){anchor.y=side*.24+u*.12*sin(phase);angle=side*.50+sin(u*3-phase)*.42;}
 else {anchor.y=side*(.20+.075*cos(abs(u)*4-phase));angle=-side*u*.8+sin(phase)*.24;stretch=1.08;}
 return float4(anchor,angle,stretch);
}
float3 kineticEnsemble(float2 p,float pressure,float snareHit,float hatHit){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float aspect=resolution.x/resolution.y,span=aspect*.50;
 float clock=tempoPhase/16,blend=ensembleBlend(clock);
 float mode=ensembleMode(floor(clock),patternSeed),nextMode=ensembleMode(floor(clock)+1,patternSeed);
 float3 c=ensembleBackdrop(p,primary,spectral,tempoPhase*.098175);
 float variant=sceneVariant();
 // Two interlocking banks span the frame; their shared pose makes waves, fans,
 // opposing sweeps and arches readable even without percussion.
 [loop]for(int bank=0;bank<2;++bank){
  float side=bank==0?-1:1;
  [loop]for(int fin=0;fin<9;++fin){
   float u=(fin-4)/4.0;
   float4 pose=lerp(finPose(mode,u,side,span),finPose(nextMode,u,side,span),blend);
   pose.xy+=float2(side*.022*sin(tempoPhase*.098175),variant*.006*side);
   float2 q=rotate2(p-pose.xy,pose.z);
   float width=min(.14,aspect*.095),height=(.22+.015*variant)*pose.w;
   q.x+=q.y*.18*side;
   // Tapered, folded leaves have a broad face, spine and visibly different facets.
   float taper=max(0,1-abs(q.y)/height);
   float d=max(abs(q.x)-width*taper,abs(q.y)-height);
   float aa=max(.0012,1.3/resolution.y),face=1-smoothstep(-aa,aa,d);
   float shadow=exp(-max(d,0)*65)*(1-face);
   c*=1-shadow*.28;
   float chase=.5+.5*cos(u*3.141592-tempoPhase*.785398+bank*3.141592);
   float3 hue=lerp(bank==0?primary:accent,spectral,.06+.06*sin(u*2+tempoPhase*.098175));
   float facet=lerp(.28,.66,smoothstep(-aa,aa,q.x+q.y*.10));
   float spine=exp(-abs(q.x+q.y*.10)*450);
   float3 surface=hue*(facet+.055*chase+.010*pressure)+accent*spine*.08;
   float edge=1-smoothstep(aa,aa+.007,-d);
   surface+=hue*edge*(.035+.035*snareHit+.025*hatHit);
   c=lerp(c,surface,face);
  }
 }
 return c+materialIdentity(p,0,.82*logoScale,primary,accent,10);
}
float3 orbitalEnsemble(float2 p,float pressure,float snareHit,float hatHit){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float aspect=resolution.x/resolution.y,variant=sceneVariant(),clock=tempoPhase/16;
 float mode=ensembleMode(floor(clock),patternSeed),blend=ensembleBlend(clock);
 
 // Discrete programs are mixed as periodic signals, never as a wrapped angle.
 float fan=lerp(cos(mode*1.047198),cos(ensembleMode(floor(clock)+1,patternSeed)*1.047198),blend);
 float phase=tempoPhase*.19634954;
 float3 c=ensembleBackdrop(p,spectral,primary,phase*.5);
 // A large central rotor and two counter-rotating wings fill wide displays.
 // On narrow displays they stack vertically instead of falling off the sides.
 [loop]for(int rotor=0;rotor<3;++rotor){
  float side=rotor==0?-1:rotor==1?1:0;
  float2 anchor=aspect>=1?float2(side*aspect*.43,side*.075*sin(phase)):float2(side*.055*sin(phase),side*.40);
  float radius=rotor==2?.43:.38;
  float2 q=p-anchor;q.x/=max(.70,aspect*.64);
  float r=length(q),angle=atan2(q.y,q.x);
  float direction=rotor==2?1:-1;
  [loop]for(int tier=0;tier<3;++tier){
   float count=8+2*variant;
   float turn=direction*phase*.50+(tier%2==0?1:-1)*.14*sin(phase+fan);
   float sector=(angle-turn)*count/6.283185;
   float lane=floor(sector),local=frac(sector)-.5;
   float laneAngle=(lane+.5)*6.283185/count+turn;
   float sweep=.5+.5*cos(laneAngle*2-direction*phase-tier*.8);
   float radial=radius*(.32+tier*.285)+.017*fan*cos(laneAngle*2-phase);
   float halfWidth=.042+.012*sweep;
   float dr=r-radial;
   float edgeAngle=abs(local)-(.39-.07*dr/halfWidth);
   float d=max(abs(dr)-halfWidth,edgeAngle*max(r,.03)*6.283185/count);
   float aa=max(.0012,1.3/resolution.y),face=1-smoothstep(-aa,aa,d);
   float innerShade=saturate(.5+dr/max(halfWidth,.001)*.5);
   float3 hue=lerp(rotor==2?primary:accent,spectral,tier==2?.85:tier*.06);
   float facet=lerp(.25,.66,innerShade)*(.82+.18*sweep);
   float spine=exp(-abs(dr+halfWidth*.48)*420);
   c*=1-exp(-max(d,0)*70)*(1-face)*.18;
   float3 surface=hue*(facet+.01*pressure)+accent*spine*.075;
   float edge=1-smoothstep(aa,aa+.005,-d);
   surface+=hue*edge*(.035+.035*snareHit+.025*hatHit);
   c=lerp(c,surface,face);
  }
 }
 return c+materialIdentity(p,0,.78*logoScale,primary,accent,19);
}
)HLSL"

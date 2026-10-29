// One parametric ribbon rig, with continuous pose and motif transport across its scenes.
R"HLSL(
bool isRibbonRig(float world){return sceneRig[(int)world].y==2;}
float3 ribbonPose(float world){return abs(world-22)<.1?float3(.38,.72,.006):abs(world-23)<.1?float3(1.570796,.12,.010):float3(0,0,0);}
float rigGesture(float clock,float lane,float position,float seed,bool old){
 float3 modes=phraseModes(seed,old),shaping=phraseShapeFor(old);
 if((old?oldPhraseProgram.w:phraseProgram.w)>.5){
  float a=sculptureMotif(modes.x,lane,position,tempoPhase*.392699);
  float b=sculptureMotif(modes.y,lane,position,tempoPhase*.392699);
  return lerp(a,b,modes.z)*shaping.x+shaping.z*sin(lane*.6-tempoPhase*.19635);
 }
 return lerp(sculptureMotif(authoredMotif(floor(clock),seed),lane,position,tempoPhase*.392699),sculptureMotif(authoredMotif(floor(clock)+1,seed),lane,position,tempoPhase*.392699),motifBlend(clock));
}
float3 composedRibbon(float2 p,float world,float pressure,float snareHit,float hatHit){
 bool bridge=isRibbonRig(scene)&&isRibbonRig(previousScene)&&abs(scene-previousScene)>.1;
 float blend=bridge?liquidEase(transition):1;
 float3 pose=bridge?lerp(ribbonPose(previousScene),ribbonPose(scene),blend):ribbonPose(world);
 float turnBridge=bridge&&(abs(scene-23)<.1||abs(previousScene-23)<.1)?1:0;
 if(turnBridge>.5)pose.x=lerp(ribbonPose(previousScene).x,ribbonPose(scene).x,liquidEase(saturate((transition-.22)/.56)));
 float variant=fmod(floor(patternSeed/16),4),oldVariant=fmod(floor(previousPatternSeed/16),4);
 // Arrangement is part of the same pose, so it also has a continuous handoff.
 variant=bridge?lerp(oldVariant,variant,blend):variant;
 float settle=sqrt(saturate(identityWeight));
 float2 q=rotate2(p,pose.x*(1-settle));q*=1+variant*.055;
 float monument=smoothstep(.6,1.5,pose.x);
 // Preserve the ensemble's width on landscape and ultra-wide screens.
 q.y/=lerp(1,max(1,resolution.x/resolution.y*1.06),monument*(1-settle));
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float3 c=float3(.004,.009,.024);float clock=tempoPhase*.125;
 // Two broad recessed folds keep a surrounding composition through the gather.
 float field=.5+.5*cos(p.y*13+sin(p.x*3-tempoPhase*.098175)*2);
 c+=lerp(primary,spectral,.65)*(.006+.026*pow(field,4));
 [loop]for(int lane=0;lane<11;++lane){
  float l=lane-5;
  float gesture=rigGesture(clock,l,q.x,patternSeed,false);
  if(bridge)gesture=lerp(rigGesture(clock,l,q.x,previousPatternSeed,true),gesture,blend);
  float bankArc=.5+.5*sin((floor(clock)+motifBlend(clock))*.785398+variant*.45);
  float road=sin(q.x*(2.1+variant*.15)-tempoPhase*.098175)*pose.y*(.18+.12*bankArc);
  float path=l*.087+gesture*.054+sin(q.x*3.1-tempoPhase*.19635+l*.75)*.042+road;
  path=lerp(path,l*.087,settle);
  float width=lerp(.034+pose.z+.005*(.5+.5*cos(q.x*3+l*.5-tempoPhase*.3927)),.044,settle);
  float d=q.y-path,aa=max(.001,1.2/resolution.y),face=1-smoothstep(width-aa,width+aa,abs(d));
  // Monument ends rise and fall continuously; racetrack lanes remain unbroken.
  float angle=pose.x*(1-settle);
  float visibleSpan=.5*(abs(cos(angle))*resolution.x/resolution.y+abs(sin(angle)))*(1+variant*.055)+.16;
  float span=lerp(visibleSpan,.42+.075*gesture+.035*cos(l*.5-tempoPhase*.19635),monument*(1-settle));
  // Gather the long strips before a quarter-turn. Otherwise an infinite bright
  // ribbon sweeps the entire display during a short musical handoff.
  // Allocate real handoff time to gathering and release. Applying another
  // quintic to the already-eased scene blend compressed the visible release.
  float gather=liquidEase(saturate(transition/.26))*(1-liquidEase(saturate((transition-.74)/.26)));
  span=lerp(span,.24,gather*turnBridge);
  face*=1-smoothstep(span-.008,span+.008,abs(q.x));
  float chase=.5+.5*cos(q.x*5-l*.65-tempoPhase*.785398);
  float colorWave=.5+.5*sin(l*.48+tempoPhase*.098175);
  float3 hue=lerp(primary,spectral,smoothstep(.25,.75,colorWave));
  hue=lerp(hue,accent,.04+.08*chase);
  // An asymmetric spine creates two crisp planar folds instead of round tubing.
  float ridge=d+width*(.22+.12*sin(q.x*2-tempoPhase*.19635+l*.5));
  float facet=lerp(.25,.57,smoothstep(-aa,aa,ridge));
  float seam=exp(-abs(ridge)*500);
  float3 surface=hue*(facet+.055*chase+.010*pressure)+accent*seam*.08;
  surface+=exp(-abs(abs(d)-width)*400)*hue*(.035+.025*snareHit+.02*hatHit);
  c=lerp(c,surface,face);
 }
 c+=exp(-abs(p.y-.40)*65)*primary*.026;
 return c+materialIdentity(p,0,.82*logoScale,primary,accent,18);
}
)HLSL"

// One parametric ribbon rig, with continuous pose and motif transport across its scenes.
R"HLSL(
bool isRibbonRig(float world){return sceneRig[(int)world].y==2;}
float3 ribbonPose(float world){return abs(world-22)<.1?float3(.38,.72,.012):abs(world-23)<.1?float3(1.570796,.12,.019):float3(0,0,0);}
float rigGesture(float clock,float lane,float position,float seed){
 return lerp(sculptureMotif(authoredMotif(floor(clock),seed),lane,position,tempoPhase*.392699),sculptureMotif(authoredMotif(floor(clock)+1,seed),lane,position,tempoPhase*.392699),motifBlend(clock));
}
float3 composedRibbon(float2 p,float world,float pressure,float snareHit,float hatHit){
 bool bridge=isRibbonRig(scene)&&isRibbonRig(previousScene)&&abs(scene-previousScene)>.1;
 float blend=bridge?liquidEase(transition):1;
 float3 pose=bridge?lerp(ribbonPose(previousScene),ribbonPose(scene),blend):ribbonPose(world);
 float turnBridge=bridge&&(abs(scene-23)<.1||abs(previousScene-23)<.1)?1:0;
 if(turnBridge>.5)pose.x=lerp(ribbonPose(previousScene).x,ribbonPose(scene).x,liquidEase(saturate((blend-.18)/.64)));
 float variant=fmod(floor(patternSeed/16),4),oldVariant=fmod(floor(previousPatternSeed/16),4);
 // Arrangement is part of the same pose, so it also has a continuous handoff.
 variant=bridge?lerp(oldVariant,variant,blend):variant;
 float settle=sqrt(saturate(identityWeight));
 float2 q=rotate2(p,pose.x*(1-settle));q*=1+variant*.055;
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float3 c=float3(.004,.009,.024);float clock=tempoPhase*.125;
 [loop]for(int lane=0;lane<11;++lane){
  float l=lane-5;
  float gesture=rigGesture(clock,l,q.x,patternSeed);
  if(bridge)gesture=lerp(rigGesture(clock,l,q.x,previousPatternSeed),gesture,blend);
  float bankArc=.5+.5*sin((floor(clock)+motifBlend(clock))*.785398+variant*.45);
  float road=sin(q.x*(2.1+variant*.15)-tempoPhase*.098175)*pose.y*(.18+.12*bankArc);
  float path=l*.070+gesture*.034+sin(q.x*2.5-tempoPhase*.19635+l*.28)*.018+road;
  path=lerp(path,l*.079,settle);
  float width=lerp(.015+pose.z+.007*(.5+.5*cos(q.x*3+l*.5-tempoPhase*.3927)),.044,settle);
  float d=q.y-path,aa=max(.001,1.2/resolution.y),face=1-smoothstep(width-aa,width+aa,abs(d));
  // Monument ends rise and fall continuously; racetrack lanes remain unbroken.
  float monument=smoothstep(.6,1.5,pose.x);
  float span=lerp(2,.26+.10*gesture+.045*cos(l*.5-tempoPhase*.19635),monument*(1-settle));
  // Gather the long strips before a quarter-turn. Otherwise an infinite bright
  // ribbon sweeps the entire display during a short musical handoff.
  float gather=liquidEase(saturate(blend/.18))*(1-liquidEase(saturate((blend-.82)/.18)));
  span=lerp(span,.24,gather*turnBridge);
  face*=1-smoothstep(span-.008,span+.008,abs(q.x));
  float chase=.5+.5*cos(q.x*5-l*.65-tempoPhase*.785398);
  float3 hue=formationColor(primary,accent,spectral,l,tempoPhase,chase);
  float metallic=.38+.62*saturate(.5-d/max(width,.001));
  c+=face*hue*(.12+.23*metallic+.07*chase+.018*pressure);
  c+=face*exp(-abs(abs(d)-width)*400)*lerp(hue,accent,.5)*(.07+.025*snareHit+.02*hatHit);
 }
 c+=exp(-abs(p.y-.40)*65)*primary*.026;
 return c+materialIdentity(p,0,.82*logoScale,primary,accent,18);
}
)HLSL"

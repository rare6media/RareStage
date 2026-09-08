// Scene handoffs use shared moving surfaces. No spatial old/new selection or wipe seam.
R"HLSL(
float2 handoffFlow(float2 p,float amount){
 float travel=tempoPhase*.16;
 float2 flow=float2(sin(p.y*4+travel),sin(p.x*3-travel*.7));
 if(transitionStyle>.5&&transitionStyle<1.5)flow=float2(sin(p.y*5+travel),cos(p.x*4+travel));
 else if(transitionStyle>1.5&&transitionStyle<2.5)flow=float2(-p.y,p.x)*1.3;
 else if(transitionStyle>2.5)flow=p*(.6+.25*sin(travel));
 return flow*.045*amount;
}
float2 categoryFlow(float2 p,float world){
 if(abs(world-2)<.1||abs(world-17)<.1||abs(world-19)<.1)return rotate2(p,.055*sin(tempoPhase*.19635))-p;
 if(abs(world-1)<.1||abs(world-12)<.1||abs(world-14)<.1||abs(world-16)<.1||abs(world-20)<.1)return p*.055;
 return float2(sin(p.y*4+tempoPhase*.19635)*.025,sin(p.x*3-tempoPhase*.19635)*.020);
}
float4 ps(O i):SV_TARGET{
 float2 p=(i.uv-.5)*float2(resolution.x/resolution.y,1);
 return float4(renderWorld(p,scene,patternSeed,morphPad0),1);
}
float4 psMorph(O i):SV_TARGET{
 float2 aspect=float2(resolution.x/resolution.y,1),p=(i.uv-.5)*aspect;
 float phase=liquidEase(transition),arc=sin(phase*3.141592);arc*=arc;
 float2 sourcePoint=p,destinationPoint=p;
 float weight=phase,shade=1;
 if(isPanelWorld(previousScene)){
  WallSurface surface=wallSurface(p,previousScene,previousPatternSeed,phase);
  destinationPoint=surface.materialPoint+handoffFlow(surface.materialPoint,arc);
  weight=phase*surface.face;
 }else if(isPanelWorld(scene)){
  WallSurface surface=wallSurface(p,scene,patternSeed,1-phase);
  sourcePoint=surface.materialPoint+handoffFlow(surface.materialPoint,arc);
  weight=1-(1-phase)*surface.face;
 }else{
  // Non-wall worlds retain their own topology. Align composition anchors, then
  // carry both materials through a continuous category-shaped deformation.
  float2 sourceAnchor=float2(0,abs(previousScene-20)<.1?-.16:0);
  float2 destinationAnchor=float2(0,abs(scene-20)<.1?-.16:0);
  float2 anchor=lerp(sourceAnchor,destinationAnchor,phase);
  float2 q=p-anchor;
  float2 flow=lerp(categoryFlow(q,previousScene),categoryFlow(q,scene),phase)*arc;
  sourcePoint=p+sourceAnchor-anchor+flow;
  destinationPoint=p+destinationAnchor-anchor+flow;

 }
 float3 source=morphSource.Sample(linearSampler,sourcePoint/aspect+.5).rgb;
 float3 destination=postScene.Sample(linearSampler,destinationPoint/aspect+.5).rgb;
 return float4(lerp(source,destination,weight)*shade,1);
}
)HLSL"

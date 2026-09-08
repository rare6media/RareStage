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
  WallSurface carrier=wallSurface(p,3,patternSeed,1-arc*.65);
  sourcePoint=destinationPoint=carrier.materialPoint+handoffFlow(carrier.materialPoint,arc);
  shade=lerp(1,.72+.28*carrier.face,arc);
 }
 float3 source=morphSource.Sample(linearSampler,sourcePoint/aspect+.5).rgb;
 float3 destination=postScene.Sample(linearSampler,destinationPoint/aspect+.5).rgb;
 return float4(lerp(source,destination,weight)*shade,1);
}
)HLSL"

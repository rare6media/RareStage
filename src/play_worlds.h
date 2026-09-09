// Twenty persistent pieces, six compositions. All motion follows the continuing
// musical clock; percussion changes surface light, never an object's position.
R"HLSL(
struct PlayPose {float2 center,size;float angle,roundness,fold;};
float3 kineticPlay(float2 p,float world){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float aspect=resolution.x/resolution.y;float2 canvas=float2(max(.25,aspect*.48),.47);
 float2 uv=p/canvas;float t=tempoPhase*.19634954;
 float3 c=float3(.002,.004,.010)+lerp(spectral,primary,.5+.5*sin(uv.x*2+t*.25))*.008;
 float halo=exp(-dot(uv,uv)*1.8);c+=spectral*halo*.012;
 float settle=liquidEase(identityWeight),aa=max(.0008,1.2/resolution.y);
 [loop]for(int i=0;i<20;++i){
  PlayPose pose;float4 geometry=playPoseData[i*2],style=playPoseData[i*2+1];pose.center=geometry.xy;pose.size=geometry.zw;pose.angle=0;pose.roundness=style.z;pose.fold=style.w;
  float2 grid=float2((fmod(i,5)-2)*.39,(floor(i/5)-1.5)*.49);
  float2 center=pose.center*canvas,size=pose.size*canvas;
  // Long surfaces use screen-relative width; rounded tokens keep physical proportions.
  float circular=pose.roundness*(1-settle);size.x=lerp(size.x,pose.size.x*min(canvas.x,canvas.y),circular);
  float2 bounds=float2(abs(style.x)*size.x+abs(style.y)*size.y,abs(style.y)*size.x+abs(style.x)*size.y);
  if(any(abs(p-center)>bounds+.10))continue;
  float2 offset=p-center;float2 local=float2(style.x*offset.x+style.y*offset.y,-style.y*offset.x+style.x*offset.y);float radius=min(size.x,size.y)*lerp(.08,.96,pose.roundness);
  float d=roundedSdf(local,size,radius),face=1-smoothstep(-aa,aa,d);
  float2 shadowOffset=offset-float2(.012,.016);float2 shadowLocal=float2(style.x*shadowOffset.x+style.y*shadowOffset.y,-style.y*shadowOffset.x+style.x*shadowOffset.y);
  float shadow=exp(-max(0,roundedSdf(shadowLocal,size,radius))*140);
  c*=1-shadow*.22;
  float chase=.5+.5*cos(i*.314159-t*2),stripe=.5+.5*sin(i*.55-t*.3);
  float3 hue=lerp(primary,spectral,smoothstep(.35,.65,stripe));hue=lerp(hue,accent,pow(chase,12)*.9);hue=pow(max(hue,0),1.9);
  float facet=lerp(.19,.44,smoothstep(-aa,aa,local.y+local.x*.32))*lerp(1,.85,pose.fold);
  float bevel=exp(-max(0,-d)*150)*(.045+.035*snare+.022*percussion);
  float3 material=hue*(facet+.07*chase)+hue*bevel;
  float inlay=exp(-abs(local.y+local.x*.32)*350)*pose.fold*face;material+=hue*inlay*.07;
  [branch]if(settle>.000001){
   float2 identityPoint=p+(pose.center-grid)*canvas*(1-settle);
   float mask=sampleChoreographyIdentity(identityTransport(identityPoint,(1-settle)*.35,identityProgram()),0,.82*logoScale,0);
   material=lerp(material,material*.30+lerp(primary,accent,.6)*mask*1.1,settle);
  }
  c=lerp(c,material,face);
 }
 return max(c,0);
}
)HLSL"

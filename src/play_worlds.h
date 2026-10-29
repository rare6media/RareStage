// Twenty persistent pieces, six compositions. All motion follows the continuing
// musical clock; percussion changes surface light, never an object's position.
R"HLSL(
struct PlayPose {float2 center,size;float angle,roundness,fold;};
// Curvature, facet contrast, directional grain and hue shift. These are stylized
// surface responses, blended with the same phase as the moving pieces.
float4 playMaterialProfile(float world){
 if(world>=34)return float4(.04,1,0,.50);
 if(world>=33)return float4(.18,.25,.5,.10);
 if(world>=32)return float4(.8,.25,.65,.20);
 if(world<26.5)return float4(1,.10,0,.16);
 if(world<27.5)return float4(.14,1,0,.65);
 if(world<28.5)return float4(.20,.32,0,.04);
 if(world<29.5)return float4(.08,.58,1,.12);
 if(world<30.5)return float4(1,.04,0,0);
 return float4(.12,.48,.65,.04);
}
float3 playShape(float world){return float3(world==32,world==33,world==34);}
float3 kineticPlay(float2 p,float world){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float aspect=resolution.x/resolution.y;float2 canvas=float2(max(.25,aspect*.48),.47);
 float2 uv=p/canvas;float t=tempoPhase*.19634954;
 float3 c=float3(.002,.004,.010)+lerp(spectral,primary,.5+.5*sin(uv.x*2+t*.25))*.008;
 float halo=exp(-dot(uv,uv)*1.8);c+=spectral*halo*.012;
 float settle=liquidEase(identityWeight),aa=max(.0008,1.2/resolution.y);
 float4 finish=playMaterialProfile(world);float3 shape=playShape(world);
 if(previousScene>=26&&previousScene<35&&abs(world-previousScene)>.1)shape=lerp(playShape(previousScene),shape,liquidEase(transition));
 shape*=1-settle;
 if(previousScene>=26&&previousScene<35&&abs(world-previousScene)>.1)finish=lerp(playMaterialProfile(previousScene),finish,liquidEase(transition));
 // Solid drive links travel with their shafts, underneath the moving gears.
 if(shape.x>.000001){[loop]for(int shaft=0;shaft<19;++shaft){if(shaft%5==4)continue;
  float2 a=playPoseData[shaft*2].xy*canvas,b=playPoseData[(shaft+1)*2].xy*canvas;
  if(any(p<min(a,b)-.024)||any(p>max(a,b)+.024))continue;
  float2 ab=b-a;float along=saturate(dot(p-a,ab)/max(dot(ab,ab),.00001));float distance=length(p-a-ab*along);
  float body=1-smoothstep(.009,.017,distance),edge=exp(-abs(distance-.012)*320);
  float carried=.5+.5*sin(along*6.283185-tempoPhase*.392699+shaft*.3);
  c=lerp(c,lerp(primary,accent,carried)*(.095+.08*carried)+spectral*edge*.06,body*shape.x);
 }}
 [loop]for(int i=0;i<20;++i){
  PlayPose pose;float4 geometry=playPoseData[i*2],style=playPoseData[i*2+1];pose.center=geometry.xy;pose.size=geometry.zw;pose.angle=0;pose.roundness=style.z;pose.fold=style.w;
  float2 grid=float2((fmod(i,5)-2)*.39,(floor(i/5)-1.5)*.49);
  float2 center=pose.center*canvas,size=pose.size*canvas;
  // Long surfaces use screen-relative width; rounded tokens keep physical proportions.
  float circular=pose.roundness*(1-settle);size.x=lerp(size.x,pose.size.x*min(canvas.x,canvas.y),circular);
  float2 bounds=float2(abs(style.x)*size.x+abs(style.y)*size.y,abs(style.y)*size.x+abs(style.x)*size.y);
  if(any(abs(p-center)>bounds+.10))continue;
  float2 offset=p-center;float2 local=float2(style.x*offset.x+style.y*offset.y,-style.y*offset.x+style.x*offset.y);float radius=min(size.x,size.y)*lerp(.08,.96,pose.roundness);
  float d=roundedSdf(local,size,radius);
  if(shape.x>.000001){float2 gear=local/max(size,.0001);float teeth=.86+.09*smoothstep(-.45,.45,cos(atan2(gear.y,gear.x)*12));float cog=(length(gear)-teeth)*min(size.x,size.y);d=lerp(d,cog,shape.x);}
  if(shape.z>.000001){float kite=(abs(local.x)/size.x+abs(local.y)/size.y-1)*min(size.x,size.y)*.72;d=lerp(d,kite,shape.z);}
  float face=1-smoothstep(-aa,aa,d);
  float2 shadowOffset=offset-float2(.012,.016);float2 shadowLocal=float2(style.x*shadowOffset.x+style.y*shadowOffset.y,-style.y*shadowOffset.x+style.x*shadowOffset.y);
  float shadow=exp(-max(0,roundedSdf(shadowLocal,size,radius))*140);
  c*=1-shadow*.22;
  if(face<=0)continue;
  float signal=playPatternSignals[i/4][i%4],band=smoothstep(.40,.88,signal);
  float3 hue=formationColor(primary,accent,spectral,floor(i/5)+fmod(i,5)*.3,tempoPhase,signal);hue=pow(max(hue,0),1.7);
  float2 surface=local/max(size,.0001);
  float dome=sqrt(saturate(1-dot(surface,surface)*.65));
  float curved=saturate(.45+dome*.48-surface.x*.18-surface.y*.23);
  float facet=lerp(.25,.48,smoothstep(-aa,aa,local.y+local.x*.32));
  float diffuse=lerp(.29+.18*curved,facet,finish.y);
  diffuse=lerp(diffuse,.20+.32*curved,finish.x);
  // Broad reflections and bands stay readable at a distance; no noisy microtexture.
  float reflection=pow(saturate(1-abs(surface.x*.40+surface.y*.65+.28)),10);
  float grain=.5+.5*cos(surface.y*18+surface.x*2);
  diffuse*=1-finish.z*.07*(1-grain);
  float3 tinted=lerp(hue,lerp(hue,spectral*.55,.32+.20*surface.x),finish.w*.45);
  float bevel=exp(-max(0,-d)*120)*(.045+.025*snare+.018*percussion);
  float3 material=tinted*(diffuse+.11*band)+hue*bevel;
  material+=lerp(hue,float3(.55,.63,.72),.24)*reflection*(.13+.12*finish.x+.06*finish.z);
  material+=accent*band*.045;
  if(shape.x>.000001){float shaft=length(surface);material=lerp(material,hue*(.20+.20*reflection),shape.x*(1-smoothstep(.22,.25,shaft)));material+=accent*shape.x*exp(-abs(shaft-.35)*90)*.13;}
  if(shape.y>.000001){float track=exp(-abs(abs(surface.y)-.64)*35);material+=hue*shape.y*track*.12;material*=1-shape.y*.20*smoothstep(.74,.85,abs(surface.x));}
  if(shape.z>.000001){material*=1-shape.z*.34*step(0,surface.x)*step(0,surface.y);material+=spectral*shape.z*exp(-abs(surface.x+surface.y)*55)*.055;}
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

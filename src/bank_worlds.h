// Blades and fins share eighteen material pieces. Pose, shape and bevel evolve
// together, avoiding two overlapping images during these two native handoffs.
R"HLSL(
float4 bankPose(float world,float index,float side,float seed,bool old){
 float3 modes=phraseModes(seed,old),shape=phraseShapeFor(old);
 float aspect=resolution.x/resolution.y;float4 pose;
 if(abs(world-8)<.1){
  float lane=(index-4)*.625;
  float3 a=bladePose(modes.x,lane,side,aspect),b=bladePose(modes.y,lane,side,aspect);
  pose=float4(lerp(a,b,modes.z),1);pose.z+=1.570796;
 }else{
  float u=(index-4)/4;
  pose=lerp(finPose(modes.x,u,side,aspect*.50),finPose(modes.y,u,side,aspect*.50),modes.z);
  pose.xy+=float2(side*.022*sin(tempoPhase*.098175),fmod(floor(seed/16),4)*.006*side);
 }
 // Bounded development changes articulation rather than speeding up the clock.
 pose.xy*=shape.y;pose.x+=side*shape.z*.10*sin(tempoPhase*.19635+index*.35);
 pose.z+=(shape.x-1)*side*.45*sin(tempoPhase*.19635+index*.22);
 return pose;
}
float3 composedBanks(float2 p,float world,float pressure,float snareHit,float hatHit){
 bool bridge=sceneRig[(int)scene].y==3&&sceneRig[(int)previousScene].y==3&&abs(scene-previousScene)>.1;
 float blend=bridge?liquidEase(transition):1;
 float sourceWorld=bridge?previousScene:world;
 float sourceSeed=bridge?previousPatternSeed:patternSeed;
 float fin=lerp(abs(sourceWorld-10)<.1?1:0,abs(world-10)<.1?1:0,blend);
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float3 c=ensembleBackdrop(p,lerp(spectral,primary,fin),lerp(primary,spectral,fin),tempoPhase*.098175);
 float aspect=resolution.x/resolution.y;
 float variant=lerp(fmod(floor(sourceSeed/16),4),sceneVariant(),blend);
 [loop]for(int bank=0;bank<2;++bank){float side=bank==0?-1:1;
  [loop]for(int piece=0;piece<9;++piece){
   float4 a=bankPose(sourceWorld,piece,side,sourceSeed,bridge),b=bankPose(world,piece,side,patternSeed,false);
   float4 pose=lerp(a,b,blend);
   float2 q=rotate2(p-pose.xy,pose.z);
   float width=lerp(.033+.002*variant,min(.14,aspect*.095),fin);
   float extent=lerp(aspect*(.285+.018*sin((piece-4)*.40625-tempoPhase*.19635)),(.22+.015*variant)*pose.w,fin);
   float2 slab=float2(q.x,q.y-q.x*2.1);
   float2 leaf=float2(q.x+q.y*.18*side,q.y);
   float2 material=lerp(slab,leaf,fin);
   float taper=lerp(1,max(0,1-abs(material.y)/extent),fin);
   float d=max(abs(material.x)-width*taper,abs(material.y)-extent);
   float aa=max(.0012,1.3/resolution.y),face=1-smoothstep(-aa,aa,d);
   c*=1-exp(-max(d,0)*70)*(1-face)*.25;
   float3 hue=lerp(bank==0?primary:accent,spectral,.06+.06*sin((piece-4)*.45+tempoPhase*.098175));
   float ridge=material.x+material.y*.10*fin;
   float facet=lerp(.25,.65,smoothstep(-aa,aa,ridge));
   float spine=exp(-abs(ridge)*450);
   float chase=.5+.5*cos((piece-4)*.75-tempoPhase*.785398+bank*3.141592);
   float3 surface=hue*(facet+.04*chase+.008*pressure)+accent*spine*.08;
   float edge=1-smoothstep(aa,aa+.006,-d);
   surface+=hue*edge*(.035+.035*snareHit+.025*hatHit);
   c=lerp(c,surface,face);
  }
 }
 return c+materialIdentity(p,0,.82*logoScale,primary,accent,8);
}
)HLSL"

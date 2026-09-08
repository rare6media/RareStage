// Embedded HLSL. These scenes have bounded per-pixel work and no simulation buffers.
R"HLSL(
float wallPattern(float kind, float2 id, float2 center, float travel) {
 kind=fmod(kind,8);
 if(kind<1) return bandChase(id.y+sin(id.x*.42)*1.5,travel,.85);
 if(kind<2) return bandChase(id.x+id.y,travel,.64);
 if(kind<3) return .5+.5*cos(length(center)*17-travel*.85);
 if(kind<4) return bandChase(abs(id.x)+id.y*.35,travel,.78);
 if(kind<5) return .5+.5*sin(id.x*.57+sin(id.y*.62-travel*.2)*2-travel*.5);
 if(kind<6) return .5+.5*cos(atan2(center.y,center.x)*3-length(center)*9+travel*.45);
 if(kind<7) return bandChase(floor(id.x*.5)+floor(id.y*.5),travel*.65,1.1);
 return .5+.5*cos(id.y*.72+sin(id.x*.37+travel*.2)*2+travel*.72);
}
float3 choreographyWall(float2 p,float world,float3 primary,float3 accent,float3 spectral,
                       float pressure,float snareHit,float hatHit,float energy) {
 // Each world keeps its own scale and movement; all share the surface formation contract.
 float2 gridScale=world<1?float2(8,7):world<4?float2(10,9):world<10?float2(12,9):
                  world<12?float2(22,15):world<14?float2(13,9):float2(16,10);
 float2 grid=p*gridScale,id=floor(grid),q=frac(grid)-.5,center=(id+.5)/gridScale;
 float clock=tempoPhase*.125,blend=phraseMix(clock),bank=floor(clock)+patternSeed;
 float signal=lerp(wallPattern(bank,id,center,tempoPhase*.62),
                   wallPattern(bank+1,id,center,tempoPhase*.62),blend);
 float support=.28+.72*easeBand(signal); // No cue can collapse the wall into two lit cells.
 float settle=liquidEase(identityWeight);
 float2 group=floor(id/float2(3,2));
 float roll=sin((floor(clock)+blend)*1.570796+dot(group,float2(.36,.48)));
 float travel=sin(blend*3.141592)*(1-settle);
 q.y-=travel*(1-signal)*(.05+(world>12&&world<14?.09:.025));
 q.x-=travel*sin(group.y+tempoPhase*.24)*.025;
 float turn=(sin(time*.16+dot(group,float2(.72,-.56)))*.09+roll*(.13+.4*(1-signal)))*(1-settle);
 float width=lerp(.39*(.38+.62*abs(cos(turn))),.493,settle);
 float height=lerp(.38,.493,settle);
 float2 local=rotate2(q,world>8&&world<10?turn*.55:0);
 float face=roundedFill(local,float2(width,height),lerp(.035,.004,settle),.006);
 float edge=roundedRing(local,float2(width,height),.02,.0035)*face;
 float sideWidth=.018+.11*abs(sin(turn));
 float side=fillBox(q-float2((turn>=0?1:-1)*(width+sideWidth*.5),.02),float2(sideWidth*.5,height*.92),.008)*(1-settle);
 // Sample at the actual face position, never once per macro panel. The source is carried
 // by the turning panel and clipped by its surface, so it cannot become a caption overlay.
 float2 surface=center+local/float2(max(width,.1),height)*.5/gridScale;
 float2 drift=float2(sin(center.y*9+tempoPhase*.21),cos(center.x*8-tempoPhase*.18))*.085*(1-settle);
 float size=min(.88*logoScale,.92);
 float mask=sampleChoreographyIdentity(surface+drift,float2(0,0),size,0);
 float2 micro=frac((local/float2(width,height)*.5+.5)*float2(6,5))-.5;
 float microTurn=sin(time*.32+dot(id,float2(.7,.4))+tempoPhase*.2)*(.18+.38*signal)*(1-settle);
 float pixels=fillBox(rotate2(micro,microTurn),lerp(float2(.32,.29),float2(.435,.435),settle),.015);
 float3 hue=lerp(spectral,primary,.5+.5*sin(id.x*.34-id.y*.26+time*.11));
 float3 c=float3(.004,.008,.019)+hue*(.015+.016*support);
 float pulse=.14*pressure*(.4+.6*signal)+.045*snareHit;
 float light=.05+.15*support+pulse;
 c=lerp(c,hue*light+float3(.006,.012,.025),face);
 c+=side*hue*(.05+.08*support)+edge*lerp(hue,accent,.25)*(.045+.1*hatHit+.06*snareHit);
 float formation=settle*mask;
 c+=face*pixels*lerp(primary,accent,.3)*(.045*support*(1-settle)+formation*(.82+.26*pressure+.12*snareHit));
 c+=face*formation*lerp(primary,accent,.5)*.035;
 c+=face*(1-settle)*exp(-abs(center.y-sin(tempoPhase*.3)*.45)*12)*accent*(.045+.07*energy);
 return c;
}
float valueNoise(float2 p) {
 float2 cell=floor(p),f=frac(p);f=f*f*(3-2*f);
 return lerp(lerp(hash(cell),hash(cell+float2(1,0)),f.x),
             lerp(hash(cell+float2(0,1)),hash(cell+1),f.x),f.y);
}
float turbulence(float2 p) {
 float result=0,amplitude=.55;
 [unroll]for(int octave=0;octave<4;++octave){result+=amplitude*valueNoise(p);p=mul(float2x2(1.6,-1.2,1.2,1.6),p)+3.7;amplitude*=.48;}
 return result;
}
float3 emberWorld(float2 p,float pressure,float snareHit,float hatHit,float energy) {
 float clock=time*.72+tempoPhase*.022,horizon=.37,height=horizon-p.y;
 float2 q=float2(p.x*6,height*3);
 float warp=turbulence(float2(q.x*.8,q.y-clock*.8));
 float flame=turbulence(float2(q.x+(warp-.5)*2,q.y*1.7-clock*2.1));
 float fine=turbulence(float2(q.x*2.6+(warp-.5)*3,q.y*2.3-clock*3.1));
 float heat=flame*.72+fine*.28-height*(.65-.10*pressure);
 float plume=smoothstep(.06,.36,heat)*smoothstep(-.008,.03,height);
 float core=pow(saturate((heat-.18)*3.3),2)*plume;
 float threads=pow(saturate(1-abs(fine-flame)*6),5)*plume;
 float3 fire=float3(.82,.045,.001)*plume*.6;
 fire+=float3(1.8,.31,.007)*plume*(.16+.62*fine);
 fire+=float3(2.3,1.25,.27)*core*(.55+.5*pressure)+float3(.8,.22,.02)*threads*.22;
 float smoke=turbulence(p*3+float2(warp,-clock*.13));
 float3 c=float3(.005,.007,.014)+float3(.045,.016,.012)*smoke*exp(-abs(p.y)*3)+fire;
 c+=float3(.08,.019,.002)*exp(-abs(height-.15)*4)*(.25+.75*smoke);
 float sparks=livingDust(p+float2(sin(clock*.2)*.015,clock*.12),clock,38,.93);
 c+=sparks*float3(1.8,.6,.06)*(.25+.7*hatHit)*smoothstep(.4,-.45,p.y);
 float ground=smoothstep(horizon-.008,horizon+.025,p.y);
 float reflected=turbulence(float2(p.x*5,(.64-p.y)*5+clock));
 c=lerp(c,float3(.012,.016,.021)+float3(.55,.10,.015)*reflected*exp(-(p.y-horizon)*12)*(.45+pressure),ground);
 float mask=sampleChoreographyIdentity(p+float2((flame-.5)*.024,0)*(1-identityWeight),float2(0,-.04),.81*logoScale,0);
 c+=mask*identityWeight*float3(1.2,.47,.07)*(.45+.4*flame+.3*snareHit)*(1-ground);
 return c;
}
float3 oceanSurface(float2 p,float clock) {
 float3 result=0;
 // Height and analytical derivatives in one pass; no extra displaced normal samples.
 [unroll]for(int wave=0;wave<7;++wave){
   float frequency=.7*pow(1.62,wave),amplitude=.20*pow(.60,wave);
   float2 direction=float2(cos(wave*2.39),sin(wave*2.39));
   float phase=dot(p,direction)*frequency+clock*sqrt(frequency)*1.3;
   result.x+=sin(phase)*amplitude;
   result.yz+=cos(phase)*amplitude*frequency*direction;
 }
 return result;
}
float3 tidalWorld(float2 p,float pressure,float snareHit,float hatHit,float energy) {
 float horizon=-.12;float clock=time*.55+tempoPhase*.022;
 float sky=exp(-abs(p.y-horizon)*5);
 float3 c=lerp(float3(.007,.016,.051),float3(.13,.28,.39),sky);
 float cloud=turbulence(p*3+float2(time*.013,0));
 c+=float3(.045,.055,.085)*cloud*smoothstep(horizon,-.45,p.y);
 float sun=softDisc(p-float2(.30,-.24),.052,.002);
 c+=sun*float3(.55,.91,1.2);
 if(p.y>horizon) {
   float depth=max(p.y-horizon,.006);
   float2 sea=float2(p.x,1.0)*(1.5/depth);
   float3 surface=oceanSurface(sea,clock);float wave=surface.x;float2 slope=surface.yz;
   float3 normal=normalize(float3(-slope.x*(1.4+pressure),1,-slope.y*(1.4+pressure)));
   float3 view=normalize(float3(-p.x,depth,1));
   float3 halfway=normalize(view+normalize(float3(.32,.45,1)));
   float spec=pow(saturate(dot(normal,halfway)),80);
   float fresnel=.04+.96*pow(1-saturate(dot(normal,view)),5);
   float crest=smoothstep(.16,.32,wave)*smoothstep(.18,.55,length(slope));
   float distanceFade=smoothstep(.006,.085,depth);
   float3 reflected=lerp(float3(.018,.055,.12),float3(.18,.34,.48),saturate(normal.z*.9+.32));
   float3 water=lerp(float3(.003,.045,.065),reflected,fresnel*.84+.16);
   water+=float3(.8,1.05,1.25)*spec*(.65+.35*hatHit);
   water+=float3(.14,.35,.39)*crest*(.17+.16*pressure);
   float caustic=pow(saturate(.5+.5*sin(sea.x*2.3+wave*8+clock)),14);
   water+=float3(.004,.028,.034)*caustic*depth;
   c=lerp(c,water,distanceFade);
   float identity=sampleChoreographyIdentity(float2(p.x,p.y-.16)+slope*.005*(1-identityWeight),float2(0,0),.82*logoScale,0);
   c+=identity*identityWeight*float3(.26,.83,.95)*(.5+.25*spec+.2*snareHit)*distanceFade;
 }
 return c;
}
float3 auroraWorld(float2 p,float pressure,float snareHit,float hatHit,float energy) {
 float clock=time*.055+tempoPhase*.005;
 float3 c=float3(.003,.009,.027);
 float stars=starField(p+float2(time*.0005,0),95,.97);
 c+=stars*float3(.35,.55,.75)*(.4+.5*hatHit);
 [unroll]for(int layer=0;layer<4;++layer){
  float path=-.18+layer*.055+sin(p.x*(2.1+layer*.42)+clock+layer*.81)*(.09+.035*pressure);
  float curtain=exp(-abs(p.y-path)*(12+layer*3));
  float rays=.35+.65*turbulence(float2(p.x*13+layer*4,p.y*.7-clock*2));
  c+=lerp(float3(.015,.55,.27),float3(.16,.09,.6),layer/3.0)*curtain*rays*(.40+.35*energy+.18*snareHit);
 }
 float mountain=.25+valueNoise(float2(p.x*4,3))*.14+valueNoise(float2(p.x*12,9))*.035;
 float land=smoothstep(mountain-.002,mountain+.002,p.y);
 c=lerp(c,float3(.004,.011,.019),land);
 float identity=sampleChoreographyIdentity(p,float2(0,-.06),.81*logoScale,0);
 c+=identity*identityWeight*float3(.13,.64,.43)*(.52+.12*sin(p.x*24+clock*4))*(1-land);
 return c;
}
)HLSL"

// Rebuilt performance worlds. Fixed loop counts, continuous tempo-based structure.
R"HLSL(
float sceneVariant(){return fmod(floor(patternSeed/16),4);}
float musicalShape(float lane,float spatial){
 float clock=tempoPhase*.125,variant=sceneVariant();
 return sculptSignal(clock,lane,spatial+variant*.6);
}
float3 reliefWave(float2 p,float pressure,float snareHit,float hatHit){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float3 c=float3(.004,.012,.026);
 // Opaque contour terraces: each layer is a broad surface with a crest and shaded face.
 [loop]for(int layer=0;layer<10;++layer){
  float lane=layer-4.5,shape=musicalShape(lane,p.x);
  float ridge=-.34+layer*.078+sin(p.x*(2.1+sceneVariant()*.3)-tempoPhase*.19635+layer*.38)*.09+shape*.023;
  float d=p.y-ridge,aa=max(.001,1.25/resolution.y);
  float face=smoothstep(-aa,aa,d);
  float stratum=exp(-max(d,0)*22);
  float3 hue=formationColor(primary,accent,spectral,lane,tempoPhase,.5+.5*shape);
  c=lerp(c,hue*(.035+.15*stratum+.025*pressure)+float3(.004,.009,.018),face);
  c+=exp(-abs(d)*300)*hue*(.09+.055*snareHit+.03*hatHit);
 }
 return c+materialIdentity(p,0,.82*logoScale,primary,accent,4);
}
float3 signalPerformance(float2 p,float pressure,float snareHit,float hatHit){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float3 c=float3(.004,.010,.024);
 // Every lane belongs to a repeating musical sequence; there are no hashed hit selectors.
 [loop]for(int laneIndex=0;laneIndex<9;++laneIndex){
  float lane=laneIndex-4,shape=musicalShape(lane,0);
  float2 a=float2(lane*.035,-.34),b=float2(lane*.20,.56);
  float2 pa=p-a,ba=b-a;float along=saturate(dot(pa,ba)/dot(ba,ba));
  float side=length(pa-ba*along),width=.009+along*.026;
  float aa=max(.001,1.1/resolution.y),face=1-smoothstep(width-aa,width+aa,side);
  float note=.5+.5*cos(along*12-tempoPhase*.785398-lane*1.570796);
  float phrase=.5+.5*shape;
  float3 hue=formationColor(primary,accent,spectral,lane,tempoPhase,phrase);
  c+=face*hue*(.065+.20*pow(note,3)+.05*phrase+.025*pressure);
  c+=face*pow(note,8)*accent*(.055+.07*snareHit+.04*hatHit);
 }
 // A moving solid cross-bank gives the lanes a physical destination.
 float y=.27+.045*sin(tempoPhase*.19635);
 c+=roundedFill(p-float2(0,y),float2(.73,.017),.01,.002)*primary*.14;
 return c+materialIdentity(p,float2(0,-.02),.81*logoScale,primary,accent,14);
}
float3 voidPerformance(float2 p,float pressure,float snareHit,float hatHit){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float3 c=float3(.004,.007,.020);float r=length(p),angle=atan2(p.y,p.x);
 float variant=sceneVariant(),petals=6+variant*2;
 float opening=.19+.035*sin(tempoPhase*.19635),shape=musicalShape(0,angle);
 float iris=opening+.026*cos(angle*petals-tempoPhase*.19635)+shape*.008;
 float face=smoothstep(iris-.002,iris+.002,r)*(1-smoothstep(.46,.54,r));
 float blade=.5+.5*cos(angle*petals+sin(r*9-tempoPhase*.19635));
 float sector=frac((angle+tempoPhase*.049087)*petals/6.283185);
 float seam=smoothstep(.008,.04,sector)*(1-smoothstep(.96,.992,sector));
 face*=seam;
 float chrome=.25+.75*saturate(sector*1.1);
 float3 hue=formationColor(primary,accent,spectral,angle*2,tempoPhase,blade);
 c+=face*hue*(.10+.38*chrome+.022*pressure);
 c+=face*exp(-abs(sector-.06)*90)*accent*(.06+.06*hatHit);
 c+=exp(-abs(r-iris)*280)*accent*(.12+.055*snareHit+.035*hatHit);
 c+=exp(-abs(r-.45)*140)*primary*.07;
 return c+materialIdentity(p,0,.72*logoScale,primary,accent,17);
}
float3 auroraPerformance(float2 p,float pressure,float snareHit,float hatHit){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float3 c=float3(.003,.013,.029);
 // Foreground ridges, a reflective basin and broad auroral curtains fill the world.
 [loop]for(int curtain=0;curtain<5;++curtain){
  float lane=curtain-2,shape=musicalShape(lane,p.x);
  float path=-.24+curtain*.085+sin(p.x*(2+curtain*.3)-tempoPhase*.098175+lane)*.09+shape*.02;
  float d=p.y-path,width=.038+.014*(.5+.5*shape);
  float body=exp(-abs(d)/width),rays=.35+.65*(.5+.5*sin(p.x*30+sin(p.x*8+tempoPhase*.098175)*3+lane));
  float hanging=exp(-max(d,0)*10)*smoothstep(-.002,.02,d)*(1-smoothstep(.17,.27,d));
  body+=hanging*.35;
  float3 hue=formationColor(primary,accent,spectral,lane,tempoPhase,.5+.5*shape);
  c+=body*rays*hue*(.24+.09*pressure)+exp(-abs(d)*180)*hue*.035;
  c+=exp(-abs(p.y-(.38-path*.2))*100)*hue*.028;
 }
 [loop]for(int ridge=0;ridge<3;++ridge){
  float horizon=.24+ridge*.085+sin(p.x*(3+ridge)+tempoPhase*.049087+ridge)*.048+sin(p.x*11-ridge)*.018;
  float ground=smoothstep(horizon-.001,horizon+.001,p.y);
  c=lerp(c,lerp(primary,spectral,ridge*.35)*(.022+.018*(2-ridge))+float3(.002,.006,.014),ground*.88);
  c+=exp(-abs(p.y-horizon)*240)*primary*(.11+.035*snareHit);
  float contour=.5+.5*cos((p.y-horizon)*180+p.x*4-tempoPhase*.19635);
  c+=ground*pow(contour,10)*primary*.018;
 }
 c+=starField(p+float2(time*.0004,0),65,.982)*accent*(.08+.05*hatHit);
 return c+materialIdentity(p,float2(0,-.08),.80*logoScale,primary,accent,7);
}
float3 pixelPerformance(float2 p,float pressure,float snareHit,float hatHit){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float3 c=float3(.005,.012,.028);float aspect=resolution.x/resolution.y;
 float size=min(.285*logoScale,aspect*.72);float2 bounds=max(.01,float2(aspect*.5,.5)-size*.5-.012);
 float2 fallback=float2(sin(time*.13),sin(time*.097+1));
 float2 pos=(dvdControlled>.5?float2(dvdX,dvdY):fallback)*bounds;
 // Audio lives in the circuit field and waves around the travelling mark.
 float2 grid=p*float2(14,10),id=floor(grid),local=frac(grid)-.5;
 float signal=.5+.5*musicalShape(id.x,id.y*.3);
 float face=roundedFill(local,float2(.34,.29),.045,.015);
 float3 hue=formationColor(primary,accent,spectral,id.x+id.y*.5,tempoPhase,signal);
 c+=face*hue*(.022+.07*signal+.06*pressure*signal);
 float ring=.5+.5*cos(length(p-pos)*22-tempoPhase*.785398);
 c+=hue*pow(ring,10)*(.014+.035*snareHit)*exp(-length(p-pos)*1.6);
 float border=roundedRing(p,float2(aspect*.5-.018,.482),.018,.003);
 c+=border*accent*(.04+.06*hatHit+.08*dvdCorner);
 float4 mark=sampleRareLogo(p,pos,size,0);
 // Fixed position/scale/opacity response. No kick deformation, size pumping or hashed hue jumps.
 compositeLogo(c,mark,.92,.045);
 c+=exp(-length(p-pos)*12)*primary*.014;
 return c;
}
)HLSL"

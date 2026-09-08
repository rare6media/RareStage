// Embedded HLSL. These scenes have bounded per-pixel work and no simulation buffers.
R"HLSL(
float authoredMotif(float stepIndex,float seed){
 float n=fmod(stepIndex,16),offset=0;
 if(n<2)offset=0;else if(n<3)offset=1;else if(n<4)offset=8;
 else if(n<5)offset=1;else if(n<6)offset=9;else if(n<7)offset=0;else if(n<8)offset=8;
 else if(n<10)offset=2;else if(n<11)offset=3;else if(n<12)offset=10;
 else if(n<13)offset=3;else if(n<14)offset=11;else if(n<15)offset=2;else offset=10;
 return fmod(seed+offset,16);
}
float wallPattern(float kind,float2 id,float2 center,float phase){
 kind=fmod(kind,16);
 if(kind<1)return .5+.5*cos(id.y*.82-phase);
 if(kind<2)return .5+.5*cos(abs(id.x)*.70+id.y*.78-phase);
 if(kind<3)return .5+.5*cos(length(center)*17-phase);
 if(kind<4)return .5+.5*cos(abs(id.x)*.85-phase);
 if(kind<5)return .5+.5*cos(id.y*.65+sin(id.x*.6-phase*.5)*1.8-phase);
 if(kind<6)return .5+.5*cos(atan2(center.y,center.x)*3+length(center)*9-phase);
 if(kind<7)return .5+.5*cos((floor(id.x*.5)+floor(id.y*.5))*3.141592-phase);
 if(kind<8)return .5+.5*cos((abs(id.x)-abs(id.y))*.78-phase);
 if(kind<9)return .5+.5*cos(id.x*.82-phase);
 if(kind<10)return .5+.5*cos(abs(id.y)*.92-phase);
 if(kind<11)return .5+.5*cos(floor(id.x/3)*1.570796+id.y*.55-phase);
 if(kind<12)return .5+.5*cos(max(abs(center.x)*.65,abs(center.y))*24-phase);
 if(kind<13)return .5+.5*cos(id.x*.78+sin(id.y*.65-phase)*1.5);
 if(kind<14)return .5+.5*cos(id.y*.78+cos(id.x*3.141592)*phase);
 if(kind<15)return .5+.5*cos((abs(id.x)+abs(id.y))*.78-phase);
 return .5+.5*cos(id.x*.5-phase)*cos(id.y*.7+phase);
}
float motifBlend(float clock){return liquidEase(smoothstep(.48,.99,frac(clock)));}
bool isPanelWorld(float world){return abs(world)<.1||abs(world-3)<.1||abs(world-9)<.1||abs(world-11)<.1||abs(world-13)<.1||abs(world-15)<.1;}
float2 wallScale(float world){return world<1?float2(8,7):world<4?float2(10,9):world<10?float2(12,9):world<12?float2(22,15):world<14?float2(13,9):float2(16,10);}
float stageStyle(float world){return isPanelWorld(world)&&abs(world-11)>.1&&abs(world-13)>.1?1:0;}
float rowGesture(float bank,float row,float clock){
 bank=fmod(bank,16);
 if(bank<1)return sin(row*.48-clock*.785398)*1.25;
 if(bank<2)return sin(row*.64+clock*.785398)*cos(row*1.570796)*1.65;
 if(bank<3)return sin(row*.38-clock*.785398)*1.8;
 if(bank<4)return sin(clock*.392699)*sin(row*.24)*2.2;
 if(bank<5)return sin(row*.78-clock*.785398)*1.4;
 if(bank<6)return sin(row*.32+clock*.392699)*2;
 if(bank<7)return sin(row*.52-clock*.785398)*cos(clock*.196350)*1.8;
 if(bank<8)return sin(row*.45+clock*.785398)*1.6;
 if(bank<9)return sin(row*.3+clock*.392699)*.18;
 if(bank<10)return sin(abs(row)*.6-clock*.785398)*2;
 if(bank<11)return sin(floor(row*.5)*1.570796+clock*.785398)*1.75;
 if(bank<12)return sin(row*.4-clock*.392699)*.6;
 if(bank<13)return sin(row*.7-clock*.785398)*.3;
 if(bank<14)return cos(row*3.141592)*sin(clock*.785398)*1.8;
 if(bank<15)return sin(abs(row)*.5-clock*.785398)*1.1;
 return sin(row*.8+clock*.785398)*1.3;
}
float columnGesture(float bank,float column,float phase){
 bank=fmod(bank,16);
 float amount=bank>=8?1:0;
 return amount*sin(column*.65-phase*.785398)*(bank<9?.85:bank<11?.35:bank<13?.75:.45);
}
float3 formationColor(float3 primary,float3 accent,float3 spectral,float lane,float phase,float signal){
 float colorWave=.5+.5*sin(lane*.48-phase*.1963495);
 float3 base=lerp(primary,spectral,smoothstep(.28,.76,colorWave));
 return lerp(base,accent,smoothstep(.60,.97,signal)*(.3+.32*colorWave));
}
struct WallSurface {float2 center,local,scale,materialPoint,id;float width,height,face,edge,side,signal,pixels,stage,join;};
WallSurface wallSurface(float2 p,float world,float seed,float opening){
 WallSurface g;
 float morph=liquidEase(transition);
 bool familyMorph=isPanelWorld(scene)&&isPanelWorld(previousScene)&&abs(scene-previousScene)>.1;
 g.scale=familyMorph?lerp(wallScale(previousScene),wallScale(scene),morph):wallScale(world);
 g.stage=familyMorph?lerp(stageStyle(previousScene),stageStyle(scene),morph):stageStyle(world);
 float clock=tempoPhase*.125,blend=motifBlend(clock),bank=authoredMotif(floor(clock),seed),nextBank=authoredMotif(floor(clock)+1,seed);
 float cascadeAmount=familyMorph?lerp(1-step(.1,abs(previousScene-13)),1-step(.1,abs(scene-13)),morph):1-step(.1,abs(world-13));
 float arrayAmount=familyMorph?lerp(1-step(.1,abs(previousScene-9)),1-step(.1,abs(scene-9)),morph):1-step(.1,abs(world-9));
 float settle=max(sqrt(saturate(identityWeight)),liquidEase(opening));
 float2 grid=p*g.scale;
 // Inverse-map the entire infinite row before selecting a cell. Travel across cell
 // boundaries therefore carries geometry continuously instead of clipping each cube.
 grid.y-=sin(p.x*2.1-tempoPhase*.19)*.32*g.stage*(1-settle);
 float column=floor(grid.x);
 float columnOffset=lerp(columnGesture(bank,column,tempoPhase),columnGesture(nextBank,column,tempoPhase),blend);
 if(familyMorph){float oldBank=authoredMotif(floor(clock),previousPatternSeed),oldNextBank=authoredMotif(floor(clock)+1,previousPatternSeed);columnOffset=lerp(lerp(columnGesture(oldBank,column,tempoPhase),columnGesture(oldNextBank,column,tempoPhase),blend),columnOffset,morph);}
 grid.y-=columnOffset*g.stage*(1-settle);
 float row=floor(grid.y);
 float offset=lerp(rowGesture(bank,row,tempoPhase),rowGesture(nextBank,row,tempoPhase),blend);
 if(familyMorph){float oldBank=authoredMotif(floor(clock),previousPatternSeed),oldNextBank=authoredMotif(floor(clock)+1,previousPatternSeed);float oldOffset=lerp(rowGesture(oldBank,row,tempoPhase),rowGesture(oldNextBank,row,tempoPhase),blend);offset=lerp(oldOffset,offset,morph);}
 grid.x-=offset*g.stage*(1-settle);
 g.id=floor(grid);g.local=frac(grid)-.5;g.center=(g.id+.5)/g.scale;
 g.signal=lerp(wallPattern(bank,g.id,g.center,tempoPhase*.785398),wallPattern(nextBank,g.id,g.center,tempoPhase*.785398),blend);
 if(familyMorph){float oldBank=authoredMotif(floor(clock),previousPatternSeed),oldNextBank=authoredMotif(floor(clock)+1,previousPatternSeed);float oldSignal=lerp(wallPattern(oldBank,g.id,g.center,tempoPhase*.785398),wallPattern(oldNextBank,g.id,g.center,tempoPhase*.785398),blend);g.signal=lerp(oldSignal,g.signal,morph);}
 g.signal=lerp(g.signal,.5,saturate(identityWeight)*.65);
 float2 group=floor(g.id/float2(3,2));
 float roll=sin((floor(clock)+blend)*1.570796+dot(group,float2(.36,.48)));
 float travel=sin(blend*3.141592)*(1-settle)*(1-g.stage);
 g.local.y-=travel*(1-g.signal)*(.075+.065*cascadeAmount);
 g.local.x-=travel*sin(group.y+tempoPhase*.24)*.025;
 float turn=(sin(time*.16+dot(group,float2(.72,-.56)))*.09+roll*(.13+.4*(1-g.signal)))*(1-settle);
 // Neighboring cubes meet along bright traveling bands, becoming a single ribbon.
 g.join=g.stage*smoothstep(.40,.87,g.signal)*(1-settle);
 g.width=lerp(lerp(.39*(.38+.62*abs(cos(turn))),.5,g.join),.493,settle);
 g.height=lerp(.38,.493,settle);
 g.width=lerp(g.width,.5,opening);g.height=lerp(g.height,.5,opening);
 g.local=rotate2(g.local,arrayAmount*turn*.55);
 float radius=lerp(.035,.004,settle);
 float aa=max(.004, .6*g.scale.y/resolution.y);
 g.face=roundedFill(g.local,float2(g.width,g.height)+opening*.008,radius,aa);
 // AA grows with output pixel size. The terminal material must still cover every
 // pixel: expanding by a fixed epsilon leaves translucent seams at low resolution.
 g.face=lerp(g.face,1,liquidEase(opening));
 g.edge=roundedRing(g.local,float2(g.width,g.height),radius,.0035)*g.face*(1-opening);
 float sideWidth=(.018+.11*abs(sin(turn)))*(1-g.join);
 g.side=fillBox(g.local-float2((turn>=0?1:-1)*(g.width+sideWidth*.5),.02),float2(sideWidth*.5,g.height*.92),.008)*(1-settle);
 g.materialPoint=g.center+g.local/float2(max(g.width,.1),g.height)*.5/g.scale;
 float2 micro=frac((g.local/float2(g.width,g.height)*.5+.5)*float2(6,5))-.5;
 float microTurn=sin(time*.32+dot(g.id,float2(.7,.4))+tempoPhase*.2)*(.18+.38*g.signal)*(1-settle);
 g.pixels=fillBox(rotate2(micro,microTurn),lerp(float2(.32,.29),float2(.435,.435),settle),.015);
 return g;
}
float3 choreographyWall(float2 p,float world,float3 primary,float3 accent,float3 spectral,
                       float pressure,float snareHit,float hatHit,float energy,float seed,float opening) {
 WallSurface g=wallSurface(p,world,seed,opening);
 float support=.28+.72*easeBand(g.signal),settle=liquidEase(identityWeight);
 float2 drift=float2(sin(g.center.y*9+tempoPhase*.21),cos(g.center.x*8-tempoPhase*.18))*.085*(1-settle);
 float mask=sampleChoreographyIdentity(g.materialPoint+drift,0,min(.88*logoScale,.92),0);
 float3 hue=lerp(spectral,primary,.5+.5*sin(g.id.x*.34-g.id.y*.26+time*.11));
 // Stagewall uses one main color and a coordinated accent band, with clean cube faces.
 float3 stageHue=lerp(formationColor(primary,accent,spectral,g.id.y+g.id.x*.3,tempoPhase,g.signal),primary,sqrt(saturate(identityWeight))*.7);
 hue=lerp(hue,stageHue,g.stage);
 float band=smoothstep(.48,.89,g.signal);
 float pulse=.14*pressure*(.4+.6*g.signal)+.045*snareHit;
 float light=lerp(.05+.15*support,.065+.23*band,g.stage)+pulse;
 float3 c=float3(.004,.008,.019)+hue*(.015+.016*support);
 c=lerp(c,hue*light+float3(.006,.012,.025),g.face);
 c+=g.side*hue*(.05+.08*support)+g.edge*lerp(hue,accent,.25)*(.045+.1*hatHit+.06*snareHit);
 float bevel=glassHighlight(g.local,float2(g.width,g.height))*g.face*(1-settle);
 c+=bevel*lerp(primary,accent,.3)*g.stage*(.04+.05*pressure);
 c+=g.face*g.stage*band*accent*(.065+.1*pressure)*(1-settle);
 float formation=settle*mask;
 c+=g.face*g.pixels*lerp(primary,accent,.3)*(.045*support*(1-settle)*(1-g.stage)+formation*(.82+.26*pressure+.12*snareHit));
 c+=g.face*formation*lerp(primary,accent,.5)*.035;
 c+=g.face*(1-settle)*(1-g.stage)*exp(-abs(g.center.y-sin(tempoPhase*.3)*.45)*12)*accent*(.045+.07*energy);
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
float architecturalPattern(float index,float lane,float depth,float clock){
 index=fmod(index,16);
 if(index<1)return .5+.5*cos(lane*.8-clock);
 if(index<2)return .5+.5*cos(lane*.8+depth*4-clock);
 if(index<3)return .5+.5*cos(abs(lane)*.9-clock);
 if(index<4)return .5+.5*sin(lane*.6+sin(depth*5-clock*.3)*2);
 if(index<5)return .5+.5*cos(depth*8-clock);
 if(index<6)return .5+.5*cos(lane*1.570796+clock*.785398);
 if(index<7)return .5+.5*cos(abs(lane)*.55+depth*5+clock);
 if(index<8)return .5+.5*sin(lane*.4-depth*6-clock);
 if(index<9)return .5+.5*cos(lane*.8+clock);
 if(index<10)return .5+.5*cos(floor(lane*.5)*1.570796-clock);
 if(index<11)return .5+.5*cos(abs(lane)*.7-depth*8-clock);
 if(index<12)return .5+.5*cos(depth*12+clock);
 if(index<13)return .5+.5*cos(lane*.7+sin(depth*8-clock)*2);
 if(index<14)return .5+.5*cos(lane*3.141592)*sin(clock);
 if(index<15)return .5+.5*cos(abs(lane)*.5+depth*8-clock);
 return .5+.5*cos(lane*.7-clock)*cos(depth*9+clock);
}
float3 prismAtrium(float2 p,float pressure,float snareHit,float hatHit,float energy){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float clock=tempoPhase*.125,blend=motifBlend(clock),bank=authoredMotif(floor(clock),patternSeed),nextBank=authoredMotif(floor(clock)+1,patternSeed);
 float3 c=float3(.004,.009,.023);
 // Nested architectural ribs have continuous travel and birth/death at the far planes.
 [unroll]for(int layer=0;layer<8;layer++){
  float z=frac(layer/8.0-tempoPhase*.0078125);
  float size=.14+z*z*1.5;
  float turn=sin(tempoPhase*.12+layer*.35)*.09;
  float2 q=rotate2(p,turn)/size;
  float ring0=max(abs(q.x)*.72,abs(q.y));
  float ring1=(abs(q.x)*.64+abs(q.y))*.72;
  float shape=lerp(ring0,ring1,.5+.5*sin((floor(clock)+blend)*.785398));
  float rib=lineGlow(shape-.43,.005+.003*pressure);
  float life=smoothstep(0,.12,z)*(1-smoothstep(.88,1,z));
  float lane=atan2(q.y,q.x)*4;
  float signal=lerp(architecturalPattern(bank,lane,z,tempoPhase*.785398),architecturalPattern(nextBank,lane,z,tempoPhase*.785398),blend);
  float3 color=formationColor(primary,accent,spectral,layer*1.3,tempoPhase,signal);
  c+=rib*life*color*(.055+.25*signal+.13*pressure);
  c+=exp(-abs(shape-.43)*36)*life*color*(.013+.05*signal);
  float brackets=pow(saturate(.5+.5*cos(lane*2)),12)*rib;
  c+=brackets*life*accent*(.04+.16*hatHit+.09*snareHit);
 }
 float2 floorP=float2(p.x,p.y-.30);
 float runway=exp(-abs(floorP.x)*5)*lineGlow(floorP.y,.004);
 c+=runway*accent*(.07+.14*pressure);
 float identity=sampleChoreographyIdentity(p,0,.82*logoScale,0);
 c+=identity*identityWeight*lerp(primary,accent,.35)*(.58+.17*snareHit);
 return c;
}
float3 pulseCauseway(float2 p,float pressure,float snareHit,float hatHit,float energy){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float clock=tempoPhase*.125,blend=motifBlend(clock),bank=authoredMotif(floor(clock),patternSeed),nextBank=authoredMotif(floor(clock)+1,patternSeed);
 float3 c=float3(.004,.010,.025);
 // Sorted far-to-near slots. Attributes follow continuous depth, never a wrapping
 // instance index. Recycling changes order only while opacity is zero at both ends.
 float travel=frac(tempoPhase*.0625);
 [unroll]for(int tier=0;tier<8;tier++){
  float z=(tier+travel)/8.0;
  float size=.13+z*z*1.85;
  float2 q=p/size;
  float tierTravel=sin(z*5-tempoPhase*.392699)*(.42+.18*sin((floor(clock)+blend)*.785398));
  float grid=q.x*6-tierTravel,lane=floor(grid),x=frac(grid)-.5;
  float signal=lerp(architecturalPattern(bank,lane,z,tempoPhase*.785398),architecturalPattern(nextBank,lane,z,tempoPhase*.785398),blend);
  // Geometry uses the beat clock, not the kick envelope. Hits illuminate edges.
  float lift=.08+.18*signal;
  float life=smoothstep(0,.16,z)*(1-smoothstep(.80,1,z));
  float aa=max(.012,7.0/(resolution.y*size));
  float2 block=float2(x,(q.y-(.33-lift))*7);
  float2 extent=float2(.44,.27+signal*.17);
  float face=roundedFill(block,extent,.035,aa);
  float edge=(1-smoothstep(aa,aa*2,abs(roundedSdf(block,extent,.035))));
  float3 hue=formationColor(primary,accent,spectral,lane+z*5,tempoPhase,signal);
  c=lerp(c,hue*(.085+.19*signal+.018*pressure),face*life);
  c+=edge*life*lerp(hue,accent,.4)*(.045+.035*hatHit+.045*snareHit);
  float rail=exp(-abs(q.y-.37)*100)*life;
  c+=rail*hue*(.035+.035*signal);
  float ceiling=roundedFill(float2(x,(q.y+.32-lift*.34)*7),float2(.42,.11+.13*signal),.03,aa);
  c+=ceiling*life*hue*(.045+.10*signal+.012*pressure);
 }
 float identity=sampleChoreographyIdentity(p,float2(0,-.05),.82*logoScale,0);
 c+=identity*identityWeight*lerp(primary,accent,.35)*(.58+.05*snareHit);
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

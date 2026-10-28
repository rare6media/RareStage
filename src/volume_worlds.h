// Volume worlds: one ray per pixel through a choreographed stage of analytic solids.
// Choreography leads. Ten named programs move the extruded Rare mark, twelve panels and the
// camera on the musical clock; flat 2D graphics sit on the panels and behind the stage at
// different depths so camera motion gives them parallax. Audio only lights the result.
R"HLSL(
bool isVolumeWorld(float world){return world>41.5&&world<46.5;}
static const float volTau=6.283185;
float volStep(float clock){return floor(clock)+liquidEase(saturate((frac(clock)-.08)/.72));}
// A swell, not a hit: eases up over a third of a beat and back to rest before the next one.
float volPulse(float clock){float f=frac(clock);return smoothstep(0,.36,f)*(1-smoothstep(.22,1,f));}
float volWrap(float a){return a-volTau*round(a/volTau);}
float3 volRotX(float3 v,float a){float c=cos(a),s=sin(a);return float3(v.x,c*v.y-s*v.z,s*v.y+c*v.z);}
float3 volRotY(float3 v,float a){float c=cos(a),s=sin(a);return float3(c*v.x+s*v.z,v.y,-s*v.x+c*v.z);}
float3 volRotZ(float3 v,float a){float c=cos(a),s=sin(a);return float3(c*v.x-s*v.y,s*v.x+c*v.y,v.z);}
// Roll about the piece's own axis first, then turn it, then tilt the whole arrangement.
float3 volToWorld(float3 v,float3 a){return volRotX(volRotY(volRotZ(v,a.z),a.y),a.x);}
float3 volToLocal(float3 v,float3 a){return volRotZ(volRotY(volRotX(v,-a.x),-a.y),-a.z);}
bool volBox(float3 ro,float3 rd,float3 h,out float t0,out float t1){
 float3 safe=rd+(step(0,rd)*2-1)*1e-5,inv=1/safe;
 float3 a=(-h-ro)*inv,b=(h-ro)*inv,lo=min(a,b),hi=max(a,b);
 t0=max(max(lo.x,lo.y),lo.z);t1=min(min(hi.x,hi.y),hi.z);
 return t1>max(t0,0);
}
)HLSL"
R"HLSL(
// Twelve panels. u runs 0..1 around the group; every pose is a continuous function of beats,
// built from eased steps so a move arrives, holds, and leaves without a visible seam.
void volTile(int program,int i,float bt,out float3 pos,out float3 ang,out float sc){
 float u=(i+.5)/12.,a,e;pos=0;ang=0;sc=1;
 if(program==0){ // TURNTABLE RING: a ring turns in 30 degree steps around the mark
  a=u*volTau+volStep(bt/2)*.5236;
  pos=volRotX(float3(cos(a)*1.75,sin(a*2+bt*.1)*.08,sin(a)*1.20+.25),.62);ang=float3(.62,1.5708-a,0);
 }else if(program==1){ // PENDULUM HALO: an upright ring swings like a pendulum
  float swing=sin(bt*.3927)*.58;a=u*volTau+volStep(bt/2)*.2618;
  pos=volRotY(float3(cos(a)*1.5,sin(a)*.86,.15),swing);ang=float3(0,swing,a);
 }else if(program==2){ // TWIN HELIX: two strands climb and fade at the ends
  int strand=i%2;float m=frac((i/2+.5)/6.+bt/48.);float phi=m*volTau*1.25+strand*3.14159+volStep(bt/4)*.5236;
  pos=float3(cos(phi)*1.28,(m-.5)*2.1,sin(phi)*.95+.30);ang=float3(0,1.5708-phi,0);
  sc=smoothstep(0,.12,m)*(1-smoothstep(.88,1,m));
 }else if(program==3){ // CROWN FAN: a fan opens over the mark and closes again
  float open=.5-.5*cos(bt/16*volTau);float th=(u-.5)*lerp(.75,2.2,open);
  pos=float3(sin(th)*1.42,.46-cos(th)*1.08,.62);ang=float3(0,0,th);sc=1.12;
 }else if(program==4){ // DIAGONAL CASCADE: staggered panels tumble through the frame
  float ph=frac(bt/8-u*.55);e=liquidEase(ph);
  pos=lerp(float3(-1.8,-1.05,1.5),float3(1.8,1.05,-.75),e)+float3(0,sin(ph*volTau)*.12,0);
  ang=float3(e*volTau*.75,e*1.2-.6,e*volTau*.5);sc=smoothstep(0,.1,ph)*(1-smoothstep(.9,1,ph));
 }else if(program==5){ // COUNTER ORBITS: two tilted rings turn against each other and press on the beat
  int ring=i%2;float dir=ring?-1:1,tilt=ring?.65:-.65;
  a=((i/2+.5)/6.)*volTau*dir+volStep(bt/2)*.5236*dir*.5;float radius=(1.30+.18*ring)*(1-.09*volPulse(bt));
  pos=volRotX(float3(cos(a)*radius,0,sin(a)*radius),tilt);ang=float3(tilt,1.5708-a,0);
 }else if(program==6){ // SPIRAL GATHER: panels spiral in to a halo, then release
  float g=liquidEase(.5-.5*cos(bt/16*volTau));float radius=lerp(1.75,.95,g);a=u*volTau+(1-g)*2.4;
  pos=float3(cos(a)*radius*1.22,sin(a)*radius*.88,lerp(.9*sin(a*1.5),.30,g));ang=float3(0,0,a);sc=lerp(.82,1.05,g);
 }else if(program==7){ // WAVE WALL: a grid behind the mark turns over in a travelling wave
  int col=i%4,row=i/4;float wave=sin(bt*.7854-col*.9+row*.6);e=liquidEase(saturate(wave*1.6+.5));
  pos=float3((col-1.5)*1.0,(row-1)*.80,.85+.12*wave);ang=float3(e*3.14159,0,0);
 }else if(program==8){ // FIGURE EIGHT: panels follow one lemniscate through depth
  float t=u*volTau+bt/32*volTau;
  pos=float3(sin(t)*1.58,sin(2*t)*.46,cos(t)*.95+.25);ang=float3(0,atan2(cos(t)*1.58,-sin(t)*.95),sin(2*t)*.5);
 }else{ // HALO SETTLE: everything answers into one calm ring
  a=u*volTau+bt*.01;pos=float3(cos(a)*1.30,sin(a)*.88,.28+.05*sin(bt*.5+a*3));ang=float3(0,0,a);sc=1+.05*sin(bt*.39+u*volTau);
 }
}
// A handover staggers the pieces and routes each one out and back, so nothing cuts straight
// through the mark or through another piece.
void volTileBlend(int pa,int pb,float w,int i,float bt,out float3 pos,out float3 ang,out float sc){
 bool blend=w>.001&&pa!=pb;float3 p0=0,a0=0,p1=0,a1=0;float s0=1,s1=1;
 // One loop body, so the program table is compiled once rather than twice.
 [loop]for(int k=0;k<2;++k){float3 pp,aa;float ss;volTile(k?pb:pa,i,bt,pp,aa,ss);
  if(k==0){p0=pp;a0=aa;s0=ss;}else{p1=pp;a1=aa;s1=ss;}if(!blend)break;}
 pos=p0;ang=a0;sc=s0;
 if(blend){float u=(i+.5)/12.,e=liquidEase(saturate(w*1.7-u*.7)),arc=sin(e*3.14159);
  float3 outward=normalize(float3(p0.x+p1.x,(p0.y+p1.y)*.6,0)+float3(.001,.001,0));
  pos=lerp(p0,p1,e)+outward*arc*.55+float3(0,0,arc*.6);
  ang=float3(a0.x+volWrap(a1.x-a0.x)*e,a0.y+volWrap(a1.y-a0.y)*e,a0.z+volWrap(a1.z-a0.z)*e);sc=lerp(s0,s1,e)*(1-.2*arc);}
}
// The mark: turn, tilt, roll and push per program. Three programs add a full turn every 64 beats.
void volLogo(int program,float bt,out float3 ang,out float push,out float bob){
 float yaw=0,pitch=0,roll=0,turnGain=0;push=0;bob=0;
 if(program==0){yaw=.55*sin(bt/16*volTau);pitch=.10*sin(bt/8*volTau);turnGain=1;}
 else if(program==1){yaw=.18*sin(bt/8*volTau);pitch=.20*sin(bt/8*volTau+1.2);roll=.06*sin(bt/16*volTau);push=.08;}
 else if(program==2){yaw=.30*sin(bt/16*volTau);pitch=.12*sin(bt/16*volTau+.8);roll=.22*sin(bt/32*volTau);}
 else if(program==3){yaw=.25*sin(bt/8*volTau);pitch=.05*sin(bt/4*volTau);push=.06;}
 else if(program==4){yaw=.40*sin(bt/8*volTau+.9);pitch=.18*sin(bt/8*volTau);roll=.10*sin(bt/8*volTau+.4);}
 else if(program==5){yaw=.20*sin(bt/4*volTau);pitch=.08*sin(bt/4*volTau+.7);push=.26*volPulse(bt);}
 else if(program==6){yaw=.70*sin(bt/16*volTau);pitch=.15*sin(bt/16*volTau+.5);push=.10;turnGain=1;}
 else if(program==7){yaw=.15*sin(bt/8*volTau);pitch=.08*sin(bt/8*volTau+.6);}
 else if(program==8){yaw=.60*sin(bt/32*volTau);pitch=.20*sin(bt/16*volTau);roll=.12*sin(bt/32*volTau+1.0);turnGain=1;}
 else{yaw=.05*sin(bt/16*volTau);pitch=.03*sin(bt/16*volTau+.5);}
 float turn=liquidEase(saturate((frac(bt/64)*64-58)/6))*volTau*turnGain;
 ang=float3(pitch,yaw+turn,roll);bob=-.035*sin(bt/4*volTau);
}
void volLogoBlend(int pa,int pb,float w,float bt,out float3 ang,out float push,out float bob){
 bool blend=w>.001&&pa!=pb;float3 a0=0,a1=0;float u0=0,u1=0,b0=0,b1=0;
 [loop]for(int k=0;k<2;++k){float3 aa;float uu,bb;volLogo(k?pb:pa,bt,aa,uu,bb);
  if(k==0){a0=aa;u0=uu;b0=bb;}else{a1=aa;u1=uu;b1=bb;}if(!blend)break;}
 float e=blend?liquidEase(w):0;
 ang=float3(a0.x+volWrap(a1.x-a0.x)*e,a0.y+volWrap(a1.y-a0.y)*e,a0.z+volWrap(a1.z-a0.z)*e);push=lerp(u0,u1,e);bob=lerp(b0,b1,e);
}
)HLSL"
R"HLSL(
// ---- The extruded mark -------------------------------------------------------------------
// Mask of the extruded shape. The Rare mark by default; the DJ name when a name formation runs.
float volHeroMask(float2 xy,float size,float nameW){
 float2 uv=xy/size+.5;float inside=step(0,uv.x)*step(uv.x,1)*step(0,uv.y)*step(uv.y,1);
 float m=smoothstep(.78,.92,logoTex.SampleLevel(linearSampler,saturate(uv),0).a*inside);
 if(nameW>.001){float2 nuv=xy/float2(size*1.34,size*.34)+.5;float inn=step(0,nuv.x)*step(nuv.x,1)*step(0,nuv.y)*step(nuv.y,1);
  m=lerp(m,smoothstep(.35,.70,identityTex.SampleLevel(linearSampler,saturate(nuv),0).a*inn),nameW);}
 return m;
}
float2 volHeroGradient(float2 xy,float size,float nameW,float e){
 return float2(volHeroMask(xy+float2(e,0),size,nameW)-volHeroMask(xy-float2(e,0),size,nameW),
               volHeroMask(xy+float2(0,e),size,nameW)-volHeroMask(xy-float2(0,e),size,nameW));
}
struct VolHit {float t;float3 normal;float3 local;float face;float edge;};
bool volTraceHero(float3 ro,float3 rd,float3 ang,float3 center,float size,float depth,float nameW,out VolHit hit){
 hit=(VolHit)0;
 float3 o=volToLocal(ro-center,ang),d=volToLocal(rd,ang);
 float3 h=float3(size*lerp(.5,.67,nameW),size*.5,depth*.5);float t0,t1;
 if(!volBox(o,d,h,t0,t1))return false;
 t0=max(t0,0);float3 x=o+d*t0;float nz=d.z>0?-1:1;
 float dt=(t1-t0)/30;float tPrev=t0,tHit=-1;
 if(volHeroMask(x.xy,size,nameW)>.5&&abs(abs(x.z)-h.z)<dt+1e-3)tHit=t0;
 else{
  [loop]for(int k=1;k<=30;++k){float t=t0+dt*k;float3 y=o+d*t;
   if(volHeroMask(y.xy,size,nameW)>.5){float lo=tPrev,hi=t;
    [unroll]for(int j=0;j<4;++j){float mid=(lo+hi)*.5;if(volHeroMask((o+d*mid).xy,size,nameW)>.5)hi=mid;else lo=mid;}
    tHit=hi;break;}
   tPrev=t;}
 }
 if(tHit<0)return false;
 hit.t=tHit;hit.local=o+d*tHit;
 bool onFace=abs(abs(hit.local.z)-h.z)<max(dt*1.6,.004);
 float2 wide=volHeroGradient(hit.local.xy,size,nameW,size*.030);
 hit.edge=saturate(length(wide)*1.6);
 float3 n;
 if(onFace){hit.face=nz<0?1:2;n=normalize(float3(-wide*1.7,nz*(.55+.45*(1-hit.edge))));}
 else{float2 g=volHeroGradient(hit.local.xy,size,nameW,size*.008);n=normalize(float3(-g/max(length(g),1e-4),0));}
 // Local normal to world: undo the local rotation.
 hit.normal=volToWorld(n,ang);
 return true;
}
)HLSL"
R"HLSL(
// ---- Light -------------------------------------------------------------------------------
// Unlit surfaces stay near black. Light arrives as a sharp tempo-travelling band; the kick
// lives on bevelled edges only. A dim key keeps the forms readable between bands.
float3 volLight(float3 albedo,float3 emissive,float3 n,float3 pos,float3 rd,float bevel,float gloss,float bt,float punch,float3 primary,float3 accent,float3 spectral){
 float3 toLight=normalize(float3(-.5,-.75,-.6)),view=-rd;
 float ndl=saturate(dot(n,toLight));float3 hv=normalize(toLight+view);
 float spec=pow(saturate(dot(n,hv)),56)*gloss,fres=pow(1-saturate(dot(n,view)),3);
 float2 axis=normalize(float2(1,-.45));
 float band=directionalLightBand(pos.xy,axis,tempoBandPosition(bt,axis,8,2.3),.24,10);
 float spike=transientSpike(kick,onset)*.55;
 float3 hue=lerp(primary,accent,.4);
 float3 c=clampAmbient(spectral*.06)+albedo*(ndl*.30+.015);
 c+=hue*band*(.30+.70*ndl)*(.25+.75*dot(albedo,float3(.333,.333,.333)))*1.25;
 c+=lerp(primary,float3(1,1,1),.25)*fres*(.10+.40*audioEnergy+.25*spike);
 c+=lerp(float3(1,1,1),primary,.45)*spec*(.30+.9*spike*bevel);
 c+=accent*bevel*spike*.6;
 return c+emissive;
}
float3 volTilePattern(float2 uv,int i,float bt,float3 primary,float3 accent,float3 spectral,out float ink){
 int style=i%4;
 if(style==0){float s=abs(frac((abs(uv.x)*.8-uv.y*.55+bt*.125)*2.2)-.5);ink=1-smoothstep(.12,.17,s);}
 else if(style==1){float s=abs(frac(length(uv*float2(1,.7))*2.4-bt*.125)-.5);ink=1-smoothstep(.10,.15,s);}
 else if(style==2){float2 g=frac(uv*float2(2,3)+float2(0,bt*.0625))-.5;ink=1-smoothstep(.14,.19,length(g));}
 else{float s=abs(frac(uv.y*2.5+bt*.0625*(i%8<4?1:-1))-.5);ink=smoothstep(.18,.22,s)*step(abs(uv.x),.82);}
 ink*=step(max(abs(uv.x),abs(uv.y)),.86);
 int tone=i%3;
 return tone==0?primary:tone==1?accent:lerp(primary,spectral,.6);
}
// Flat graphic layers behind the stage. Each sits at its own depth, so camera yaw shifts them
// by different amounts. Edges are crisp fills, not glows.
float volDust(float2 p,float scale,float seed,float bt){
 float2 g=p*scale,id=floor(g),f=frac(g)-.5;float h=hash(id+seed);
 float2 o=(float2(hash(id*1.7+seed),hash(id*2.3+seed))-.5)*.6;
 return step(.80,h)*(1-smoothstep(0,.07,length(f-o)))*(.55+.45*sin(bt*.5+h*40));
}
float3 volBackdrop(float2 p,float bt,float2 shift,float punch,float3 primary,float3 accent,float3 spectral){
 float aa=1.4/resolution.y;
 float3 c=float3(.0010,.0020,.0055)+lerp(spectral,primary,.5)*.007*(1-smoothstep(.15,1.2,length(p)));
 float2 q=p-shift*.55;float r=length(q),angle=atan2(q.y,q.x);float spin=volStep(bt/2)*.2618;
 [unroll]for(int k=0;k<4;++k){
  float radius=.78+k*.17,width=.0060+.0025*(k%2),dir=(k%2)?-1:1,seg=6+k*2;
  float local=frac((angle-spin*dir*(1+k*.3))/volTau*seg);
  float gate=smoothstep(0,.03,local)*(1-smoothstep(.52+.08*k,.55+.08*k,local));
  float ring=1-smoothstep(0,aa*1.5,abs(r-radius)-width);
  c+=ring*gate*lerp(primary,accent,k*.28)*(.06+.03*punch);
 }
 float2 q2=p-shift*.95;float a2=atan2(q2.y,q2.x),r2=length(q2);
 float wl=frac((a2+volStep(bt/4)*.5236)/volTau*12);
 float wedge=smoothstep(0,.02,wl)*(1-smoothstep(.48,.5,wl))*smoothstep(.95,1.25,r2)*(1-smoothstep(1.5,2.2,r2));
 c+=wedge*primary*.028;
 c+=volDust(p+shift*.3,9,1.3,bt)*primary*.35+volDust(p+shift*.8,5,7.1,bt)*accent*.5;
 return c;
}
float3 volFloor(float3 ro,float3 rd,float bt,float punch,float3 primary,float3 accent,float3 spectral,float floorY,out float t){
 t=(floorY-ro.y)/rd.y;float3 x=ro+rd*t;
 float fade=exp(-max(t-2.6,0)*.30);
 float2 cell=abs(frac(x.xz*1.6-.5)-.5);float w=.022+.010*t;
 float lines=max(1-smoothstep(0,w,cell.x),1-smoothstep(0,w,cell.y));
 float band=directionalLightBand(x.xz,float2(0,1),tempoBandPosition(bt,float2(0,1),8,4),.7,6);
 float pool=exp(-dot(x.xz,x.xz)*.8);
 return (spectral*.004+lerp(primary,accent,.3)*lines*(.03+.30*band+.04*punch)+primary*pool*(.03+.02*punch))*fade;
}
)HLSL"
R"HLSL(
float3 volumeSample(float2 p){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float bt=tempoPhase;
 float4 prog=phraseProgram;float clock=bt/16,seedStep=floor(patternSeed/4);
 int pa=prog.w>.5?(int)prog.x:(int)fmod(floor(clock)+seedStep,10);
 int pb=prog.w>.5?(int)prog.y:(int)fmod(floor(clock)+1+seedStep,10);
 float pw=prog.w>.5?prog.z:liquidEase(saturate((frac(clock)-.75)*4));
 float punch=saturate(kick*.95+bass*.25);
 // Camera: a slow orbit plus a small bass dolly. It never leaves the mark.
 float3 camAng=float3(.045*sin(bt*.19635)+.02*punch,.16*sin(bt*.098175+.6),0);
 float3 ro=volToWorld(float3(0,0,-3.15+.07*punch),camAng);
 float3 rd=volToWorld(normalize(float3(p,1.45)),camAng);
 float3 logoAng;float logoPush,logoBob;volLogoBlend(pa,pb,pw,bt,logoAng,logoPush,logoBob);
 float nameW=saturate(identityKind)*liquidEase(identityWeight);
 float size=1.36*clamp(logoScale,.6,1.5),depth=.17+.05*punch;float3 center=float3(0,logoBob,-logoPush);
 float bestT=1e5;float3 col=0;
 VolHit hit;
 if(volTraceHero(ro,rd,logoAng,center,size,depth,nameW,hit)){
  bestT=hit.t;float3 pos=ro+rd*hit.t;float3 albedo,emissive=0;float gloss=1;
  if(hit.face>.5&&hit.face<1.5){
   float2 uv=hit.local.xy/size+.5;float3 tex=logoTex.SampleLevel(linearSampler,saturate(uv),0).rgb;
   float lum=dot(tex,float3(.2126,.7152,.0722)),chroma=max(tex.r,max(tex.g,tex.b))-min(tex.r,min(tex.g,tex.b));
   float3 tint=lerp(primary,accent,saturate((tex.g-tex.b)*3));
   albedo=lerp(tex,tint*(.30+1.30*lum),saturate(chroma*1.6))*.80+primary*.03;
   emissive=albedo*smoothstep(.2,.8,lum)*(.34+.10*punch);
   float3 nameColor=lerp(primary,accent,.35)*.8;albedo=lerp(albedo,nameColor,nameW);emissive=lerp(emissive,nameColor*(.30+.10*punch),nameW);
  }else{albedo=lerp(spectral,primary,hit.local.z*.5/depth+.5)*.18;gloss=1.4;}
  col=volLight(albedo,emissive,hit.normal,pos,rd,hit.edge,gloss,bt,punch,primary,accent,spectral);
 }
 [loop]for(int i=0;i<12;++i){
  float3 tp,ta;float tsc;volTileBlend(pa,pb,pw,i,bt,tp,ta,tsc);
  if(tsc<.03)continue;
  float3 oc=tp-ro;float tc=dot(oc,rd),d2=dot(oc,oc)-tc*tc,rr=.5*tsc;
  if(tc<0||d2>rr*rr)continue;
  float3 o=volToLocal(ro-tp,ta),d=volToLocal(rd,ta);float3 h=float3(.25,.38,.035)*tsc;float t0,t1;
  if(!volBox(o,d,h,t0,t1)||t0<=0||t0>=bestT)continue;
  bestT=t0;float3 q=o+d*t0;float3 m=abs(q)/h;float3 nl;
  if(m.x>m.y&&m.x>m.z)nl=float3(sign(q.x),0,0);else if(m.y>m.z)nl=float3(0,sign(q.y),0);else nl=float3(0,0,sign(q.z));
  float edge=0;
  if(abs(nl.z)>.5){float e=min(h.x-abs(q.x),h.y-abs(q.y));float bevel=.045*tsc;edge=saturate(1-e/bevel);nl=normalize(float3(sign(q.x)*edge*step(h.x-abs(q.x),h.y-abs(q.y))*.9,sign(q.y)*edge*step(h.y-abs(q.y),h.x-abs(q.x))*.9,nl.z));}
  float3 n=volToWorld(nl,ta),pos=ro+rd*t0;
  float3 albedo=primary*.030,emissive=0;
  if(abs(nl.z)>.5&&edge<.6){
   float ink;float3 base=volTilePattern(q.xy/h.xy,i,bt,primary,accent,spectral,ink);
   if(nl.z<0){albedo=lerp(albedo,base*.50,ink);emissive=base*ink*(.12+.05*punch);}
   else albedo=lerp(albedo,base*.22,ink*.6);
  }
  col=volLight(albedo,emissive,n,pos,rd,edge,1.1,bt,punch,primary,accent,spectral)*exp(-max(t0-3.4,0)*.10);
 }
 if(bestT>=1e5){
  float2 shift=float2(camAng.y,camAng.x)*2.2;
  col=volBackdrop(p,bt,shift,punch,primary,accent,spectral);
  if(rd.y>.02){float tf;float3 fc=volFloor(ro,rd,bt,punch,primary,accent,spectral,1.22,tf);col+=fc;}
 }
 return max(col,0);
}
)HLSL"

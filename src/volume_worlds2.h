// More volume worlds on the same engine. Each has its own cast and its own ten choreographies:
//   43 Card Match    eighteen cards flip, deal, swap and match; faces turn into Rare marks
//   44 Rarebot       a floating robot with an LED face; the mark lives on its chest
//   46 Rarebot Squad clones of Rarebot pop in and out on the drums
//   45 Pillar Choir  forty-nine pillars rise in patterns and can raise themselves into the mark
R"HLSL(
static const float volPi=3.14159265;
struct VolBoxHit {float t;float3 q;float3 nl;float3 n;float edge;};
// Oriented box with a bevel: the normal leans toward the nearest edge so edges catch light.
bool volHitBox(float3 ro,float3 rd,float3 c,float3 ang,float3 h,float bevel,float bestT,out VolBoxHit r){
 r=(VolBoxHit)0;
 float3 o=volToLocal(ro-c,ang),d=volToLocal(rd,ang);float t0,t1;
 if(!volBox(o,d,h,t0,t1)||t0<=0||t0>=bestT)return false;
 float3 q=o+d*t0,m=abs(q)/h,gap=h-abs(q),nl;
 if(m.x>=m.y&&m.x>=m.z)nl=float3(sign(q.x),0,0);else if(m.y>=m.z)nl=float3(0,sign(q.y),0);else nl=float3(0,0,sign(q.z));
 float3 nt=nl;float e;
 if(nl.x!=0){e=min(gap.y,gap.z);if(gap.y<gap.z)nt.y=sign(q.y);else nt.z=sign(q.z);}
 else if(nl.y!=0){e=min(gap.x,gap.z);if(gap.x<gap.z)nt.x=sign(q.x);else nt.z=sign(q.z);}
 else{e=min(gap.x,gap.y);if(gap.x<gap.y)nt.x=sign(q.x);else nt.y=sign(q.y);}
 r.edge=saturate(1-e/bevel);
 nt=nl+(nt-nl)*r.edge*.9;
 r.t=t0;r.q=q;r.nl=nl;r.n=volToWorld(normalize(nt),ang);
 return true;
}
struct VolView {float bt;int pa;int pb;float pw;float punch;float3 camAng;float3 ro;float3 rd;float3 primary;float3 accent;float3 spectral;};
VolView volView(float2 p,float camDist,float pitch){
 VolView v;rarePalette(palette,v.primary,v.accent,v.spectral);
 v.bt=tempoPhase;float4 prog=phraseProgram;float clock=v.bt/16,seedStep=floor(patternSeed/4);
 v.pa=prog.w>.5?(int)prog.x:(int)fmod(floor(clock)+seedStep,10);
 v.pb=prog.w>.5?(int)prog.y:(int)fmod(floor(clock)+1+seedStep,10);
 v.pw=prog.w>.5?prog.z:liquidEase(saturate((frac(clock)-.75)*4));
 v.punch=saturate(kick*.95+bass*.25);   // no easing: the hit lands on the frame it is detected
 v.camAng=float3(pitch+.04*sin(v.bt*.19635)+.012*v.punch,.16*sin(v.bt*.098175+.6),0);
 v.ro=volToWorld(float3(0,0,-camDist+.05*v.punch),v.camAng);
 v.rd=volToWorld(normalize(float3(p,1.45)),v.camAng);
 return v;
}
float3 volBackground(VolView v,float2 p,float floorY){
 float3 col=volBackdrop(p,v.bt,float2(v.camAng.y,v.camAng.x)*2.2,v.punch,v.primary,v.accent,v.spectral);
 if(v.rd.y>.02){float tf;col+=volFloor(v.ro,v.rd,v.bt,v.punch,v.primary,v.accent,v.spectral,floorY,tf);}
 return col;
}
float3 volTone(int tone,float3 primary,float3 accent,float3 spectral){return tone==0?primary:tone==1?accent:lerp(primary,spectral,.6);}
// The Rare mark tinted by the palette, for faces and chests. Returns colour; alpha in a.
float3 volMarkFace(float2 uv,float3 hue,out float a){
 float inside=step(0,uv.x)*step(uv.x,1)*step(0,uv.y)*step(uv.y,1);
 float4 tex=logoTex.SampleLevel(linearSampler,saturate(uv),0);
 float lum=dot(tex.rgb,float3(.2126,.7152,.0722)),chroma=max(tex.r,max(tex.g,tex.b))-min(tex.r,min(tex.g,tex.b));
 a=smoothstep(.5,.85,tex.a)*inside;
 return lerp(tex.rgb,hue*(.3+1.3*lum),saturate(chroma*1.6))*.9;
}
)HLSL"
R"HLSL(
// ---- 43 Card Match ------------------------------------------------------------------------
// Six columns by three rows. Card i matches card i+9. Rest shows the patterned back; a flip shows
// a symbol or a Rare mark. Handovers stagger the cards and arc them over the others.
void volCard(int program,int i,float bt,out float3 pos,out float3 ang,out float sc){
 int col=i%6,row=i/6;float tau=6.283185;
 float3 base=float3((col-2.5)*.60,(row-1)*.74,0);pos=base;ang=0;sc=1;float f;
 if(program==0){ // PAIR REVEAL: each pair turns up together, one pair after another
  float local=fmod(bt/2.-(i%9)+18.,9.);
  f=liquidEase(saturate(local*2.))*(1-liquidEase(saturate((local-1.3)*2.)));
  ang.y=f*volPi;pos.z=-.12*sin(f*volPi);
 }else if(program==1){ // ROW WAVE: a wave of flips runs across the grid
  f=liquidEase(saturate(sin(bt*.7854-col*.8-row*.5)*1.4+.5));
  ang.y=f*volPi;pos.y+=sin(bt*.7854-col*.8)*.04;pos.z=-.08*sin(f*volPi);
 }else if(program==2){ // CHECKER SWAP: two colours of squares turn over in turn
  float ph=bt/4.+((col+row)%2)*.5;f=liquidEase(saturate((frac(ph)-.1)/.4));
  ang.y=fmod((floor(ph)+f)*volPi,tau);pos.z=-.07*sin(f*volPi);
 }else if(program==3){ // SPIRAL FLIP: the turn travels around the grid
  float2 c=float2(col-2.5,(row-1)*1.4);float order=(atan2(c.y,c.x)/tau+.5)*.8+length(c)*.10;
  f=liquidEase(saturate(sin((bt/16.-order)*tau)*1.6+.5));ang.y=f*volPi;pos.z=-.10*sin(f*volPi);
 }else if(program==4){ // CASCADE DEAL: cards fly in from a deck, turn in flight, then gather again
  float s=frac(bt/16.),d=i/18.*.5;
  float arrive=liquidEase(saturate((s-d*.5)/.3)),leave=liquidEase(saturate((s-.72-d*.2)/.25)),present=arrive*(1-leave);
  float3 deck=float3(2.0,.95,.4);pos=lerp(deck,base,present)+float3(0,-sin(present*volPi)*.4,-sin(present*volPi)*.5);
  ang=float3(0,present*volPi,(1-present)*.4);sc=lerp(.7,1,present);
 }else if(program==5){ // MIRROR MATCH: each card turns with its mirror image
  float w=sin(bt*.5236-min(col,5-col)*.7);f=liquidEase(saturate(w*1.5+.5));
  ang.y=(col<3?1:-1)*f*volPi;pos.z=-.12*f;
 }else if(program==6){ // SHUFFLE SWAP: cards trade places, one over and one under
  int j=(i+9)%18;float3 other=float3(((j%6)-2.5)*.60,((j/6)-1)*.74,0);float side=i<9?1:-1;
  float s=liquidEase(saturate(sin(bt*.3927-(col+row)*.3)*1.1+.5));
  pos=lerp(base,other,s)+float3(0,0,-side*sin(s*volPi)*.45);ang.z=side*sin(s*volPi)*.3;
 }else if(program==7){ // FAN DEAL: the grid opens into a hand and turns face up
  float open=.5-.5*cos(bt/16.*tau),th=(i/17.-.5)*lerp(.5,2.6,open);
  float3 fan=float3(sin(th)*1.5,.95-cos(th)*1.55,-.015*i);
  pos=lerp(base,fan,liquidEase(open));ang.z=th*open;ang.y=volPi*liquidEase(saturate((open-.4)*3.));
 }else if(program==8){ // TOWER TURN: columns tip in a wave and turn over behind it
  float w=bt*.3927-col*.6;ang.x=sin(w)*.8;pos.z=sin(w)*.18;
  f=liquidEase(saturate(sin(w+1.0)*1.6+.5));ang.y=f*volPi;
 }else{ // LOGO CHORUS: every card turns to a Rare mark, nods in a wave, and turns back
  float local=frac(bt/16.),d=(col+row)*.03;
  f=liquidEase(saturate((local-.05-d)/.2))*(1-liquidEase(saturate((local-.7-d)/.2)));
  ang.y=f*volPi;ang.x=.12*sin(bt*.7854-col*.5)*f;pos.z=-.06*f;sc=1+.04*f;
 }
}
void volCardBlend(int pa,int pb,float w,int i,float bt,out float3 pos,out float3 ang,out float sc){
 bool blend=w>.001&&pa!=pb;float3 p0=0,a0=0,p1=0,a1=0;float s0=1,s1=1;
 [loop]for(int k=0;k<2;++k){float3 pp,aa;float ss;volCard(k?pb:pa,i,bt,pp,aa,ss);
  if(k==0){p0=pp;a0=aa;s0=ss;}else{p1=pp;a1=aa;s1=ss;}if(!blend)break;}
 pos=p0;ang=a0;sc=s0;
 if(blend){float u=(i+.5)/18.,e=liquidEase(saturate(w*1.8-u*.8)),arc=sin(e*volPi);
  pos=lerp(p0,p1,e)+float3(0,0,-arc*.35*(i%2?1:-1));
  ang=float3(a0.x+volWrap(a1.x-a0.x)*e,a0.y+volWrap(a1.y-a0.y)*e,a0.z+volWrap(a1.z-a0.z)*e);sc=lerp(s0,s1,e);}
}
float3 volCardArt(float2 uv,int sym,float logoAll,float3 primary,float3 accent,float3 spectral,out float ink){
 float3 hue=volTone((sym/3)%3,primary,accent,spectral),c=hue*.05;ink=0;
 if(sym%3==0||logoAll>.5){
  float a;float3 art=volMarkFace(float2(uv.x*.5+.5,uv.y*.696+.5),hue,a);c=lerp(c,art,a);ink=a;
 }else{
  float2 q=float2(uv.x,uv.y*1.39);float d;
  if(sym==1)d=abs(length(q)-.55)-.10;
  else if(sym==2){float v=abs(q.x)*.9+q.y;d=max(min(abs(v-.1),abs(v+.45))-.08,abs(q.x)-.8);}
  else if(sym==4){float2 g=abs(frac(q*1.5+.5)-.5);d=length(g)-.22;}
  else if(sym==5)d=abs(frac(q.y*1.6)-.5)-.2;
  else if(sym==7)d=abs(abs(q.x)+abs(q.y)-.8)-.09;
  else d=min(max(abs(q.x)-.12,abs(q.y)-.8),max(abs(q.x)-.6,abs(q.y)-.12));
  ink=1-smoothstep(0,.05,d);c=lerp(c,hue*.95,ink);
 }
 float frame=1-smoothstep(0,.035,abs(max(abs(uv.x)-.86,abs(uv.y)-.90)));
 return c+hue*frame*.55;
}
float3 volCardsSample(float2 p){
 VolView v=volView(p,3.45,0);
 float logoAll=lerp(v.pa==9?1.:0.,v.pb==9?1.:0.,v.pw);
 float bestT=1e5;float3 col=0;
 [loop]for(int i=0;i<18;++i){
  float3 cp,ca;float csc;volCardBlend(v.pa,v.pb,v.pw,i,v.bt,cp,ca,csc);
  float3 oc=cp-v.ro;float tc=dot(oc,v.rd),d2=dot(oc,oc)-tc*tc,rr=.46*csc;
  if(tc<0||d2>rr*rr)continue;
  VolBoxHit h;if(!volHitBox(v.ro,v.rd,cp,ca,float3(.23,.32,.025)*csc,.04*csc,bestT,h))continue;
  bestT=h.t;float3 pos=v.ro+v.rd*h.t,albedo=lerp(v.primary,v.spectral,.4)*.03,emissive=0;
  if(abs(h.nl.z)>.5&&h.edge<.55){
   float2 uv=h.q.xy/float2(.23,.32)/csc;
   if(h.nl.z>0){ // the face that shows after a turn: art, mirrored back to read correctly
    float ink;float3 art=volCardArt(float2(-uv.x,uv.y),i%9,logoAll>.1+.8*frac(i*.618)?1.:0.,v.primary,v.accent,v.spectral,ink);
    albedo=lerp(albedo,art*.45,.9);emissive=art*(.20+.04*v.punch);
   }else{ // the patterned back
    float ink;float3 base=volTilePattern(uv,i,v.bt,v.primary,v.accent,v.spectral,ink);
    albedo=lerp(albedo,base*.45,ink);emissive=base*ink*(.10+.04*v.punch);
   }
  }
  col=volLight(albedo,emissive,h.n,pos,v.rd,h.edge,1.1,v.bt,v.punch,v.primary,v.accent,v.spectral)*exp(-max(h.t-3.9,0)*.10);
 }
 if(bestT>=1e5)col=volBackground(v,p,1.22);
 return max(col,0);
}
)HLSL"
R"HLSL(
// ---- 44 Rarebot / 46 Rarebot Squad ---------------------------------------------------------
// Rarebot floats on a stack of exhaust rings. His face is a dot-matrix of lamps: eyes blink and
// change mood, the mouth is a nine-bar meter. Drums move him directly (no easing, so the reaction
// is instant) and heavy music adds random twitches and stop-motion stutters on top of the dance.
float volHash1(float n){return frac(sin(n*127.1+31.7)*43758.5453);}
float3 volHash3(float n){return float3(volHash1(n),volHash1(n+17.3),volHash1(n+41.9))*2-1;}
// How hard the music is hitting: instant on the drums, held up by the loudness.
float volHeavy(){return saturate(bass*.45+kick*.45+snare*.30+audioEnergy*.70-.12);}
float volPop(float x){x=saturate(x);return 1-exp(-7*x)*cos(16*x)*(1-x*x*x);}
struct VolRobot {float3 body;float bob;float3 head;float3 shL;float elL;float3 shR;float elR;float glow;};
// Arm angles: x swings forward (negative) or back, z raises outward, el bends the elbow.
VolRobot volRobotPose(int program,float bt){
 VolRobot r=(VolRobot)0;float tau=6.283185;float beatSway=sin(bt*volPi);
 r.bob=-.03*sin(bt*volPi*.5);r.glow=.5;r.shL=float3(0,0,.12);r.shR=float3(0,0,.12);
 if(program==0){ // DJ SCRATCH
  r.shL=float3(-1.15+.28*sin(bt*volPi),0,.22);r.shR=float3(-1.15-.28*sin(bt*volPi),0,.22);r.elL=-.55;r.elR=-.55;
  r.head=float3(.12+.08*beatSway,.15*sin(bt*volPi*.5),0);r.body=float3(.06,.12*sin(bt*volPi*.5),0);
 }else if(program==1){ // ROBOT WAVE
  r.shR=float3(0,0,2.4);r.elR=.7*sin(bt*volPi*1.0);r.shL=float3(0,0,.15);
  r.head=float3(0,-.3+.1*sin(bt*volPi*.5),.05*beatSway);r.body=float3(0,.2*sin(bt*volPi*.25),.05*sin(bt*volPi*.5));
 }else if(program==2){ // ARMS UP DROP
  float pump=.5+.5*sin(bt*volPi);
  r.shL=float3(0,0,2.45+.3*pump);r.shR=float3(0,0,2.45+.3*(1-pump));r.head=float3(-.25,0,0);r.bob=-.08*pump;r.glow=.7+.3*pump;
 }else if(program==3){ // TURNTABLE SPIN
  r.body=float3(0,volStep(bt/4.)*volPi,0);r.shL=float3(0,0,1.45);r.shR=float3(0,0,1.45);r.head=float3(0,-.2*sin(bt*volPi*.5),0);
 }else if(program==4){ // CLAP BUILD
  float m=.5+.5*cos(bt*tau*.5);r.shL=float3(-1.3,0,.15+.5*m);r.shR=float3(-1.3,0,.15+.5*m);r.elL=-.5;r.elR=-.5;r.head=float3(.1*m,0,0);r.glow=.4+.6*(1-m);
 }else if(program==5){ // POINT AND ANSWER
  float a=liquidEase(saturate(sin(bt*volPi*.25)*1.6+.5));
  r.shL=float3(0,0,lerp(.25,2.0,a));r.shR=float3(0,0,lerp(2.0,.25,a));r.body=float3(0,0,(a-.5)*.2);r.head=float3(0,(a-.5)*.7,0);
 }else if(program==6){ // MIRROR SWAY
  float s=sin(bt*volPi*.5);r.shL=float3(0,0,1.2+.35*s);r.shR=float3(0,0,1.2-.35*s);r.elL=.3*s;r.elR=-.3*s;r.body=float3(0,0,.18*s);r.head=float3(0,0,-.1*s);
 }else if(program==7){ // HEAD GROOVE
  r.head=float3(.28*max(0,sin(bt*volPi*2.)),.5*sin(bt*volPi*.5),0);r.shL=float3(-.3+.15*beatSway,0,.25);r.shR=float3(-.3-.15*beatSway,0,.25);r.bob=-.05*abs(sin(bt*volPi));
 }else if(program==8){ // WINDMILL
  float w=volStep(bt/2.)*volPi*.5;r.shL=float3(w,0,.12);r.shR=float3(-w,0,.12);r.head=float3(0,.2*sin(bt*volPi*.25),0);
 }else{ // BOW AND SETTLE
  float bow=liquidEase(saturate(sin(bt*tau/16.)*1.8+.3));r.body=float3(.55*bow,0,0);r.head=float3(.15*bow,0,0);r.shL=float3(.2*bow,0,.2);r.shR=float3(.2*bow,0,.2);
 }
 // Overlapping action: forearms and head trail the beat so no pose moves as one block.
 float lag=sin(bt*volPi-1.1);
 r.elL+=.20*lag;r.elR-=.20*lag;r.head.z+=.05*sin(bt*volPi*.5-.6);r.body.z+=.03*lag;
 return r;
}
// Heavy music: on every eighth note the limbs snap to a new random twitch. Drum hits are added
// straight from the detectors, so a kick nods the head and a snare flicks the arms the instant it lands.
VolRobot volRobotSpasm(VolRobot r,float bt,float seed,float heavy){
 float clock=bt*2+seed*3.7,n=floor(clock),snap=1-exp(-frac(clock)*18);
 float live=step(.30,volHash1(n*1.31+seed));
 float3 tw=lerp(volHash3(n-1+seed*11),volHash3(n+seed*11),snap)*heavy*(.45+.55*live);
 float3 tw2=lerp(volHash3(n+5.5+seed*11),volHash3(n+6.5+seed*11),snap)*heavy*(.45+.55*live);
 r.head+=float3(tw.x*.55,tw.y*.9,tw.z*.6);r.body+=float3(tw2.x*.18,tw2.y*.7,tw2.z*.15);
 r.shL+=float3(tw2.z*1.4,0,tw.x*.9);r.shR+=float3(tw2.x*1.4,0,-tw.z*.9);
 r.elL+=tw.z*1.1;r.elR+=tw2.y*1.1;r.bob+=tw2.z*.06;
 r.head.x+=.30*kick;r.body.x-=.10*kick;r.bob+=.05*kick;
 r.shL.z+=.70*snare;r.shR.z+=.70*snare;r.elL-=.35*snare;r.elR-=.35*snare;
 r.head.y+=.25*mid*sin(bt*volPi*2.);
 r.shL.z=max(r.shL.z,.06);r.shR.z=max(r.shR.z,.06);r.glow=saturate(r.glow+.5*kick+.3*snare);
 return r;
}
VolRobot volRobotBlend(int pa,int pb,float w,float bt,float seed){
 float heavy=volHeavy();
 // Some bars the whole dance drops to a stop-motion stutter.
 float tq=lerp(bt,floor(bt*6+.5)/6.,step(.45,volHash1(floor(bt*.5)+seed*5.3))*step(.5,heavy));
 bool blend=w>.001&&pa!=pb;VolRobot a=(VolRobot)0,b=(VolRobot)0;
 [loop]for(int k=0;k<2;++k){VolRobot r=volRobotPose(k?pb:pa,tq);if(k==0)a=r;else b=r;if(!blend)break;}
 if(blend){float e=liquidEase(w);
  a.body+=(b.body-a.body)*e;a.bob=lerp(a.bob,b.bob,e);a.head+=(b.head-a.head)*e;a.shL+=(b.shL-a.shL)*e;a.elL=lerp(a.elL,b.elL,e);
  a.shR+=(b.shR-a.shR)*e;a.elR=lerp(a.elR,b.elR,e);a.glow=lerp(a.glow,b.glow,e);}
 return volRobotSpasm(a,bt,seed,heavy);
}
// Parts in the robot's own frame: 0 torso 1 hip 2 thruster 3 neck 4 head 5 antenna 6/7 shoulder pads,
// 8-10 left upper arm, forearm, hand; 11-13 right; 14/15 ears; 16 antenna lamp.
void volRobotPart(int k,VolRobot r,out float3 c,out float3 ang,out float3 h){
 c=0;ang=0;h=float3(.1,.1,.1);
 bool left=k==6||(k>=8&&k<=10);
 float3 shoulder=float3(left?-.50:.50,-.18,0),neck=float3(0,-.40,0);
 float3 up=left?float3(r.shL.x,0,r.shL.z):float3(r.shR.x,0,-r.shR.z),fo;
 float el=left?r.elL:r.elR;fo=float3(up.x+el,up.y,up.z);
 if(k==0){c=float3(0,.12,0);h=float3(.36,.40,.21);}
 else if(k==1){c=float3(0,.62,0);h=float3(.26,.12,.16);}
 else if(k==2){c=float3(0,.84,0);h=float3(.15,.06,.15);}
 else if(k==3){c=float3(0,-.34,0);h=float3(.08,.07,.08);}
 else if(k==4){ang=r.head;c=neck+volToWorld(float3(0,-.18,0),r.head);h=float3(.23,.19,.21);}
 else if(k==5){ang=r.head;c=neck+volToWorld(float3(.13,-.48,0),r.head);h=float3(.015,.09,.015);}
 else if(k==16){ang=r.head;c=neck+volToWorld(float3(.13,-.59,0),r.head);h=float3(.032,.032,.032);}
 else if(k==14||k==15){ang=r.head;c=neck+volToWorld(float3(k==14?-.27:.27,-.18,0),r.head);h=float3(.04,.085,.085);}
 else if(k==6||k==7){ang=up*.4;c=shoulder+float3(left?-.05:.05,-.03,0);h=float3(.14,.11,.16);}
 else if(k==8||k==11){ang=up;c=shoulder+volToWorld(float3(0,.23,0),up);h=float3(.075,.23,.075);}
 else if(k==9||k==12){ang=fo;c=shoulder+volToWorld(float3(0,.46,0),up)+volToWorld(float3(0,.20,0),fo);h=float3(.065,.20,.065);}
 else{ang=fo;c=shoulder+volToWorld(float3(0,.46,0),up)+volToWorld(float3(0,.40,0),fo)+volToWorld(float3(0,.07,0),fo);h=float3(.09,.07,.07);}
}
)HLSL"
R"HLSL(
float volLedBox(float2 q,float2 h,float r){float2 d=abs(q)-h+r;return length(max(d,0))+min(max(d.x,d.y),0)-r;}
// One lamp of the face: x = eye, y = mouth. f is in head-front units (x -1..1, y down).
float2 volLedMask(float2 f,float bt){
 float block=floor(bt/4);
 int ex=volHash1(block*3.1+2.2)<.45?0:1+(int)(volHash1(block*5.7+1.1)*5);   // calm most bars, a mood otherwise
 if(snare>.55)ex=3;else if(kick>.7&&ex==0)ex=4;                              // snare: wide eyes, hard kick: scowl
 float open=1-.92*step(frac(bt/5.),.03);
 float hh=lerp(.04,.27,open)*(1-.5*kick)*(1+.35*snare);
 float2 e=float2(abs(f.x)-.46,f.y+.22);float d;
 if(ex==0)d=volLedBox(e,float2(.17,hh),.08);
 else if(ex==1){float2 a=e-float2(0,.10);d=max(abs(length(a)-.19)-.055,a.y);}
 else if(ex==2)d=max(min(abs(e.x-e.y),abs(e.x+e.y))*.707-.05,max(abs(e.x),abs(e.y))-.2);
 else if(ex==3)d=length(e)-.20;
 else if(ex==4)d=volLedBox(float2(e.x,e.y+e.x*.55),float2(.17,hh*.8),.05);
 else{float hm=.05+.22*(f.x<0?bass:high)+.2*(f.x<0?kick:snare);d=volLedBox(float2(e.x,e.y-(.2-hm)),float2(.10,hm),.03);}
 float i=round(f.x/.16)+4,bx=f.x-(i-4)*.16,mouth=0;
 if(i>=0&&i<=8&&abs(bx)<.058){
  float level=i<3?bass*(1-i*.1)+kick*.35:i<6?mid:high+percussion*.5;
  float hm=.035+.30*saturate(level)*(.75+.25*volHash1(i+floor(bt*4)));
  mouth=step(abs(f.y-.46),hm);
 }
 return float2(step(d,0),mouth);
}
// Traces one robot at `center` with uniform scale `sc`. Colour is written on a hit; the exhaust
// rings are additive and go to `glow`. Returns false when the ray misses the robot's bounds.
bool volBotTrace(VolView v,VolRobot r,float3 center,float sc,inout float bestT,inout float3 col,inout float3 glow){
 float3 oc=center+float3(0,.3*sc,0)-v.ro;float tc=dot(oc,v.rd),d2=dot(oc,oc)-tc*tc,rad=1.5*sc;
 if(tc<-rad||d2>rad*rad)return false;
 float3 rw=(v.ro-center)/sc;
 float3 ro=volToLocal(rw,r.body),rd=volToLocal(v.rd,r.body);
 float lim=bestT/sc;bool hitAny=false;
 [loop]for(int k=0;k<17;++k){
  float3 c,a,h;volRobotPart(k,r,c,a,h);
  VolBoxHit hit;if(!volHitBox(ro,rd,c,a,h,.045,lim,hit))continue;
  lim=hit.t;bestT=hit.t*sc;hitAny=true;float3 n=volToWorld(hit.n,r.body);float3 pos=v.ro+v.rd*bestT;
  float3 albedo=lerp(v.primary,v.spectral,.3)*.05,emissive=0;float gloss=1.2;
  if(k==0){
   albedo=v.primary*.03;
   if(abs(hit.nl.z)>.5&&hit.edge<.5){ // the Rare mark on the chest, front and back
    float al;float3 art=volMarkFace(float2(hit.q.x*(hit.nl.z<0?1:-1)/.62+.5,hit.q.y/.62+.5),lerp(v.primary,v.accent,.3),al);
    albedo=lerp(albedo,art*.6,al);emissive=art*al*(.30+.35*r.glow+.05*v.punch);
   }
  }else if(k==4){
   albedo=v.primary*.035;
   if(hit.nl.z<-.5&&hit.edge<.5){ // LED face: a grid of round lamps lit by the expression mask
    float2 f=hit.q.xy/.23,g=f/.0909,cen=(floor(g)+.5)*.0909;
    float lamp=1-smoothstep(.30,.46,length(frac(g)-.5)),face=step(abs(f.y),.80)*step(abs(f.x),.98);
    float2 m=volLedMask(cen,v.bt);
    float scan=exp(-pow((f.y-(frac(v.bt*.25)*2-1)*.9)*5,2));
    albedo=lerp(albedo,float3(.01,.01,.02),face);
    emissive=lamp*face*(v.primary*(m.x*(1.5+.9*kick+.4*r.glow)+.035+.05*scan)+v.accent*(m.y*(1.2+.8*bass)+.02*scan));
   }
  }else if(k==2){albedo=v.spectral*.03;if(hit.nl.y>.5)emissive=v.accent*(1.2+2.2*kick);}
  else if(k==1){albedo=v.spectral*.04;if(hit.nl.z<-.5&&abs(hit.q.y)<.04)emissive=v.accent*(.3+.9*bass);}
  else if(k==5)emissive=v.accent*.8*r.glow;
  else if(k==16)emissive=v.accent*(.6+2.5*kick);
  else if(k==14||k==15){albedo=v.spectral*.05;if(abs(hit.nl.x)>.5)emissive=lerp(v.primary,v.accent,.5)*(.25+1.3*mid+percussion);}
  else if(k==10||k==13){albedo=v.primary*.05;emissive=lerp(v.primary,v.accent,.5)*(.28+.30*r.glow);}
  else if(k==6||k==7){albedo=v.spectral*.06;gloss=1.6;if(hit.nl.y<-.5&&hit.edge<.5)emissive=v.accent*(.18+.9*snare);}
  else if(k>=8){albedo=v.primary*.045;if(hit.nl.z<-.5&&hit.edge<.4)emissive=v.primary*.10*(.5+.5*sin(hit.q.y*18-v.bt*6.28));}
  col=volLight(albedo,emissive,n,pos,v.rd,hit.edge,gloss,v.bt,v.punch,v.primary,v.accent,v.spectral);
 }
 // Exhaust rings: four sheets leave the thruster and widen as they fall. Each is a glowing band
 // on a horizontal plane, drawn only where nothing nearer hides it.
 [unroll]for(int m=0;m<4;++m){
  float s=frac(v.bt*.5+m*.25),y=.86+.66*s+r.bob*.5,R=.13+.42*s;
  if(abs(v.rd.y)<.0001)continue;
  float t=(y-rw.y)/v.rd.y;if(t<=0||t*sc>=bestT)continue;
  float3 p=rw+v.rd*t;float rr=length(p.xz),w=.020+.020*kick;
  float fade=pow(1-s,1.2)*smoothstep(0,.08,s);
  glow+=(exp(-pow((rr-R)/w,2))+.22*exp(-pow((rr-R)/(w*4.5),2)))*fade*lerp(v.primary,v.accent,s)*(1.0+1.8*kick+.4*r.glow);
  hitAny=true;
 }
 return hitAny;
}
// Stage behind Rarebot: god-rays fanning from behind the head, a bass-lit halo, a kick-snapped
// ring pair and embers drifting up.
float3 volRobotBackdrop(VolView v,float2 p,float floorY){
 float3 col=volBackground(v,p,floorY);
 float2 q=p-float2(v.camAng.y,v.camAng.x)*1.3-float2(0,-.12);float rq=length(q),aq=atan2(q.y,q.x);
 float rays=pow(saturate(sin(aq*9+v.bt*.19635)*.5+.5),6)*smoothstep(.12,.5,rq)*(1-smoothstep(.6,1.6,rq));
 col+=lerp(v.primary,v.spectral,.5)*rays*(.03+.05*bass+.05*kick);
 col+=lerp(v.primary,v.accent,.35)*exp(-rq*rq*3.2)*(.04+.06*bass+.10*kick);
 float dashPos=frac((aq+v.bt*.05)/6.283185*40.);
 float dash=smoothstep(.0,.04,dashPos-.1)*(1-smoothstep(.55,.62,dashPos));
 float pulse=.03*kick;
 col+=lerp(v.primary,v.accent,.3)*((1-smoothstep(0,.004,abs(rq-.72-pulse)-.006))*(.10+.25*kick)+(1-smoothstep(0,.004,abs(rq-.80+pulse)-.004))*dash*.12);
 col+=volDust(p+float2(0,v.bt*.03),11,4.7,v.bt)*v.accent*.55;
 return col;
}
)HLSL"
R"HLSL(
float3 volRobotSample(float2 p){
 VolView v=volView(p,4.25,-.14);
 VolRobot r=volRobotBlend(v.pa,v.pb,v.pw,v.bt,0);
 float bestT=1e5;float3 col=0,glow=0;
 volBotTrace(v,r,float3(0,r.bob-.3,0),1,bestT,col,glow);
 if(bestT>=1e5)col=volRobotBackdrop(v,p,1.75);
 return max(col+glow,0);
}
// ---- 46 Rarebot Squad ---------------------------------------------------------------------
// Six slots of the same robot. Each slot pops in on a beat-grid draw that heavy music makes more
// likely, and flashes in for a moment on its own drum: kick, snare or mids. Slot 0 is the original.
struct VolBot {float3 pos;float scale;float t;float mirror;};
float volSym(int i){return i==0?0:((i%2)?-1:1)*((i+1)/2);}
float volSquadAppear(int i,float bt,float period,float prob,int band,bool always){
 float w=floor(bt/period),f=bt-w*period;
 float on=step(volHash1(w*1.9+i*13.7),prob),prev=step(volHash1((w-1)*1.9+i*13.7),prob);
 float s=on>.5?(prev>.5?1:volPop(f/.5)):(prev>.5?1-saturate(f/.25):0);
 float hit=band==0?smoothstep(.25,.8,kick):band==1?smoothstep(.25,.8,snare):smoothstep(.55,.95,mid);
 return always?1:max(s,hit*.9);
}
VolBot volSquadBot(int program,int i,float bt){
 VolBot b;b.pos=0;b.scale=.7;b.t=bt;b.mirror=0;
 float heavy=volHeavy(),period=4,prob=.55+.35*heavy,appear=1;int band=i%3;bool always=i==0;float x=volSym(i);
 if(program==0){ // LINEUP POP: five in a row popping up in turn
  b.pos.x=x*1.0;b.scale=.64;always=i==2;prob=.5+.4*heavy;if(i==5)prob=0;
 }else if(program==1){ // KICK CLONES: clones jump in left and right on every kick
  b.pos=float3(x*1.15,-.06*abs(x),.25*abs(x));b.scale=i==0?.92:.66;period=1;prob=.15+.3*heavy;band=0;
 }else if(program==2){ // SNARE ANSWER: the snare answers the hero with a clone
  b.pos=float3(x*1.3,0,.2*abs(x));b.scale=i==0?.9:.68;period=2;prob=.2+.3*heavy;band=1;
 }else if(program==3){ // MIRROR PAIR: two twins, the back row jumps in on drums
  b.pos=i<2?float3(i==0?-.8:.8,0,0):float3(i==2?-2.0:i==3?2.0:i==4?-3.0:3.0,-.1,.8);b.scale=i<2?.85:.52;b.mirror=i==1?1:0;always=i<2;period=2;band=i%2?1:0;
 }else if(program==4){ // CANON: the same dance, a beat behind each neighbour
  b.pos.x=x*1.05;b.scale=.66;b.t=bt-abs(x)*1.0;always=i<5;prob=1;if(i==5)prob=0;
 }else if(program==5){ // TELEPORT: the hero hops about, afterimages follow on heavy music
  float bi=floor(bt)-i;
  b.pos=float3((volHash1(bi*2.1+3.3)-.5)*4.2,(volHash1(bi*3.7+9.1)-.5)*.5,(volHash1(bi*1.3+5.5)-.5)*.6);b.scale=.8*(1-i*.14);
  appear=i==0?volPop(frac(bt)/.4):(i<=heavy*5?volPop(frac(bt)/.4):0);always=true;
 }else if(program==6){ // CROWD SURGE: two rows, random bursts
  int row=i/3;b.pos=float3((i%3-1)*1.5+row*.7-.35,-row*.3,row*.7);b.scale=.62-row*.08;period=2;prob=.45+.4*heavy;
 }else if(program==7){ // TOTEM: a stack that builds up the bar and topples
  float ph=frac(bt/16.)*16;b.pos=float3(0,(i-2)*-.72,0);b.scale=.40;
  appear=volPop((ph-i*1.5)/.5)*(1-saturate((ph-12-i*.3)/.3));always=true;if(i==5)appear=0;
 }else if(program==8){ // ORBIT CREW: clones circle the hero
  float a=bt*.3927+i*1.2566;b.pos=i==0?float3(0,0,0):float3(cos(a)*2.0,sin(a*2)*.15,sin(a)*1.3);
  b.scale=i==0?.85:.58+.12*sin(a);prob=.8+.15*heavy;if(i==5)prob=0;
 }else{ // FINALE COLLAPSE: a line of five that spreads, then folds back into the hero
  float ph=frac(bt/16.),spread=1-liquidEase(saturate((ph-.55)/.35));
  b.pos.x=x*1.05*spread;b.scale=.62+.23*(1-spread);appear=i==0?1:(i==5?0:spread);always=true;
 }
 if(!(program==5||program==7||program==9))appear=volSquadAppear(i,bt,period,prob,band,always);
 b.scale*=appear;
 return b;
}
float3 volSquadSample(float2 p){
 VolView v=volView(p,5.2,-.12);
 float bestT=1e5;float3 col=0,glow=0;
 float uni=step(3.5,(float)v.pa)*step((float)v.pa,4.5)+step(8.5,(float)v.pa);  // canon and finale share one dance
 [loop]for(int i=0;i<6;++i){
  VolBot a=volSquadBot(v.pa,i,v.bt),b=a;
  bool blend=v.pw>.001&&v.pa!=v.pb;
  if(blend){b=volSquadBot(v.pb,i,v.bt);float e=liquidEase(saturate(v.pw*1.8-i*.12));
   a.pos=lerp(a.pos,b.pos,e);a.scale=lerp(a.scale,b.scale,e);a.t=lerp(a.t,b.t,e);a.mirror=b.mirror;}
  if(a.scale<.03)continue;
  int dance=((int)v.pa+(uni>.5?0:i*3)+(int)floor(a.t/8.))%10;
  VolRobot r=volRobotBlend(dance,(dance+1)%10,liquidEase(saturate((frac(a.t/8.)-.75)*4)),a.t,i+1);
  if(a.mirror>.5){float3 s=r.shL;r.shL=r.shR;r.shR=s;float e=r.elL;r.elL=r.elR;r.elR=e;r.body.y=-r.body.y;r.head.y=-r.head.y;}
  volBotTrace(v,r,a.pos+float3(0,r.bob*a.scale,0),a.scale,bestT,col,glow);
 }
 if(bestT>=1e5)col=volRobotBackdrop(v,p,1.55);
 return max(col+glow,0);
}
)HLSL"
R"HLSL(
// ---- 45 Pillar Choir ----------------------------------------------------------------------
float volPillarHeight(int program,int col,int row,float bt){
 float2 c=float2(col-3,row-3);float r=length(c),a=atan2(c.y,c.x),h;
 if(program==0)h=.5+.5*sin(r*1.2-bt*.7854);
 else if(program==1){float bar=(frac(bt/8.)*2-1)*4.5;h=exp(-(c.x-bar)*(c.x-bar)*.55)*.9+.1;}
 else if(program==2)h=.5+.5*sin(a*2-r*.9+bt*.7854);
 else if(program==3)h=.5+.5*sin(bt*.7854+((col+row)%2)*volPi);
 else if(program==4)h=.5+.5*sin(abs(c.x)*1.1-bt*.7854)*(.6+.4*cos(c.y*.7));
 else if(program==5)h=.15+.85*abs(sin(((c.x+c.y)*.2+bt/16.)*volPi));
 else if(program==6){float f=frac(r*.28-bt/8.)-.5;h=exp(-f*f*38.)*.95+.08;}
 else if(program==7)h=.5+.5*sin(c.x*.9+bt*.7854)*cos(c.y*.6+bt*.3927);
 else if(program==8){ // the mark, in pillars
  float2 uv=float2(col+.5,6-row+.5)/7.;float m=0;
  [unroll]for(int s=0;s<4;++s){float2 o=float2(s%2-.5,s/2-.5)*.07;m+=logoTex.SampleLevel(linearSampler,saturate(uv+o),0).a*.25;}
  h=.10+.90*smoothstep(.35,.85,m)+.04*sin(bt*.5+r);
 }else h=.18+.05*sin(bt*.39+r*.8);
 return saturate(h);
}
float volPillarBlend(int pa,int pb,float w,int col,int row,float bt){
 bool blend=w>.001&&pa!=pb;float a=0,b=0;
 [loop]for(int k=0;k<2;++k){float hh=volPillarHeight(k?pb:pa,col,row,bt);if(k==0)a=hh;else b=hh;if(!blend)break;}
 if(blend){float u=(col+row)/12.,e=liquidEase(saturate(w*1.6-u*.6));a=lerp(a,b,e);}
 return a;
}
float3 volPillarsSample(float2 p){
 VolView v=volView(p,4.6,-.85);
 float bestT=1e5;float3 col=0;float t0,t1;
 bool field=volBox(v.ro-float3(0,.2,0),v.rd,float3(1.3,1.0,1.3),t0,t1);
 if(field){
  [loop]for(int i=0;i<49;++i){
   int cx=i%7,cz=i/7;float h=volPillarBlend(v.pa,v.pb,v.pw,cx,cz,v.bt);float height=.08+1.15*h;
   float3 c=float3((cx-3)*.36,.8-height*.5,(cz-3)*.36),half3=float3(.145,height*.5,.145);
   VolBoxHit hit;if(!volHitBox(v.ro,v.rd,c,0,half3,.03,bestT,hit))continue;
   bestT=hit.t;float3 pos=v.ro+v.rd*hit.t;
   float3 hue=lerp(v.spectral,v.accent,saturate(h*1.3)),albedo=lerp(v.primary,v.spectral,.4)*.03,emissive=0;
   if(hit.nl.y<-.5&&hit.edge<.6)emissive=hue*(.10+.50*h);   // lit cap
   else if(abs(hit.nl.y)<.5){float rib=1-smoothstep(.0,.02,abs(frac(hit.q.y*2.5+v.bt*.125)-.5)-.04);emissive=hue*rib*(.04+h*.14);}
   col=volLight(albedo,emissive,hit.n,pos,v.rd,hit.edge,1.3,v.bt,v.punch,v.primary,v.accent,v.spectral)*exp(-max(hit.t-4.6,0)*.12);
  }
 }
 if(bestT>=1e5)col=volBackground(v,p,.8);
 return max(col,0);
}
)HLSL"
R"HLSL(
// Two rotated-grid samples keep the ray-traced edges clean at the cost of one extra trace.
float3 volumeWorld(float2 p,float s){
 float2 j=float2(.25,.125)/resolution.y;float3 sum=0;
 [loop]for(int k=0;k<2;++k){float2 q=p+(k?-j:j);
  sum+=s<42.5?volumeSample(q):s<43.5?volCardsSample(q):s<44.5?volRobotSample(q):s<45.5?volPillarsSample(q):volSquadSample(q);}
 return sum*.5;
}
)HLSL"

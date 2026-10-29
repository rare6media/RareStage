// Bounded analytic sculptures: no ray marching, particle simulation or history buffers.
R"HLSL(
float sculptureMotif(float bank,float lane,float position,float phase){
 bank=fmod(bank,16);
 if(bank>=15)return sin(lane*.4-phase)*cos(position*4+phase);
 if(bank>=14)return cos(abs(lane)*.65-position*4-phase);
 if(bank>=13)return sin(lane*1.570796)*cos(position*2-phase);
 if(bank>=12)return sin(lane*.4+position*2+phase);
 if(bank>=11)return cos(lane*.5+sin(position*4+phase*.5)*1.5);
 if(bank>=10)return cos(abs(lane)*.9+phase);
 if(bank>=9)return sin(lane*.8-position*3+phase);
 if(bank>=8)return sin(lane*.65+phase);
 if(bank<1)return sin(lane*.65-phase);
 if(bank<2)return sin(abs(lane)*.72-phase);
 if(bank<3)return cos(lane*.8+position*4-phase);
 if(bank<4)return sin(lane*.6+sin(position*3-phase*.5)*2);
 if(bank<5)return cos(lane*1.570796+phase);
 if(bank<6)return sin(lane*.55-phase)*cos(position*2+phase*.5);
 if(bank<7)return cos(abs(lane)*.6+position*3-phase);
 return sin(lane*.7+phase)*cos(position*3-phase*.5);
}
float sculptSignal(float clock,float lane,float position){
 float bank=authoredMotif(floor(clock),patternSeed),nextBank=authoredMotif(floor(clock)+1,patternSeed);
 return lerp(sculptureMotif(bank,lane,position,tempoPhase*.392699),sculptureMotif(nextBank,lane,position,tempoPhase*.392699),motifBlend(clock));
}
float3 vectorSpeedway(float2 p,float pressure,float snareHit,float hatHit){
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 float clock=tempoPhase*.125;
 float3 c=float3(.004,.008,.022);
 // Continuous perspective track. Exponential fog bounds frequencies near the horizon.
 float horizon=-.16,depth=max(p.y-horizon,.004),z=.19/depth;
 float bend=sin(z*.32-tempoPhase*.098175)*.09;
 float track=p.x/depth-bend;
 float visible=smoothstep(.012,.10,depth);
 float lane=floor(track*1.2),local=frac(track*1.2)-.5;
 float wave=sculptSignal(clock,lane,z*.16);
 float stripe=1-smoothstep(.033,.033+max(.007,fwidth(track)*1.2),abs(local));
 float travel=.5+.5*cos(z*2-tempoPhase*.785398);
 float markings=pow(travel,5);
 float3 hue=formationColor(primary,accent,spectral,lane,tempoPhase,.5+.5*wave);
 c+=visible*hue*(stripe*(.16+.16*(.5+.5*wave))+markings*.055);
 c+=visible*accent*stripe*(.02*pressure+.025*snareHit);
 [unroll]for(int arch=0;arch<7;arch++){
  float a=(arch+frac(tempoPhase*.0625))/7.0;
  float size=.10+a*a*1.8;
  float2 q=(p-float2(0,horizon))/size;
  q.x-=sin(a*3-tempoPhase*.098175)*.06;
  float archShape=length(q*float2(.65,1));
  float life=smoothstep(0,.18,a)*(1-smoothstep(.8,1,a));
  float band=1-smoothstep(.008,.008+max(.002,1.5/(resolution.y*size)),abs(archShape-.48));
  float chase=.5+.5*cos(a*8-tempoPhase*.785398);
  c+=band*life*lerp(primary,spectral,a)*(.13+.19*chase+.025*hatHit);
 }
 c+=materialIdentity(p,float2(0,-.03),.8*logoScale,primary,accent,20);
 return c;
}
)HLSL"

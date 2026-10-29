// One authored pattern vocabulary, compiled for both the wall shader and CPU object choreography.
RS_PATTERN_SOURCE(
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
float QuantizedStep(float musical_time,float sub_div,float sharpness){
 float scaled=musical_time*max(sub_div,1);
 float travel=saturate(frac(scaled)/.15);
 return floor(scaled)-1+(1-pow(1-travel,max(sharpness,1)));
}
)

// Reversible eight-beat strokes have zero velocity and acceleration at each join.
R"HLSL(
float puzzleStroke(float beat,float delay){
 float u=frac((beat-delay)/16)*2;
 float wave=sin(min(u,1)*3.14159265);
 return u<1?wave*wave*wave*wave:0;
}
float puzzleLane(float mode,float lane,bool vertical,float beat){
 float phase=vertical?8:0,amplitude=1,group=lane;
 if(mode<.5){amplitude=cos(lane*3.14159265);}
 else if(mode<1.5){amplitude=cos(floor(lane/2)*3.14159265)*1.6;phase=vertical?8:0;}
 else if(mode<2.5){amplitude=vertical?cos(floor(lane/2)*3.14159265)*2:.35;}
 else if(mode<3.5){amplitude=cos(lane*3.14159265)*2;phase+=fmod(abs(lane),2)*1.0;}
 else if(mode<4.5){amplitude=lane<0?-1.5:1.5;phase+=min(abs(lane),4)*.38;}
 else if(mode<5.5){amplitude=cos(floor(lane/3)*3.14159265)*2;}
 else if(mode<6.5){amplitude=(fmod(abs(lane),4)-1.5);phase+=fmod(abs(lane),4)*.4;}
 else if(mode<7.5){amplitude=cos(lane*1.570796)*1.8;phase+=2;}
 else if(mode<8.5){amplitude=abs(lane)>2?sign(lane)*1.8:.25;}
 else {amplitude=cos(lane*3.14159265)*2.4;phase+=min(abs(lane),4)*.25;}
 return amplitude*puzzleStroke(beat,phase);
}
float authoredLane(float world,float seed,float lane,bool vertical,bool old){
 if(world>=24&&world<26){
  float3 m=phraseModes(seed,old);
  return lerp(puzzleLane(m.x,lane,vertical,tempoPhase),puzzleLane(m.y,lane,vertical,tempoPhase),m.z);
 }
 float clock=tempoPhase*.125,blend=motifBlend(clock),a=authoredMotif(floor(clock),seed),b=authoredMotif(floor(clock)+1,seed);
 return vertical?lerp(columnGesture(a,lane,tempoPhase),columnGesture(b,lane,tempoPhase),blend):lerp(rowGesture(a,lane,tempoPhase),rowGesture(b,lane,tempoPhase),blend);
}
)HLSL"

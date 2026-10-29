R"HLSL(
float3 phraseModes(float seed,bool old){
 float4 program=old?oldPhraseProgram:phraseProgram;
 float clock=tempoPhase/16;
 return program.w>.5?program.xyz:float3(fmod(floor(clock)+floor(seed/4),6),fmod(floor(clock)+1+floor(seed/4),6),liquidEase(saturate((frac(clock)-.5)*2)));
}
float3 phraseShapeFor(bool old){return (old?oldPhraseProgram.w:phraseProgram.w)>.5?(old?oldPhraseShape.xyz:phraseShape.xyz):float3(1,1,0);}
)HLSL"

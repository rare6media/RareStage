#pragma once
// Colour ring and procedural-palette functions shared by the main shader and generated worlds.
// Requires the Frame cbuffer (g_Pal*) and paletteRing (palette_ring_shader.h) to be declared first.
#define RS_PALETTE_FUNCS_HLSL R"(
float3 ringHue(float degrees,float boost){float h=degrees/360.;float3 rgb=saturate(abs(frac(h+float3(0,.666667,.333333))*6-3)-1);float luma=dot(rgb,float3(.2126,.7152,.0722));rgb=(rgb*.98+.02)*(1+(.60/max(luma,.08)-1)*.65);return min(rgb*boost,1.2);}
float ringHueBlend(float a,float b,float t){float d=fmod(b-a+540,360)-180;return a+d*t;}
float3 procColor(float t){float3 c=saturate(g_PalA.xyz+g_PalB.xyz*cos(6.2831853*(g_PalC.xyz*t+g_PalD.xyz)));float luma=dot(c,float3(.2126,.7152,.0722));c=(c*.98+.02)*(1+(.60/max(luma,.08)-1)*.65);return min(c,1.2);}
void rarePalette(float selection,out float3 primary,out float3 accent,out float3 spectral){
 // The position is a continuous ring coordinate. Hues travel the short way round, so every glide stays saturated.
 float wrapped=frac(selection/paletteRingCount)*paletteRingCount;int i0=(int)floor(wrapped)%24;int i1=(i0+1)%24;float blend=frac(wrapped);
 float3 a=paletteRing[i0],b=paletteRing[i1];
 primary=ringHue(ringHueBlend(a.x,b.x,blend),1);accent=ringHue(ringHueBlend(a.y,b.y,blend),1.08);spectral=ringHue(ringHueBlend(a.z,b.z,blend),.92);
 if(g_PalA.w>.001){
  // Procedural harmony: three evaluations of one cosine palette, bridged from the colours being left.
  float3 pa=procColor(g_PalB.w),pb=procColor(g_PalB.w+g_PalC.w),pc=procColor(g_PalB.w+g_PalD.w);
  pa=lerp(g_PalPrev0.rgb,pa,g_PalPrev0.w);pb=lerp(g_PalPrev1.rgb,pb,g_PalPrev0.w);pc=lerp(g_PalPrev2.rgb,pc,g_PalPrev0.w);
  primary=lerp(primary,pa,g_PalA.w);accent=lerp(accent,pb,g_PalA.w);spectral=lerp(spectral,pc,g_PalA.w);
 }
})"

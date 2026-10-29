// Flat, filled motion graphics. CPU-authored poses occupy the existing pose buffer.
R"HLSL(
GraphicInk motionField(float2 p,float world){
 GraphicInk ink;ink.distance=100;ink.color=0;float front=-100;
 bool arcs=abs(world-39)<.1||abs(world-41)<.1;
 [loop]for(int id=0;id<16;++id){
  float4 g=playPoseData[id*2],s=playPoseData[id*2+1];float2 offset=p-g.xy;
  float2 bounds=arcs?(g.z+g.w).xx:float2(abs(s.x)*g.z+abs(s.y)*g.w,abs(s.y)*g.z+abs(s.x)*g.w);
  if(any(abs(offset)>bounds+.012))continue;
  float2 q=float2(s.x*offset.x+s.y*offset.y,-s.y*offset.x+s.x*offset.y);
  float d;
  if(arcs){q.y=abs(q.y);d=(s.z*q.y>s.w*q.x?length(q-float2(s.z,s.w)*g.z):abs(length(q)-g.z))-g.w;}
  else d=roundedSdf(q,g.zw,min(g.z,g.w)*s.z);
  // A whole piece owns its depth. Per-pixel depth ranks cut false wedges into
  // overlapping silhouettes and make an ordinary crossing look fragmented.
  float depth=fmod(id,8)*2+(id<8?0:1);
  if(d<ink.distance&&ink.distance>=0||d<0&&depth>front){
   ink.distance=d;front=depth;
   float lane=fmod(id,8),across=arcs?saturate(length(q)/max(g.z+g.w,.001)):saturate(q.x/max(g.z,.001)*.5+.5);
   ink.color=pow(max(graphicColor(floor(lane*.5)+(id<8?0:2),across),0),2.0)*(.90+.22*across);
   ink.color+=graphicColor(lane,1)*exp(-abs(d)*190)*.06;
  }
 }
 return ink;
}
)HLSL"

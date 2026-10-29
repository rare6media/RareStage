// Identity is carried by animated material fragments, with independent entrance/exit programs.
R"HLSL(
float2 identityTransport(float2 p,float amount,float style){
 float row=floor(p.y*22),column=floor(p.x*28);
 float smoothAmount=liquidEase(amount);
 if(style<.5)p.x-=sin(row*1.7)*smoothAmount*.65;
 else if(style<1.5)p.y-=cos(column*1.3)*smoothAmount*.55;
 else if(style<2.5){float ring=floor(length(p)*18);p=rotate2(p,sin(ring*.8)*smoothAmount*1.8);p*=1+smoothAmount*.75;}
 else if(style<3.5){p.x*=1+smoothAmount*1.6;p.y-=sin(p.x*5)*smoothAmount*.18;}
 else if(style<4.5){float band=floor((p.x+p.y)*18);p+=float2(-1,1)*sin(band*.9)*smoothAmount*.28;}
 else if(style<5.5){p=rotate2(p,smoothAmount*.65);p.y*=1+smoothAmount*2;}
 else if(style<6.5){p.x-=sin(p.y*12+amount*3.14159)*smoothAmount*.28;p.y-=sin(p.x*9)*smoothAmount*.07;}
 else {float side=p.y>=0?1:-1;p.x*=1+smoothAmount*.6;p.y-=side*smoothAmount*.32;if(p.y*side<0)p=float2(10,10);}
 return p;
}
float identityProgram(){return stanzaProgress>.72?morphPad2:morphPad1;}
float3 materialIdentity(float2 p,float2 anchor,float size,float3 primary,float3 accent,float world){
 float weight=saturate(identityWeight);if(weight<.00001)return 0;
 float amount=1-sqrt(weight),style=identityProgram();
 float2 q=identityTransport(p-anchor,amount,style);
 float mask=sampleChoreographyIdentity(q,0,size,0);
 float2 grid=q*float2(58,36),cell=frac(grid)-.5;
 // Filled luminous strips/segments retain a material surface during the readable hold.
 float material=1;float finish=fmod(world,8);
 if(finish<2)material=1-smoothstep(.31,.40,abs(cell.y));
 else if(finish<4)material=1-smoothstep(.30,.40,abs(cell.x));
 else if(finish<6)material=roundedFill(cell,float2(.39,.39),.045,.025);
 else material=.30+.70*pow(.5+.5*cos(q.y*135+q.x*30),.4);
 float shimmer=.5+.5*sin(q.x*8-q.y*5+tempoPhase*.19635+world*.4);
 float3 hue=lerp(primary,accent,.20+.25*shimmer);
 return mask*weight*hue*(.40+.32*saturate(.5-cell.x)) *material;
}
)HLSL"

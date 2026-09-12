// Included inside rarestage::play after PlayPose and the vector helpers.
inline PlayPose machinePose(int world,int mode,int id,float beat){
 const float pi=3.14159265359f,t=beat*pi/16.f,u=(id+.5f)/20.f;
 const int col=id%5,row=id/5;const float x=(col-2)*.37f,y=(row-1.5f)*.46f;
 const float a=u*2*pi,side=id<10?-1.f:1.f,v=(id%10+.5f)/10.f;
 const float breathe=.5f-.5f*cos(t);
 PlayPose p{{x,y},{.195f,.225f},0,0,.8f,.5f};
 if(world==32){
  // Tooth phase alternates across adjacent shafts. Whole banks move together.
  p.roundness=1;p.size={.218f,.218f};p.angle=((col+row)%2?-1.f:1.f)*sin(t)*1.3f+((col+row)%2?pi/12:0);
  switch(mode){
   case 0:p.center.x+=sin(t)*.055f;break;
   case 1:p.center.x+=((row%2)?-1.f:1.f)*.15f*sin(t);break;
   case 2:p.center.y+=((col%2)?-1.f:1.f)*.12f*sin(t);break;
   case 3:p.center={cos(a)*(.54f+.08f*breathe),sin(a)*(.64f+.08f*breathe)};p.size={.16f,.16f};break;
   case 4:p.center={side*.43f+cos(v*2*pi)*.32f,sin(v*2*pi)*.62f};p.size={.19f,.19f};break;
   case 5:p.center=rotate2({x*.90f,y*.88f},sin(t)*.30f);break;
   case 6:p.center={x,y+sin(t-col*.65f)*.14f};break;
   case 7:p.center={x*(.72f+.28f*breathe),y*(.72f+.28f*breathe)};p.size=p.size*(.78f+.22f*breathe);break;
   case 8:p.center={cos(a+t*.25f)*(.23f+.50f*u),sin(a+t*.25f)*(.23f+.50f*u)};p.size={.17f,.17f};break;
   case 9:p.center={x+side*.12f*sin(t)*sin(t),y};p.angle*=.65f+.35f*breathe;break;
  }
 }else if(world==33){
  // Closed circuits have no wrapping seam or recycled object teleport.
  const float phase=v*2*pi+t*.5f,bank=side*.40f;
  p.roundness=.22f;p.fold=.45f;p.size={.17f,.12f};
  auto belt=[&](float theta,float width,float height){return float2{width*cos(theta),height*sin(theta)};};
  switch(mode){
   case 0:p.center=belt(phase,.80f,.21f)+float2(0,bank);p.angle=-.22f*sin(phase);break;
   case 1:p.center=belt(v*2*pi+side*t*.5f,.80f,.21f)+float2(0,bank);p.angle=side*.24f*sin(phase);break;
   case 2:p.center=belt(phase,.24f,.74f)+float2(side*.42f,0);p.angle=pi/2;break;
   case 3:p.center={.80f*sin(phase),.62f*sin(phase)*cos(phase)+side*.16f};p.angle=.35f*cos(phase*2);break;
   case 4:p.center={.81f*cos(phase),bank+sin(phase)*(.12f+.18f*breathe)};p.size.x=.14f+.07f*breathe;break;
   case 5:p.center=rotate2(belt(phase,.70f,.24f),side*pi/4);p.angle=side*pi/4;break;
   case 6:p.center={x+sin(t-row*.7f)*.13f,y};p.size={.17f,.17f};break;
   case 7:p.center={x,y+sin(t-col*.7f)*.14f};p.angle=pi/2;p.size={.19f,.13f};break;
   case 8:p.center={cos(a+t*.5f)*.74f,sin(a+t*.5f)*.70f};p.angle=sin(a+t*.5f)*.65f;break;
   case 9:{auto dock=float2(x,y);p.center=lerp(dock,belt(phase,.80f,.21f)+float2(0,bank),breathe);p.angle=side*sin(t)*.22f;break;}
  }
 }else{
  p.size={.23f,.27f};p.roundness=0;p.fold=.85f;
  const float fold=.68f+.32f*(.5f+.5f*cos(t-col*.45f-row*.6f));
  p.size.x*=fold;p.angle=sin(t-row*.65f)*.16f;
  switch(mode){
   case 0:p.center.y+=sin(t-col*.5f)*.09f;break;
   case 1:p.center.x+=((row%2)?-1.f:1.f)*.14f*sin(t);p.angle*=((row%2)?-1.f:1.f);break;
   case 2:p.center={cos(a)*(.40f+.18f*breathe),sin(a)*(.40f+.18f*breathe)};p.angle=a;p.size={.26f,.095f};break;
   case 3:p.center={side*(.18f+v*.62f),(v-.5f)*1.25f};p.angle=side*(.45f+.3f*sin(t));break;
   case 4:p.center=rotate2({x*.86f,y*.86f},.35f*sin(t));p.angle+=.35f*sin(t);break;
   case 5:p.center={x,y+sin(t-col*.65f)*.15f};p.size.y*=.8f+.2f*breathe;break;
   case 6:p.center={side*.41f+cos(v*2*pi)*.27f,sin(v*2*pi)*.57f};p.angle=v*2*pi+side*.22f*sin(t);p.size={.21f,.115f};break;
   case 7:p.center={x*(.74f+.26f*breathe),y};p.angle=((col%2)?-1.f:1.f)*(.3f+.28f*sin(t));break;
   case 8:p.center={cos(a+.25f*sin(t))*(.20f+.58f*u),sin(a+.25f*sin(t))*(.20f+.58f*u)};p.angle=a;p.size={.22f,.13f};break;
   case 9:p.center={x,y*(.74f+.26f*breathe)};p.angle*=breathe;p.size.x=.18f+.05f*breathe;break;
  }
 }
 p.signal=.5f+.5f*sin(t-row*.65f-col*.30f);return p;
}

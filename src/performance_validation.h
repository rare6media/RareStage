// Production shader coverage for new scenes, arrangements and material identities.
int runPerformanceValidation(){
 const std::filesystem::path output="RareStage-performance-validation";std::filesystem::create_directories(output/"preview");
 settings_.quality=2;settings_.spout=0;settings_.vsync=0;settings_.djName=L"RARESIX";if(!rebuildIdentityTexture())return 21;resize(640,360);
 bool passed=true;std::ofstream contracts(output/"contracts.csv");contracts<<"check,passed,value\n";
 auto check=[&](const char*name,bool ok,double value=0){contracts<<name<<','<<ok<<','<<value<<'\n';contracts.flush();passed&=ok;};
 D3D11_TEXTURE2D_DESC desc{};backBuffer_->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
 ComPtr<ID3D11Texture2D>readback;if(FAILED(device_->CreateTexture2D(&desc,nullptr,&readback)))return 21;
 auto pixels=[&](){std::vector<float>result;context_->CopyResource(readback.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE map{};if(FAILED(context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)))return result;
  for(UINT y=0;y<height_;y+=4)for(UINT x=0;x<width_;x+=4){const BYTE*p=static_cast<const BYTE*>(map.pData)+size_t(y)*map.RowPitch+x*4;for(int k=0;k<3;++k)result.push_back(p[k]/255.f);}context_->Unmap(readback.Get(),0);return result;};
 auto delta=[](const std::vector<float>&a,const std::vector<float>&b){if(a.empty()||a.size()!=b.size())return 1.0;double sum=0;for(size_t i=0;i<a.size();++i)sum+=std::abs(a[i]-b[i]);return sum/a.size();};
 auto fixture=[&](float seconds){Constants c{};c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);c.time=seconds;c.tempoPhase=seconds*128.f/60.f;c.logoScale=1;c.palette=5;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.songSection=4;c.bass=.3f;c.mid=.35f;c.audioEnergy=.4f;c.patternSeed=c.previousPatternSeed=4;c.transition=1;return c;};
 std::ofstream banks(output/"arrangements.csv");banks<<"world,program,mean_luma,coverage\n";double lowest=1;int rendered=0;
 for(int world=0;world<WorldCount;++world)for(int program=0;program<64;++program){Constants c=fixture(9);c.scene=c.previousScene=float(world);c.patternSeed=float(program);c.palette=float((program/8)%8);drawScene(c);auto frame=pixels();double sum=0;size_t lit=0;
  for(size_t i=0;i+2<frame.size();i+=3){double l=frame[i]*.0722+frame[i+1]*.7152+frame[i+2]*.2126;sum+=l;lit+=l>.13;}
  double mean=sum/std::max<size_t>(1,frame.size()/3),coverage=double(lit)/std::max<size_t>(1,frame.size()/3);lowest=std::min(lowest,mean);banks<<world<<','<<program<<','<<mean<<','<<coverage<<'\n';
  if(frame.empty()||mean<.018||mean>.9)passed=false;
  if(program%16==4){const auto name="scene-"+std::to_string(world)+"-arrangement-"+std::to_string(program/16)+".png";if(!saveBackBufferPng(output/name))passed=false;}
  ++rendered;
 }
 check("all_scene_arrangements_render",passed,rendered);check("minimum_scene_luma",lowest>.018,lowest);
 std::ofstream identity(output/"identities.csv");identity<<"world,kind,entrance,exit,max_frame_delta,hold_style_delta\n";double maxChange=0,maxHold=0;
 for(int world:{3,6,10,14,17,7})for(int kind=0;kind<2;++kind)for(int style=0;style<8;++style){std::vector<float>previous;double maximum=0;
  for(int f=0;f<=240;++f){Constants c=fixture(16+f/60.f);c.scene=c.previousScene=float(world);c.identityKind=float(kind);c.morphPad1=float(style);c.morphPad2=float((style+3)%8);c.stanzaProgress=f/240.f;c.identityWeight=liquidProgress((c.stanzaProgress-.1f)/.3f)*(1-liquidProgress((c.stanzaProgress-.72f)/.27f));drawScene(c);auto current=pixels();if(!previous.empty())maximum=std::max(maximum,delta(previous,current));previous=current;
   if(f==90||f==150||f==210){const auto name="identity-"+std::to_string(world)+"-"+std::to_string(kind)+"-"+std::to_string(style)+"-"+std::to_string(f)+".png";if(!saveBackBufferPng(output/name))passed=false;}
  }
  Constants held=fixture(18);held.scene=held.previousScene=float(world);held.identityKind=float(kind);held.identityWeight=1;held.stanzaProgress=.71f;held.morphPad1=float(style);held.morphPad2=float((style+3)%8);drawScene(held);auto before=pixels();held.stanzaProgress=.73f;drawScene(held);double hold=delta(before,pixels());maxChange=std::max(maxChange,maximum);maxHold=std::max(maxHold,hold);identity<<world<<','<<kind<<','<<style<<','<<(style+3)%8<<','<<maximum<<','<<hold<<'\n';identity.flush();
 }
 check("identity_motion_continuity",maxChange<.06,maxChange);check("entrance_exit_hold_matches",maxHold<.00001,maxHold);
 // A same-time hit must not move or rescale the travelling mark. CPU contract covers path.
 check("session_memory_and_pixel_path",PerformanceSelfTest());
 if(std::wcsstr(GetCommandLineW(),L"--preview")){
  resize(960,540);const int scenes[]={3,6,8,10,14,12,17,7,4,5,16,3};CornerChoreography dvd;
  for(int frame=0;frame<3960;++frame){const int chapter=frame/360,local=frame%360;Constants c=fixture(frame/60.f);c.scene=c.previousScene=float(scenes[chapter]);c.palette=std::fmod(5+frame/660.f,8.f);c.patternSeed=float((chapter%4)*16+(chapter*3)%16);c.previousPatternSeed=c.patternSeed;
   if(local>=300){c.scene=float(scenes[chapter+1]);c.transition=(local-300)/60.f;c.transitionStyle=float(rarestage::show::HandoffStyle(scenes[chapter],scenes[chapter+1]));}
   float stanza=std::clamp((local-35)/240.f,0.f,1.f);c.stanzaProgress=stanza;c.identityWeight=liquidProgress(stanza/.32f)*(1-liquidProgress((stanza-.70f)/.30f));c.identityKind=float(chapter%2);c.morphPad1=float(chapter%8);c.morphPad2=float((chapter+3)%8);
   c.kick=std::exp(-std::fmod(c.tempoPhase,1.f)*8)*.8f;c.snare=std::exp(-std::fmod(c.tempoPhase+1,2.f)*12)*.6f;c.percussion=std::exp(-std::fmod(c.tempoPhase*2,1.f)*12)*.45f;
   if(chapter==9&&local==180)dvd.aim(c.time,c.time+2,true);dvd.update(c.time,false,0,0);c.dvdX=dvd.x;c.dvdY=dvd.y;c.dvdCorner=dvd.impact(c.time);c.dvdControlled=1;
   drawScene(c);wchar_t name[40]{};swprintf_s(name,L"frame-%04d.png",frame);if(!saveBackBufferPng(output/"preview"/name))return 21;
  }
 }
 return passed?0:22;
}

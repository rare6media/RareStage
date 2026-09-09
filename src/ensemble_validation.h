// Screen occupation is measured above the dim supporting field, in every quadrant.
// Sample whole choreography phrases and three display shapes, not just beauty frames.
int runEnsembleValidation(){
 const std::filesystem::path output="RareStage-ensemble-validation";
 std::filesystem::create_directories(output/"preview");
 settings_.quality=2;settings_.spout=0;settings_.vsync=0;settings_.djName=L"RARESIX";
 if(!rebuildIdentityTexture())return 23;
 bool passed=true;const bool previewOnly=std::wcsstr(GetCommandLineW(),L"--preview-only")!=nullptr;std::ofstream report;
 if(!previewOnly){report.open(output/"composition.csv");report<<"world,width,height,arrangement,beat,coverage,least_quadrant,mean_luma\n";}
 auto fixture=[&](float beats,int world,int arrangement){Constants c{};
  c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);
  c.time=beats*.5f;c.tempoPhase=beats;c.scene=c.previousScene=float(world);c.transition=1;
  c.patternSeed=c.previousPatternSeed=float(arrangement*16+4);c.palette=float((arrangement*2+2)%8);
  c.logoScale=1;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.songSection=4;
  return c;
 };
 if(!previewOnly)for(const auto size:std::array<std::array<UINT,2>,3>{{{320,180},{180,320},{480,180}}}){
  resize(size[0],size[1]);D3D11_TEXTURE2D_DESC desc{};backBuffer_->GetDesc(&desc);
  desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
  ComPtr<ID3D11Texture2D>readback;if(FAILED(device_->CreateTexture2D(&desc,nullptr,&readback)))return 23;
  for(int world:{6,8,10,12,19,18,22,23})for(int arrangement=0;arrangement<4;++arrangement)for(int beat=0;beat<96;++beat){
   auto c=fixture(float(beat),world,arrangement);drawScene(c);
   context_->CopyResource(readback.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE map{};
   if(FAILED(context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)))return 23;
   std::array<size_t,4>lit{},samples{};double sum=0;
   for(UINT y=2;y<height_;y+=4)for(UINT x=2;x<width_;x+=4){
    const BYTE*pixel=static_cast<const BYTE*>(map.pData)+size_t(y)*map.RowPitch+x*4;
    const double luma=(pixel[0]*.0722+pixel[1]*.7152+pixel[2]*.2126)/255;
    const int quadrant=(x>=width_/2?1:0)+(y>=height_/2?2:0);++samples[quadrant];lit[quadrant]+=luma>.23;sum+=luma;
   }
   context_->Unmap(readback.Get(),0);double lowest=1;size_t litTotal=0,total=0;
   for(int q=0;q<4;++q){lowest=std::min(lowest,double(lit[q])/std::max<size_t>(1,samples[q]));litTotal+=lit[q];total+=samples[q];}
   const double coverage=double(litTotal)/std::max<size_t>(1,total),mean=sum/std::max<size_t>(1,total);
   report<<world<<','<<width_<<','<<height_<<','<<arrangement<<','<<beat<<','<<coverage<<','<<lowest<<','<<mean<<'\n';
   // These worlds are authored as filled ensembles, rather than intentionally sparse scenes.
   passed&=coverage>.25&&lowest>.10&&mean<.80;
   if(beat%16==4){const auto name="scene-"+std::to_string(world)+"-"+std::to_string(width_)+"-a"+std::to_string(arrangement)+"-beat"+std::to_string(beat)+".png";if(!saveBackBufferPng(output/name))return 23;}
  }
 }
 report.flush();
 if(std::wcsstr(GetCommandLineW(),L"--preview")){
  resize(960,540);const int worlds[]={6,8,10,12,19,18,22,23,3};
  for(int frame=0;frame<5760;++frame){const int chapter=frame/720,local=frame%720;
   auto c=fixture(frame/30.f,worlds[chapter],chapter%4);c.palette=2+frame/900.f;
   c.kick=std::exp(-std::fmod(c.tempoPhase,1.f)*8)*.8f;c.snare=std::exp(-std::fmod(c.tempoPhase+1,2.f)*12)*.6f;c.percussion=std::exp(-std::fmod(c.tempoPhase*2,1.f)*12)*.45f;c.bass=.25f;c.audioEnergy=.4f;
   if(local>=600){c.scene=float(worlds[chapter+1]);c.transition=(local-600)/120.f;c.previousPatternSeed=c.patternSeed;c.patternSeed=float(((chapter+1)%4)*16+4);c.transitionStyle=float(rarestage::show::HandoffStyle(worlds[chapter],worlds[chapter+1]));}
   drawScene(c);wchar_t name[40]{};swprintf_s(name,L"frame-%04d.png",frame);if(!saveBackBufferPng(output/"preview"/name))return 23;
  }
 }
 return passed?0:24;
}

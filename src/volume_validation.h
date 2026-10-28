// Production-pipeline checks for Rare Volume: every program is rendered through the real shader,
// then measured for visibility and for pops. The fixture substitutes timing and audio only.
int runVolumeValidation(){
 using rarestage::show::ProgramCount;
 const std::filesystem::path output="RareStage-volume-validation";std::filesystem::create_directories(output/"stills");
 settings_.quality=2;settings_.spout=0;settings_.vsync=0;settings_.djName=L"RARESIX";if(!rebuildIdentityTexture())return 41;
 resize(960,540);
 int world=42;
 ComPtr<ID3D11Texture2D>readback;D3D11_TEXTURE2D_DESC desc{};backBuffer_->GetDesc(&desc);
 desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
 if(FAILED(device_->CreateTexture2D(&desc,nullptr,&readback)))return 41;
 auto fixture=[&](float beat,int from,int to,float blend){Constants c{};c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);c.scene=c.previousScene=float(world);c.transition=1;c.time=beat*.5f;c.tempoPhase=beat;c.palette=13;c.logoScale=1;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.songSection=1;
  c.phrase=c.previousPhrase={float(from),float(to),blend,1.f,1,1,0,0};return c;};
 struct Metrics {double mean{},lit{},delta{};std::vector<float>luma;};
 auto measure=[&](const Constants&c,Metrics&previous,bool keep){
  drawScene(c);context_->CopyResource(readback.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
  if(FAILED(context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
  Metrics now;double sum=0;size_t active=0;
  for(UINT y=2;y<height_;y+=4){const BYTE*row=static_cast<const BYTE*>(mapped.pData)+size_t(y)*mapped.RowPitch;
   for(UINT x=2;x<width_;x+=4){const BYTE*px=row+x*4;const float luma=(.0722f*px[0]+.7152f*px[1]+.2126f*px[2])/255.f;sum+=luma;active+=luma>.08f;now.luma.push_back(luma);}}
  context_->Unmap(readback.Get(),0);
  now.mean=sum/now.luma.size();now.lit=double(active)/now.luma.size();
  if(previous.luma.size()==now.luma.size()){double d=0;for(size_t n=0;n<now.luma.size();++n)d+=std::abs(now.luma[n]-previous.luma[n]);now.delta=d/now.luma.size();}
  previous=std::move(now);(void)keep;return true;
 };
 bool passed=true;std::ofstream report(output/"volume-checks.csv");report<<"kind,from,to,beat,mean_luma,lit_fraction,frame_delta\n";
 for(world=42;world<rarestage::show::SceneCount;++world){
 // 1. Every program must show a lit composition at several points of its phrase.
 for(int mode=0;mode<ProgramCount(world);++mode)for(int sample=0;sample<4;++sample){
  const float beat=sample*4+1.5f;auto c=fixture(beat,mode,mode,1);Metrics now;
  if(!measure(c,now,true))return 41;
  const auto name="scene-"+std::to_string(world)+"-program-"+std::to_string(mode)+"-sample-"+std::to_string(sample);
  if(!saveBackBufferPng(output/"stills"/(name+".png")))return 41;
  report<<"pattern,"<<world<<'/'<<mode<<','<<mode<<','<<beat<<','<<now.mean<<','<<now.lit<<",0\n";
  if(now.mean<.008||now.mean>.5||now.lit<.015)passed=false;
 }
 // 2. Continuity. Each program runs 32 beats and each handover blends over 4 beats, sampled every
 //    eighth beat with no audio. Camera and choreography move the whole frame, so the test looks
 //    for outliers: a frame whose change is far above its recent neighbours is a pop.
 double worstRatio=0;
 for(int mode=0;mode<ProgramCount(world);++mode){
  const int next=(mode+1)%ProgramCount(world);Metrics previous;std::vector<double>deltas;
  for(int step=0;step<=32*8;++step){
   const float beat=16.f+step/8.f;const float blend=step<16*8?0.f:std::min(1.f,(step-16*8)/32.f);
   auto c=fixture(beat,mode,next,blend);
   if(!measure(c,previous,false))return 41;
   if(step>0)deltas.push_back(previous.delta);
   if(deltas.size()>6){double base=0;for(size_t n=deltas.size()-6;n<deltas.size()-1;++n)base+=deltas[n]/5;base=std::max(base,.012);
    const double ratio=deltas.back()/base;worstRatio=std::max(worstRatio,ratio);
    if(ratio>2.4&&deltas.back()>.045){passed=false;report<<"pop,"<<world<<'/'<<mode<<','<<next<<','<<beat<<','<<previous.mean<<','<<previous.lit<<','<<previous.delta<<'\n';}}
   if(step%32==0)report<<"continuity,"<<world<<'/'<<mode<<','<<next<<','<<beat<<','<<previous.mean<<','<<previous.lit<<','<<previous.delta<<'\n';
  }
 }
 report<<"summary,0,0,0,0,0,"<<worstRatio<<'\n';
 // 3. Handovers to and from other families go through the shared morph and must never go dark or blank.
 for(int other:{0,3,13,20,35,38,41})for(int direction=0;direction<2;++direction){Metrics handover;
  for(int frame=0;frame<=8;++frame){const int to=direction?other:world,from=direction?world:other;
   auto c=fixture(16+frame*.5f,2,2,1);c.scene=float(to);c.previousScene=float(from);c.transition=frame/8.f;
   c.phrase=c.previousPhrase={2,2,1,rarestage::show::PhraseFamily(to)>=0?1.f:0.f,1,1,0,0};
   if(!measure(c,handover,false))return 41;
   if(handover.mean<.006||handover.lit<.01)passed=false;
   if(frame%4==0&&!saveBackBufferPng(output/"stills"/("join-"+std::to_string(from)+"-"+std::to_string(to)+"-"+std::to_string(frame)+".png")))return 41;
  }
 }
 // 4. The name formation extrudes the DJ name in place of the mark.
 for(int frame=0;frame<=8;frame+=4){auto c=fixture(5.5f,0,0,1);c.identityKind=1;c.identityWeight=frame/8.f;drawScene(c);
  if(!saveBackBufferPng(output/"stills"/("scene-"+std::to_string(world)+"-identity-"+std::to_string(frame)+".png")))return 41;}
 }
 // 5. Cost: wall time per frame, synchronised by reading the last frame back.
 for(const auto&mode:{std::pair<UINT,UINT>{1920,1080},std::pair<UINT,UINT>{3840,2160}}){
  resize(mode.first,mode.second);ComPtr<ID3D11Texture2D>sync;D3D11_TEXTURE2D_DESC sd{};backBuffer_->GetDesc(&sd);
  sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;sd.MiscFlags=0;if(FAILED(device_->CreateTexture2D(&sd,nullptr,&sync)))return 41;
  auto settle=[&](){context_->CopyResource(sync.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE m{};if(SUCCEEDED(context_->Map(sync.Get(),0,D3D11_MAP_READ,0,&m)))context_->Unmap(sync.Get(),0);};
  for(int frame=0;frame<20;++frame)drawScene(fixture(16.f+frame/8.f,6,6,1));settle();
  const auto begin=Clock::now();for(int frame=0;frame<120;++frame)drawScene(fixture(16.f+frame/8.f,frame%10,frame%10,1));settle();
  const double ms=std::chrono::duration<double,std::milli>(Clock::now()-begin).count()/120;
  report<<"cost_"<<mode.first<<"x"<<mode.second<<",0,0,0,"<<ms<<",0,0\n";
 }
 return passed?0:42;
}

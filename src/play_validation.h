// New content is checked through the live HDR scene, morph and composite passes.
int runPlayValidation(){
 const bool motionOnly=std::wcsstr(GetCommandLineW(),L"--motion-only")!=nullptr;
 const bool graphics=std::wcsstr(GetCommandLineW(),L"--graphic-only")!=nullptr;
 const bool late=std::wcsstr(GetCommandLineW(),L"--long-clock")!=nullptr;
 const bool materials=std::wcsstr(GetCommandLineW(),L"--material-test")!=nullptr;
 const std::filesystem::path output=motionOnly?(late?"RareStage-motion-validation/long-clock":"RareStage-motion-validation/programs"):graphics?(late?"RareStage-graphic-validation/long-clock":"RareStage-graphic-validation/programs"):(materials?"RareStage-material-validation":"RareStage-play-validation");std::filesystem::create_directories(output/"preview");
 settings_.quality=2;settings_.spout=0;settings_.vsync=0;settings_.djName=L"RARESIX";if(!rebuildIdentityTexture())return 27;
 const bool stills=std::wcsstr(GetCommandLineW(),L"--stills-only")!=nullptr;
 const bool preview=std::wcsstr(GetCommandLineW(),L"--preview-only")!=nullptr;
 ComPtr<ID3D11Texture2D>readback;bool passed=true;
 auto prepare=[&](UINT w,UINT h){readback.Reset();resize(w,h);D3D11_TEXTURE2D_DESC d{};backBuffer_->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;return SUCCEEDED(device_->CreateTexture2D(&d,nullptr,&readback));};
 auto pixels=[&](){std::vector<float>result;context_->CopyResource(readback.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&m)))return result;
  for(UINT y=1;y<height_;y+=3)for(UINT x=1;x<width_;x+=3){const BYTE*p=static_cast<const BYTE*>(m.pData)+size_t(y)*m.RowPitch+x*4;for(int k=0;k<3;++k)result.push_back(p[k]/255.f);}context_->Unmap(readback.Get(),0);return result;};
 auto delta=[](const std::vector<float>&a,const std::vector<float>&b){if(a.empty()||a.size()!=b.size())return 1.;double sum=0;for(size_t i=0;i<a.size();++i)sum+=std::abs(a[i]-b[i]);return sum/a.size();};
 auto fixture=[&](int world,float beat,int mode){if(late)beat+=8192;Constants c{};c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);c.scene=c.previousScene=float(world);c.transition=1;c.time=beat*.5f;c.tempoPhase=beat;c.palette=2;c.logoScale=1;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.patternSeed=c.previousPatternSeed=0;c.phrase=c.previousPhrase={float(mode),float(mode),1,1,1,1,0,0};return c;};
 if(materials&&!preview&&!stills){
  std::ofstream report(output/"material-patterns.csv");report<<"world,bank,width,height,frames,max_delta,mean_motion,minimum_mean\n";
  for(const auto size:std::array<std::array<UINT,2>,3>{{{320,180},{180,320},{480,180}}}){
   if(!prepare(size[0],size[1]))return 27;
   for(int world=26;world<32;++world)for(int bank=0;bank<16;++bank){std::vector<float>previous;double maximum=0,movement=0,minimum=1;
    for(int frame=0;frame<240;++frame){auto c=fixture(world,frame/30.f,bank%10);c.patternSeed=c.previousPatternSeed=float(bank);drawScene(c);auto now=pixels();if(now.empty())return 27;double sum=0;for(float p:now)sum+=p;minimum=std::min(minimum,sum/now.size());if(!previous.empty()){double d=delta(previous,now);maximum=std::max(maximum,d);movement+=d;}previous=std::move(now);}
    report<<world<<','<<bank<<','<<size[0]<<','<<size[1]<<",240,"<<maximum<<','<<movement/239<<','<<minimum<<'\n';report.flush();passed&=maximum<.06&&movement/239>.0001&&minimum>.025;
   }
  }return passed?0:28;
 }
 if(std::wcsstr(GetCommandLineW(),L"--catalog-capture")){
  const auto directory=std::filesystem::path("RareStage-sequence-validation")/"catalog";std::filesystem::create_directories(directory);
  if(!prepare(480,270))return 27;
  for(int world=0;world<WorldCount;++world){int count=rarestage::show::PhraseFamily(world)>=0?rarestage::show::ProgramCount(world):1;
   for(int mode=0;mode<count;++mode){auto c=fixture(world,5.5f,mode);c.palette=float((world/3)%8);c.phrase.enabled=rarestage::show::PhraseFamily(world)>=0?1.f:0.f;drawScene(c);
    if(!saveBackBufferPng(directory/("scene-"+std::to_string(world)+"-mode-"+std::to_string(mode)+".png")))return 27;}
  }return 0;
 }
 if(stills){if(!prepare(960,540))return 27;for(int world=motionOnly?38:graphics?35:24;world<rarestage::show::SceneCount;++world)for(int mode=0;mode<10;++mode){auto c=fixture(world,5.5f,mode);drawScene(c);if(!saveBackBufferPng(output/("scene-"+std::to_string(world)+"-mode-"+std::to_string(mode)+".png")))return 27;}return 0;}
 if(preview){
  if(!prepare(960,540))return 27;const int fullWorlds[]{3,32,33,34,28,32,34,33,29,13},materialWorlds[]{0,26,27,28,29,30,31,3};const int*worlds=materials?materialWorlds:fullWorlds;
  for(int frame=0;frame<(materials?5760:7200);++frame){int chapter=frame/720,local=frame%720;float beat=frame/30.f;auto c=fixture(worlds[chapter],beat,(chapter*3)%10);if(worlds[chapter]<24)c.phrase.enabled=0;
   c.palette=2+frame/(materials?1371.428571f:1714.285714f);c.snare=std::exp(-std::fmod(beat+1,2.f)*12)*.5f;c.percussion=std::exp(-std::fmod(beat*2,1.f)*12)*.4f;c.bass=.25f;
   if(chapter&&local<240){c.previousScene=float(worlds[chapter-1]);c.transition=local/240.f;c.previousPhrase={float((chapter*3)%10),float((chapter*3)%10),1,worlds[chapter-1]>=24?1.f:0.f,1,1,0,0};}
   if(local>=420){c.phrase.to=float((chapter*3+3)%10);c.phrase.blend=liquidProgress((local-420)/240.f);}
   if(chapter==6&&local>=300){c.identityWeight=liquidProgress((local-300)/120.f)*(1-liquidProgress((local-600)/120.f));c.identityKind=0;}
   drawScene(c);wchar_t name[40]{};swprintf_s(name,L"frame-%04d.png",frame);if(!saveBackBufferPng(output/"preview"/name))return 27;
  }return 0;
 }
 std::ofstream motion(output/"patterns.csv"),programs(output/"program-blends.csv"),bridges(output/"bridges.csv");
 motion<<"world,mode,width,height,frames,max_delta,mean_motion,minimum_mean\n";
 programs<<"world,source,target,max_delta\n";
 bridges<<"from,to,source,target,max_delta,endpoint_error,near_endpoint_error\n";
 for(const auto size:std::array<std::array<UINT,2>,3>{{{320,180},{180,320},{480,180}}}){
  if(!prepare(size[0],size[1]))return 27;
  for(int world=motionOnly?38:graphics?35:24;world<rarestage::show::SceneCount;++world)for(int mode=0;mode<10;++mode){std::vector<float>previous;double maximum=0,movement=0,minimum=1;
   for(int frame=0;frame<480;++frame){auto c=fixture(world,frame/30.f,mode);drawScene(c);auto now=pixels();if(now.empty())return 27;double sum=0;for(float p:now)sum+=p;minimum=std::min(minimum,sum/now.size());if(!previous.empty()){double d=delta(previous,now);maximum=std::max(maximum,d);movement+=d;}previous=std::move(now);}
   motion<<world<<','<<mode<<','<<size[0]<<','<<size[1]<<",480,"<<maximum<<','<<movement/479<<','<<minimum<<'\n';motion.flush();passed&=maximum<.06&&movement/479>.0001&&minimum>.025;
  }
 }
 if(!prepare(320,180))return 27;
 for(int world=motionOnly?38:graphics?35:24;world<rarestage::show::SceneCount;++world)for(int source=0;source<10;++source)for(int target=0;target<10;++target){if(source==target)continue;std::vector<float>previous;double maximum=0;
  for(int frame=0;frame<=120;++frame){auto c=fixture(world,20+frame/15.f,source);c.phrase.to=float(target);c.phrase.blend=liquidProgress(frame/120.f);drawScene(c);auto now=pixels();if(!previous.empty())maximum=std::max(maximum,delta(previous,now));previous=std::move(now);}
  programs<<world<<','<<source<<','<<target<<','<<maximum<<'\n';programs.flush();passed&=maximum<.06;
 }
 // All newly introduced native edges, including new wall scenes to old favorites.
 for(int from=0;from<rarestage::show::SceneCount;++from)for(int to=0;to<rarestage::show::SceneCount;++to){if(from==to||std::max(from,to)<(motionOnly?38:graphics?35:24)||!rarestage::show::NativeBridge(from,to))continue;
  for(int source:{0,4,9})for(int target:{1,5,8}){std::vector<float>previous;double maximum=0,endpoint=0,nearError=0;
   for(int frame=0;frame<=120;++frame){auto c=fixture(to,20+frame/30.f,target);c.previousScene=float(from);c.previousPhrase={float(source),float(source),1,from>=24?1.f:0.f,1,1,0,0};if(to<24)c.phrase.enabled=0;c.transition=frame/120.f;if(from>=26&&to>=26){c.previousPatternSeed=52;c.patternSeed=12;}
    drawScene(c);auto now=pixels();if(!previous.empty())maximum=std::max(maximum,delta(previous,now));previous=now;
    if(frame==0||frame==120){auto end=c;end.scene=end.previousScene=frame==0?c.previousScene:c.scene;end.phrase=frame==0?c.previousPhrase:c.phrase;end.patternSeed=frame==0?c.previousPatternSeed:c.patternSeed;end.transition=1;drawScene(end);endpoint=std::max(endpoint,delta(now,pixels()));c.transition=frame==0?.0001f:.9999f;drawScene(c);nearError=std::max(nearError,delta(now,pixels()));}
   }
   bridges<<from<<','<<to<<','<<source<<','<<target<<','<<maximum<<','<<endpoint<<','<<nearError<<'\n';bridges.flush();passed&=maximum<.06&&endpoint<.00001&&nearError<.003;
  }
 }
 return passed?0:28;
}

// Included in App. Uses production conductor, shaders, morphs and output mastering.
int runPhraseValidation(){
 using namespace rarestage::show;
 const std::filesystem::path output="RareStage-phrase-validation";std::filesystem::create_directories(output/"preview");
 settings_.quality=2;settings_.spout=0;settings_.vsync=0;settings_.djName=L"RARESIX";if(!rebuildIdentityTexture())return 25;
 bool passed=true;const bool previewOnly=std::wcsstr(GetCommandLineW(),L"--preview-only")!=nullptr;
 ComPtr<ID3D11Texture2D>readback;
 auto prepare=[&](UINT width,UINT height){readback.Reset();resize(width,height);D3D11_TEXTURE2D_DESC desc{};backBuffer_->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;return SUCCEEDED(device_->CreateTexture2D(&desc,nullptr,&readback));};
 double coverage=0,quadrant=0;
 auto pixels=[&](){std::vector<float> result;context_->CopyResource(readback.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE map{};
  if(FAILED(context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)))return result;
  std::array<int,4>lit{},total{};
  for(UINT y=2;y<height_;y+=4)for(UINT x=2;x<width_;x+=4){const BYTE*p=static_cast<const BYTE*>(map.pData)+size_t(y)*map.RowPitch+x*4;
   for(int k=0;k<3;++k)result.push_back(p[k]/255.f);const int q=(x>=width_/2?1:0)+(y>=height_/2?2:0);++total[q];lit[q]+=(p[0]*.0722+p[1]*.7152+p[2]*.2126)/255>.23;
  }context_->Unmap(readback.Get(),0);quadrant=1;int litTotal=0,totalSamples=0;for(int q=0;q<4;++q){quadrant=std::min(quadrant,double(lit[q])/std::max(1,total[q]));litTotal+=lit[q];totalSamples+=total[q];}coverage=double(litTotal)/std::max(1,totalSamples);return result;
 };
 auto delta=[](const std::vector<float>&a,const std::vector<float>&b){if(a.empty()||a.size()!=b.size())return 1.;double sum=0;for(size_t i=0;i<a.size();++i)sum+=std::abs(a[i]-b[i]);return sum/a.size();};
 auto fixture=[&](int world,float beat){Constants c{};c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);c.scene=c.previousScene=float(world);c.transition=1;c.tempoPhase=beat;c.time=beat*.5f;c.patternSeed=c.previousPatternSeed=20;c.palette=2;c.logoScale=1;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;return c;};
 auto evidence=[](float beat){PhraseEvidence e;e.section=beat<48?4:beat<64?1:beat<80?2:beat<128?4:beat<160?3:4;e.revision=int(beat/16);e.sectionAge=std::fmod(beat,16.f);e.trend=e.section==1?.08f:0;return e;};
 if(!previewOnly){
  std::ofstream motion(output/"phrases.csv"),bridges(output/"bank-bridges.csv"),trace(output/"decisions.csv");
  motion<<"world,width,height,max_delta,mean_motion,minimum_coverage,minimum_quadrant\n";
  bridges<<"from,to,width,source_mode,target_mode,max_delta,endpoint_error,near_endpoint_error\n";
  trace<<"world,beat,from,to,action,blend,gain\n";
  for(const auto size:std::array<std::array<UINT,2>,3>{{{320,180},{180,320},{480,180}}}){
   if(!prepare(size[0],size[1]))return 25;
   for(int world:{6,8,10,12,19,18,22,23}){
    PhraseConductor conductor;PhraseMemory memory;conductor.begin(world,20,0);
    std::vector<float> previous;double maximum=0,sum=0,minCoverage=1,minQuadrant=1;
    // 192 beats at 120 BPM and 60 samples per second, including action joins.
    for(int frame=0;frame<5760;++frame){float beat=frame/30.f;conductor.update(beat,evidence(beat),memory);auto c=fixture(world,beat);c.phrase=conductor.sample(beat);drawScene(c);auto current=pixels();
     if(!previous.empty()){double d=delta(previous,current);maximum=std::max(maximum,d);sum+=d;}previous=current;minCoverage=std::min(minCoverage,coverage);minQuadrant=std::min(minQuadrant,quadrant);
     if(frame%240==0&&size[0]==320)trace<<world<<','<<beat<<','<<c.phrase.from<<','<<c.phrase.to<<','<<c.phrase.action<<','<<c.phrase.blend<<','<<c.phrase.gain<<'\n';
     if(frame%960==720){auto name="scene-"+std::to_string(world)+"-"+std::to_string(size[0])+"-"+std::to_string(frame)+".png";if(!saveBackBufferPng(output/name))return 25;}
    }
    motion<<world<<','<<size[0]<<','<<size[1]<<','<<maximum<<','<<sum/5759<<','<<minCoverage<<','<<minQuadrant<<'\n';motion.flush();
    passed&=maximum<.04&&sum/5759>.0001&&minCoverage>.25&&minQuadrant>.10;
   }
   for(int from:{8,10})for(int source=0;source<6;++source)for(int target=0;target<6;++target){int to=from==8?10:8;std::vector<float>previous;double maximum=0,endpoint=0,nearError=0;
    for(int frame=0;frame<=120;++frame){auto c=fixture(to,40+frame/30.f);c.previousScene=float(from);c.transition=frame/120.f;c.previousPatternSeed=52;
     c.previousPhrase={float(source),float((source+1)%6),.37f,1,.84f,.96f,.12f,2};c.phrase={float(target),float((target+2)%6),.62f,1,1.14f,1.04f,-.15f,3};drawScene(c);auto current=pixels();
     if(!previous.empty())maximum=std::max(maximum,delta(previous,current));previous=current;
     if(frame==0||frame==120){auto end=c;end.scene=end.previousScene=frame==0?c.previousScene:c.scene;end.patternSeed=frame==0?c.previousPatternSeed:c.patternSeed;end.phrase=frame==0?c.previousPhrase:c.phrase;end.transition=1;drawScene(end);endpoint=std::max(endpoint,delta(current,pixels()));c.transition=frame==0?.0001f:.9999f;drawScene(c);nearError=std::max(nearError,delta(current,pixels()));}
    }
    bridges<<from<<','<<to<<','<<size[0]<<','<<source<<','<<target<<','<<maximum<<','<<endpoint<<','<<nearError<<'\n';bridges.flush();passed&=maximum<.06&&endpoint<.00001&&nearError<.003;
   }
  }
 }
 if(std::wcsstr(GetCommandLineW(),L"--preview")){
  if(!prepare(960,540))return 25;const int worlds[]={6,8,10,12,19,18,22,23};
  PhraseConductor conductor,old;PhraseMemory memory;conductor.begin(worlds[0],20,-32);for(float beat=-24;beat<0;beat+=8)conductor.update(beat,evidence(0),memory);
  for(int frame=0;frame<5760;++frame){const int chapter=frame/720,local=frame%720;float beat=frame/30.f;
   if(local==0&&chapter){old=conductor;conductor.begin(worlds[chapter],20,beat-32);for(float warm=beat-24;warm<beat;warm+=8)conductor.update(warm,evidence(beat),memory);}
   auto e=evidence(beat);e.safe=local>=120;conductor.update(beat,e,memory);auto c=fixture(worlds[chapter],beat);c.palette=2+frame/1100.f;c.phrase=conductor.sample(beat);c.previousPhrase=old.sample(beat);
   if(chapter&&local<120){c.previousScene=float(worlds[chapter-1]);c.transition=local/120.f;}
   c.snare=std::exp(-std::fmod(beat+1,2.f)*12)*.6f;c.percussion=std::exp(-std::fmod(beat*2,1.f)*12)*.45f;c.bass=.25f;
   drawScene(c);wchar_t name[40]{};swprintf_s(name,L"frame-%04d.png",frame);if(!saveBackBufferPng(output/"preview"/name))return 25;
  }
 }
 return passed?0:26;
}

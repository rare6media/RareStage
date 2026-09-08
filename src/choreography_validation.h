// Exercises the same GPU passes as live playback, including mixed-scene material transport.
int runChoreographyValidation(bool benchmark){
    const std::filesystem::path output="RareStage-choreography-validation";
    std::error_code error;std::filesystem::create_directories(output/"preview",error);if(error)return 14;
    settings_.quality=2;settings_.vsync=0;settings_.spout=0;settings_.djName=L"RARESIX";
    if(!rebuildIdentityTexture())return 14;resize(benchmark?3840u:640u,benchmark?2160u:360u);
    ComPtr<ID3D11Texture2D>readback;D3D11_TEXTURE2D_DESC desc{};backBuffer_->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    if(!benchmark&&FAILED(device_->CreateTexture2D(&desc,nullptr,&readback)))return 14;
    auto pixels=[&](){std::vector<float> result;context_->CopyResource(readback.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE map{};
        if(FAILED(context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)))return result;
        for(UINT y=0;y<height_;y+=2){const auto*row=static_cast<const BYTE*>(map.pData)+size_t(y)*map.RowPitch;for(UINT x=0;x<width_;x+=2)for(int channel=0;channel<3;++channel)result.push_back(row[x*4+channel]/255.f);}
        context_->Unmap(readback.Get(),0);return result;};
    auto difference=[](const std::vector<float>&a,const std::vector<float>&b){if(a.empty()||a.size()!=b.size())return 1.0;double delta=0;for(size_t n=0;n<a.size();++n)delta+=std::abs(a[n]-b[n]);return delta/a.size();};
    auto drain=[&](){context_->Flush();const auto deadline=Clock::now()+std::chrono::seconds(10);for(;;){pollGpuTimers();bool pending=false;for(const auto&slot:gpuTimers_)pending|=slot.pending;if(!pending)return true;if(Clock::now()>deadline)return false;Sleep(1);}};
    auto fixture=[&](float time){Constants c{};c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);c.time=time;c.tempoPhase=time*128.f/60.f;c.logoScale=1;c.palette=0;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.songSection=2;c.visualMovement=2;c.bass=.3f;c.mid=.4f;c.high=.2f;c.audioEnergy=.4f;c.transition=1;return c;};
    std::vector<std::array<int,2>>pairs{{3,13},{13,3},{3,9},{9,11},{15,0},{1,3},{3,1},{16,3},{3,16},{1,16},{16,7},{2,17},{3,18},{18,19},{19,20},{20,3},{16,18},{16,20},{1,19},{20,16}};
    if(benchmark){const std::array<std::array<int,2>,9> rebuilt{{{6,8},{8,10},{10,14},{14,12},{12,17},{17,7},{7,4},{4,5},{5,16}}};pairs.insert(pairs.end(),rebuilt.begin(),rebuilt.end());}
    std::ofstream report(output/(benchmark?"morph-gpu.csv":"morph-frames.csv"));
    if(benchmark)report<<"from,to,width,height,samples,mean_ms,p95_ms,p99_ms,max_ms\n";
    else report<<"from,to,style,frame,phase,temporal_delta\n";
    bool passed=true;
    for(const auto&pair:pairs)for(int style=0;style<(benchmark?1:4);++style){
        if(!drain())return 14;if(benchmark){for(int warm=0;warm<60;++warm){Constants c=fixture(7.f+warm/60.f);c.previousScene=float(pair[0]);c.scene=float(pair[1]);c.previousPatternSeed=2;c.patternSeed=5;c.transition=.5f;drawScene(c);if(warm%8==7&&!drain())return 14;}if(!drain())return 14;}
        validationGpuSamples_.clear();collectValidationGpu_=benchmark;
        std::vector<float>previous;
        for(int frame=0;frame<=120;++frame){
            Constants c=fixture(8.f+frame/60.f);c.previousScene=float(pair[0]);c.scene=float(pair[1]);c.previousPatternSeed=float(2+style*3);c.patternSeed=float(5+style*3);c.transition=frame/120.f;c.transitionStyle=float(style);if(benchmark){c.tempoPhase=frame*128.f/120.f;c.previousPatternSeed=12;c.patternSeed=8;}
            pollGpuTimers();drawScene(c);
            if(benchmark){context_->Flush();if(frame%8==7&&!drain())return 14;continue;}
            const auto current=pixels();const double delta=previous.empty()?0:difference(current,previous);previous=current;
            report<<pair[0]<<','<<pair[1]<<','<<style<<','<<frame<<','<<c.transition<<','<<delta<<'\n';
            if(current.empty()||delta>.06)passed=false;
            if(frame==0||frame==120){
                Constants endpoint=c;endpoint.scene=endpoint.previousScene=frame==0?c.previousScene:c.scene;endpoint.patternSeed=frame==0?c.previousPatternSeed:c.patternSeed;endpoint.transition=1;
                drawScene(endpoint);if(difference(current,pixels())>.00001)passed=false;
                Constants neighbor=c;neighbor.transition=frame==0?.0001f:.9999f;drawScene(neighbor);if(difference(current,pixels())>.003)passed=false;
            }
            if(style==0&&frame%30==0){drawScene(c);const auto name="morph-"+std::to_string(pair[0])+"-"+std::to_string(pair[1])+"-"+std::to_string(frame)+".png";if(!saveBackBufferPng(output/name))passed=false;}
        }
        if(!drain())return 14;collectValidationGpu_=false;
        if(benchmark){auto samples=validationGpuSamples_;if(samples.size()!=121)return 14;std::sort(samples.begin(),samples.end());double sum=0;for(float sample:samples)sum+=sample;report<<pair[0]<<','<<pair[1]<<",3840,2160,"<<samples.size()<<','<<sum/samples.size()<<','<<samples[size_t(samples.size()*.95)]<<','<<samples[size_t(samples.size()*.99)]<<','<<samples.back()<<'\n';report.flush();}
    }
    if(!benchmark){
        // 20-second, 60 fps capture: row motion, identity, then two geometric scene handoffs.
        for(int frame=0;!std::wcsstr(GetCommandLineW(),L"--no-preview")&&frame<1200;++frame){
            Constants c=fixture(frame/60.f);c.scene=c.previousScene=3;c.patternSeed=c.previousPatternSeed=3;
            float beatPosition=std::fmod(c.tempoPhase,1.f);c.kick=std::exp(-beatPosition*8)*.75f;c.percussion=std::exp(-std::fmod(c.tempoPhase*2,1.f)*14)*.4f;
            if(frame>=360&&frame<660){float stanza=(frame-360)/300.f;c.identityWeight=liquidProgress((stanza-.1f)/.3f)*(1-liquidProgress((stanza-.7f)/.3f));c.identityKind=0;}
            if(frame>=720&&frame<840){c.previousScene=3;c.scene=1;c.transition=(frame-720)/120.f;c.patternSeed=5;}
            else if(frame>=840&&frame<960){c.scene=c.previousScene=1;c.patternSeed=5;}
            else if(frame>=960&&frame<1080){c.previousScene=1;c.scene=16;c.transition=(frame-960)/120.f;c.previousPatternSeed=5;c.patternSeed=2;}
            else if(frame>=1080){c.scene=c.previousScene=16;c.patternSeed=2;}
            pollGpuTimers();drawScene(c);wchar_t name[40]{};std::swprintf(name,std::size(name),L"frame-%04d.png",frame);if(!saveBackBufferPng(output/"preview"/name))return 14;
        }
        // Rapid manual selections preserve the in-flight scene and update one pending target.
        started_=Clock::now();world_=3;transitionStarted_=-100;queuedWorld_=-1;
        selectWorld(1);selectWorld(16);if(world_!=1||previousWorld_!=3||queuedWorld_!=16)passed=false;
        selectWorld(1);if(queuedWorld_!=-1)passed=false;
        selectWorld(16);queuedMotionSeconds_=3.75f;queuedMotionStyle_=2;transitionStarted_=-100;serviceQueuedWorld();
        if(world_!=16||previousWorld_!=1||queuedWorld_!=-1||activeTransitionSeconds_!=3.75f||activeTransitionStyle_!=2)passed=false;
        StageSyncPacket packet{};packet.transitionSeconds=8.f*60.f/65.f;if(!ValidStageSyncPacket(packet,packet.groupId))passed=false;
        packet.transitionSeconds=8.01f;if(ValidStageSyncPacket(packet,packet.groupId))passed=false;
        transitionStarted_=-100;visualBpm_=128;songSection_=2;selectWorld(3,true);
        if(std::abs(activeTransitionSeconds_-4.f*60.f/128.f)>.0001f)passed=false;
        transitionStarted_=-100;songSection_=3;selectWorld(1,true);
        if(std::abs(activeTransitionSeconds_-8.f*60.f/128.f)>.0001f)passed=false;
    }
    return passed?0:15;
}

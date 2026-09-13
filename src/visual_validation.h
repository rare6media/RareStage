// Included inside App so validation uses the production shaders, targets and GPU timers.
int runVisualValidation(bool benchmark) {
    const std::filesystem::path output=benchmark?"RareStage-benchmark-4k":"RareStage-visual-validation";
    std::error_code error;std::filesystem::create_directories(output,error);
    if(error)return 11;
    settings_.quality=2;settings_.vsync=0;settings_.spout=0;settings_.djName=L"RARESIX";
    if(!rebuildIdentityTexture())return 11;
    resize(benchmark?3840u:1280u,benchmark?2160u:720u);
    std::ofstream report(output/(benchmark?"timings.csv":"frames.csv"));
    if(!report)return 11;
    if(benchmark)report<<"world,width,height,render_scale,samples,mean_ms,p95_ms,p99_ms,max_ms,budget_ms,throughput_fps\n";
    else report<<"world,identity,frame,stanza,weight,mean_luma,active_fraction,temporal_delta\n";
    // Readback is reused for artifact checks; benchmark timing never includes PNG/readback work.
    ComPtr<ID3D11Texture2D>readback;D3D11_TEXTURE2D_DESC desc{};backBuffer_->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    if(!benchmark&&FAILED(device_->CreateTexture2D(&desc,nullptr,&readback)))return 11;
    auto drain=[&](){context_->Flush();const auto deadline=Clock::now()+std::chrono::seconds(10);for(;;){pollGpuTimers();bool pending=false;for(const auto&slot:gpuTimers_)pending|=slot.pending;if(!pending)return true;if(Clock::now()>deadline){StartupLog("GPU benchmark query drain timed out");return false;}Sleep(1);}};
    bool passed=true;std::array<int,WorldCount>timingRetries{};std::ofstream retries;if(benchmark){retries.open(output/"timing-retries.csv");retries<<"world,attempt,valid_samples,rejected_samples,skipped_samples\n";}
    const std::array<int,WorldCount>worlds{0,3,9,11,13,15,1,7,16,2,4,5,6,8,10,12,14,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41};
    for(int world:worlds){
        const bool wall=world==0||world==3||world==9||world==11||world==13||world==15;
        if(!benchmark&&!(wall||world==1||world==7||world==16||world==2||world>=18))continue;
        const int kinds=benchmark?1:2,frames=benchmark?1080:((wall||world==1||world==16||world>=18)?1024:128);
        for(int kind=0;kind<kinds;++kind){
            if(!drain())return 11;validationGpuSamples_.clear();collectValidationGpu_=false;
            std::vector<float>previous;Clock::time_point benchmarkStart{};
            for(int frame=0;frame<frames;++frame){
                if(benchmark&&frame==120){if(!drain())return 11;validationGpuSamples_.clear();validationGpuRejected_=validationGpuSkipped_=0;collectValidationGpu_=true;benchmarkStart=Clock::now();}
                pollGpuTimers();Constants c{};
                c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);
                c.time=benchmark?float(frame)/120.f:float(frame)*16.f/128.f*60.f/128.f;
                c.scene=c.previousScene=float(world);c.transition=1;c.logoScale=1;c.palette=2;
                c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.songSection=2;
                c.visualMovement=2;c.patternSeed=float(world%8);c.idleLife=1;
                c.tempoPhase=benchmark?std::max(0,frame-120)*128.f/960.f:c.time*128.f/60.f;c.beatIndex=std::floor(c.tempoPhase);
                if(benchmark&&rarestage::show::PhraseFamily(world)>=0)c.phrase={2,4,liquidProgress(std::fmod(c.tempoPhase,16.f)/16.f),1,1.16f,1.04f,.18f,3};
                c.stanzaProgress=std::fmod(c.tempoPhase,16.f)/16.f;
                c.identityKind=float(kind);c.identityWeight=liquidProgress((c.stanzaProgress-.16f)/.30f)*(1-liquidProgress((c.stanzaProgress-.72f)/.24f));
                c.movementProgress=liquidProgress(c.stanzaProgress);
                // Alternate silent and instrument-hit stanzas; both must preserve a complete wall.
                if(benchmark||kind==1){float pulse=std::exp(-std::fmod(c.tempoPhase,1.f)*8);c.bass=.52f;c.mid=.38f;c.high=.24f;c.audioEnergy=.55f;c.kick=pulse;c.beat=pulse;c.percussion=std::exp(-std::fmod(c.tempoPhase*2,1.f)*14)*.65f;c.snare=std::exp(-std::fmod(c.tempoPhase+1,2.f)*12)*.8f;}
                drawScene(c);
                if(benchmark){context_->Flush();if(frame%8==7&&!drain())return 11;continue;}
                context_->CopyResource(readback.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
                if(FAILED(context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)))return 11;
                std::vector<float>pixels;double sum=0,delta=0;size_t active=0;
                for(UINT y=2;y<height_;y+=4){const BYTE*row=static_cast<const BYTE*>(mapped.pData)+size_t(y)*mapped.RowPitch;
                    for(UINT x=2;x<width_;x+=4){const BYTE*px=row+x*4;float luma=(.0722f*px[0]+.7152f*px[1]+.2126f*px[2])/255.f;sum+=luma;active+=luma>.13f;pixels.push_back(luma);}}
                context_->Unmap(readback.Get(),0);
                if(previous.size()==pixels.size())for(size_t n=0;n<pixels.size();++n)delta+=std::abs(pixels[n]-previous[n]);
                const double mean=sum/pixels.size(),coverage=double(active)/pixels.size(),motion=delta/pixels.size();previous=std::move(pixels);
                report<<world<<','<<kind<<','<<frame<<','<<c.stanzaProgress<<','<<c.identityWeight<<','<<mean<<','<<coverage<<','<<motion<<'\n';
                if(wall&&(mean<.10||coverage<.45||(kind==0&&motion>.06)))passed=false;
                if(frame==0||frame==28||frame==52||frame==76||frame==108){
                    const auto name="world-"+std::to_string(world+1)+"-identity-"+std::to_string(kind)+"-frame-"+std::to_string(frame)+".png";
                    if(!saveBackBufferPng(output/name))passed=false;
                }
            }
            if(!drain())return 11;collectValidationGpu_=false;
            if(benchmark){
                const double duration=std::chrono::duration<double>(Clock::now()-benchmarkStart).count();
                auto samples=validationGpuSamples_;if(samples.size()!=960){
                    retries<<world<<','<<timingRetries[world]<<','<<samples.size()<<','<<validationGpuRejected_<<','<<validationGpuSkipped_<<'\n';retries.flush();
                    StartupLog(("Incomplete GPU timestamps: scene "+std::to_string(world)+", valid "+std::to_string(samples.size())+", rejected "+std::to_string(validationGpuRejected_)+", skipped "+std::to_string(validationGpuSkipped_)).c_str());
                    // Retry the complete deterministic scene window once. Never report
                    // a partial percentile or silently discard an individual slow frame.
                    if(timingRetries[world]++==0){--kind;continue;}return 11;
                }std::sort(samples.begin(),samples.end());
                double total=0;for(float sample:samples)total+=sample;
                const auto percentile=[&](double p){return samples[std::min(samples.size()-1,size_t(std::ceil(samples.size()*p)-1))];};
                report<<world<<",3840,2160,1,"<<samples.size()<<','<<total/samples.size()<<','<<percentile(.95)<<','<<percentile(.99)<<','<<samples.back()<<",8.33333,"<<960/duration<<'\n';
                report.flush();
            }
        }
    }
    // Exercise production stanza decisions with disruptive section revisions while a name holds.
    world_=3;worldBeatOrigin_=0;identityCycle_=-1;movementRevision_=-1;settings_.djIdentity=1;
    // Establish a scheduled reveal on its boundary before examining the hold.
    // The old fixture relied on every even stanza revealing, which spacing replaces.
    automatic_=false;frozen_=false;transitionStarted_=-100;songSection_=4;identitySpacing_={};phraseConductor_.begin(3,0,0);
    tempoPhase_=32;updateWorldProgression();tempoPhase_=40;updateWorldProgression();const float latchedKind=identityKind_;
    for(int section=0;section<5;++section){songSection_=section;++sectionRevision_;movementRevision_=-1;updateWorldProgression();if(identityKind_!=latchedKind||identityWeight_<.98f)passed=false;}
    songSection_=4;world_=0;worldBeatOrigin_=32;identityCycle_=-1;movementRevision_=-1;
    tempoPhase_=48;updateWorldProgression();if(identityStanza_||identityWeight_!=0)passed=false;
    worldBeatOrigin_=0;identityCycle_=-1;movementRevision_=-1;tempoPhase_=96;updateWorldProgression();tempoPhase_=104;updateWorldProgression();
    if(identityKind_==latchedKind||identityWeight_<.98f)passed=false;
    return passed?0:12;
}

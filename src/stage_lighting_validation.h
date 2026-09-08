int runStageLightingValidation(){
    const std::filesystem::path output="RareStage-lighting-validation";std::error_code error;std::filesystem::create_directories(output,error);if(error)return 16;
    std::ofstream report(output/"contracts.csv");report<<"check,passed\n";bool passed=true;
    auto check=[&](const char*name,bool ok){report<<name<<','<<ok<<'\n';report.flush();passed&=ok;};
    // Exercise the actual window handler and swap chain, not only key parsing.
    const RECT before=[&](){RECT r{};GetWindowRect(hwnd_,&r);return r;}();
    SendMessageW(hwnd_,WM_SYSKEYDOWN,VK_RETURN,LPARAM(1)<<29);
    RECT full{};GetWindowRect(hwnd_,&full);const RECT expected=displays_[settings_.monitor].bounds;
    BOOL exclusive=TRUE;swap_->GetFullscreenState(&exclusive,nullptr);
    check("alt_enter_borderless",fullscreen_&&!exclusive&&EqualRect(&full,&expected));
    SendMessageW(hwnd_,WM_SYSKEYDOWN,VK_RETURN,(LPARAM(1)<<29)|(LPARAM(1)<<30));check("alt_enter_repeat_ignored",fullscreen_);
    SendMessageW(hwnd_,WM_KEYDOWN,VK_F11,0);RECT restored{};GetWindowRect(hwnd_,&restored);
    check("f11_restore",!fullscreen_&&EqualRect(&before,&restored));
    const RECT fit=FitStageFrame(1920,1080,1080,1920);check("portrait_letterbox",fit.left==0&&fit.top==656&&fit.right==1080&&fit.bottom==1264);
    // Real swap chains on three hidden test windows with different shapes and negative coordinates.
    const std::vector<StageDisplay> synthetic{{{0,0,640,360},L"main"},{{-480,0,0,800},L"portrait"},{{640,0,1600,400},L"wide"}};
    check("mirror_create",outputs_.configure(synthetic,7,0)&&outputs_.count()==2);
    resize(640,360);const float color[4]={.23f,.51f,.77f,1};context_->ClearRenderTargetView(rtv_.Get(),color);
    check("mirror_same_frame_and_letterbox",outputs_.present(backBuffer_.Get(),true));
    check("mirror_remove",outputs_.configure(synthetic,0,0)&&outputs_.count()==0);
    check("mirror_reconnect",outputs_.configure(synthetic,2,0)&&outputs_.count()==1);
    context_->ClearRenderTargetView(rtv_.Get(),color);check("mirror_reconnect_frame",outputs_.present(backBuffer_.Get(),true));outputs_.clear();
    for(int hz:{60,120,144}){
        CornerChoreography dvd;float previousX=0,previousY=0,maxStep=0;bool exact=false;
        for(int frame=0;frame<=hz*5;++frame){float beat=frame*2.f/hz;dvd.update(beat,beat>=2&&beat<6,6-beat,beat>=6?1:0);maxStep=std::max(maxStep,std::hypot(dvd.x-previousX,dvd.y-previousY));previousX=dvd.x;previousY=dvd.y;if(std::abs(beat-6)<.001f)exact=std::abs(dvd.x)==1&&std::abs(dvd.y)==1;}
        check(("corner_forecast_"+std::to_string(hz)).c_str(),exact&&maxStep<.10f);
    }
    CornerChoreography surprise;surprise.update(0,false,0,0);surprise.update(.02f,false,0,1);check("unpredicted_drop_no_teleport",std::abs(surprise.x)<.01&&std::abs(surprise.y)<.01);
    for(int n=2;n<=102;++n)surprise.update(n*.01f,false,0,1);check("unpredicted_drop_arrives",std::abs(surprise.x)==1&&std::abs(surprise.y)==1);
    started_=Clock::now();world_=3;previousWorld_=3;transitionStarted_=-100;automatic_=true;frozen_=false;settings_.timeline=0;songSection_=DriveSection;worldChangeArmed_=false;nextWorldAt_=10;tempoPhase_=21.2f;worldCueBeat_=-1;AudioFrame audio{};
    updateTimedDirector(audio,10);check("timed_cycle_waits_for_bar",world_==3&&worldChangeArmed_&&worldCueBeat_==24);
    tempoPhase_=24;updateTimedDirector(audio,11.4f);check("steady_track_changes_world",world_!=3&&!worldChangeArmed_&&nextWorldAt_>11.4f);
    const int held=world_;frozen_=true;nextWorldAt_=0;tempoPhase_=100;updateTimedDirector(audio,100);check("manual_hold_preserved",world_==held);
    // Palette and motif matrix through production shaders, with the full wall present.
    settings_.quality=2;resize(640,360);std::ofstream matrix(output/"patterns.csv");matrix<<"world,palette,motif,mean_luma,coverage\n";
    D3D11_TEXTURE2D_DESC readDesc{};backBuffer_->GetDesc(&readDesc);readDesc.Usage=D3D11_USAGE_STAGING;readDesc.BindFlags=0;readDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;readDesc.MiscFlags=0;ComPtr<ID3D11Texture2D> readback;if(FAILED(device_->CreateTexture2D(&readDesc,nullptr,&readback)))return 16;
    for(int world:{0,3,9,15,16})for(int palette=0;palette<8;++palette)for(int motif=0;motif<16;++motif){
        Constants c{};c.resolution[0]=640;c.resolution[1]=360;c.scene=c.previousScene=float(world);c.transition=1;c.logoScale=1;c.palette=float(palette);c.patternSeed=float(motif);c.tempoPhase=2;c.time=c.tempoPhase*60.f/128.f;c.bass=.28f;c.mid=.35f;c.audioEnergy=.4f;c.masteringProfile=1;c.masterBrightness=.92f;c.cueDensity=1;c.songSection=4;
        for(int frame=0;frame<4;++frame){c.tempoPhase+=.25f;c.time+=.25f*60.f/128;pollGpuTimers();drawScene(c);}
        context_->CopyResource(readback.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE map{};if(FAILED(context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)))return 16;
        double sum=0;size_t lit=0,count=0;for(UINT y=2;y<360;y+=4)for(UINT x=2;x<640;x+=4){const auto*color=static_cast<const BYTE*>(map.pData)+size_t(y)*map.RowPitch+x*4;const float luma=(color[0]*.0722f+color[1]*.7152f+color[2]*.2126f)/255.f;sum+=luma;lit+=luma>.13f;++count;}context_->Unmap(readback.Get(),0);
        const double mean=sum/count,coverage=double(lit)/count;matrix<<world<<','<<palette<<','<<motif<<','<<mean<<','<<coverage<<'\n';if(world!=16&&(mean<.10||coverage<.45))passed=false;
        if((world==3||world==16)&&motif%4==0){const std::string name="world-"+std::to_string(world)+"-palette-"+std::to_string(palette)+"-motif-"+std::to_string(motif)+".png";if(!saveBackBufferPng(output/name))passed=false;}
    }
    check("palette_pattern_matrix",matrix.good());

    if(std::wcsstr(GetCommandLineW(),L"--preview")){
        std::filesystem::create_directories(output/"preview");CornerChoreography dvd;
        for(int frame=0;frame<1680;++frame){
            Constants c{};c.resolution[0]=640;c.resolution[1]=360;c.scene=c.previousScene=3;c.transition=1;c.logoScale=1;c.palette=4+frame/560.f;c.tempoPhase=frame/60.f*128.f/60.f;c.time=frame/60.f;c.patternSeed=c.previousPatternSeed=8;c.bass=.32f;c.mid=.35f;c.high=.18f;c.audioEnergy=.4f;c.masteringProfile=1;c.masterBrightness=.92f;c.cueDensity=1;c.songSection=4;
            c.kick=std::exp(-std::fmod(c.tempoPhase,1.f)*8)*.8f;c.percussion=std::exp(-std::fmod(c.tempoPhase*2,1.f)*12)*.6f;c.snare=std::exp(-std::fmod(c.tempoPhase+1,2.f)*12)*.6f;
            if(frame>=180&&frame<420){float stanza=(frame-180)/240.f;c.identityWeight=liquidProgress(stanza/.35f)*(1-liquidProgress((stanza-.7f)/.3f));}
            if(frame>=600&&frame<720){c.scene=0;c.previousScene=3;c.transition=(frame-600)/120.f;}
            else if(frame>=720&&frame<840)c.scene=c.previousScene=0;
            else if(frame>=840&&frame<960){c.scene=16;c.previousScene=0;c.transition=(frame-840)/120.f;}
            else if(frame>=960&&frame<1200)c.scene=c.previousScene=16;
            else if(frame>=1200&&frame<1320){c.scene=5;c.previousScene=16;c.transition=(frame-1200)/120.f;}
            else if(frame>=1320)c.scene=c.previousScene=5;
            const float dropBeat=1560.f/60.f*128.f/60.f;
            dvd.update(c.tempoPhase,frame>=1440&&frame<1560,dropBeat-c.tempoPhase,frame>=1560?1:0);c.dvdX=dvd.x;c.dvdY=dvd.y;c.dvdCorner=dvd.impact(c.tempoPhase);c.dvdControlled=1;
            if(frame>=1560){c.dropPulse=std::exp(-(frame-1560)/60.f*4);c.songSection=2;}
            pollGpuTimers();drawScene(c);wchar_t name[40]{};swprintf_s(name,L"frame-%04d.png",frame);if(!saveBackBufferPng(output/"preview"/name))return 16;
        }
    }
    return passed?0:17;
}

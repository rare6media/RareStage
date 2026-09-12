// Included in App: this fixture exercises production selection, cue timing and rendering.
int runProfessionalValidation(){
 const std::filesystem::path output="RareStage-professional-validation";std::filesystem::create_directories(output/"preview");
 std::ofstream checks(output/"contracts.csv");checks<<"check,passed\n";bool passed=true;
 auto check=[&](const char*name,bool ok){checks<<name<<','<<ok<<'\n';checks.flush();passed&=ok;};
 rarestage::show::Director d;d.visit(3);d.visit(9);
 rarestage::show::Context context;context.current=9;context.section=4;context.energy=.5f;context.fit.fill(1);
 auto route=d.plan(context);check("contrast_after_two_surface_visits",rarestage::show::Scenes[route.next].motion!=0);
 std::array<bool,rarestage::show::SceneCount>seen{};std::ofstream decisions(output/"route-trace.csv");decisions<<"cue,section,from,next,then,compatibility\n";
 for(int category=0;category<5;++category){
  rarestage::show::Director director;context.category=category;context.current=3;director.visit(3);
  for(int cue=0;cue<100;++cue){context.section=(cue/4)%5;context.energy=context.section==2?.65f:context.section==3?.22f:.48f;context.trend=context.section==1?.08f:context.section==3?-.08f:0;
   for(int w=0;w<rarestage::show::SceneCount;++w)context.fit[w]=1.f-std::abs(context.energy-rarestage::show::Scenes[w].energy);
   const auto plan=director.plan(context);bool legal=plan.next>=0&&plan.next<rarestage::show::SceneCount&&plan.then>=0&&plan.then<rarestage::show::SceneCount&&plan.next!=context.current&&plan.then!=plan.next;
   if(category)legal&=rarestage::show::Scenes[plan.next].category==category-1&&rarestage::show::Scenes[plan.then].category==category-1;
   if(!legal)passed=false;if(!category)seen[plan.next]=true;
   decisions<<cue<<','<<context.section<<','<<context.current<<','<<plan.next<<','<<plan.then<<','<<rarestage::show::Compatibility(context.current,plan.next)<<'\n';
   director.visit(plan.next);context.current=plan.next;
  }
 }
 check("routes_legal_all_categories",passed);check("route_variety",std::count(seen.begin(),seen.end(),true)>=12);
 // Route lock uses real App code. A single hit cannot replace a rehearsed route.
 started_=Clock::now();automatic_=true;frozen_=false;settings_.sceneCategory=0;settings_.timeline=0;world_=3;transitionStarted_=-100;tempoPhase_=100;nextWorldAt_=2;route_={};routeSection_=-1;sectionLongEnergy_=.4f;sectionShortEnergy_=.42f;songSection_=4;
 AudioFrame audio{};audio.bass=.4f;audio.mid=.35f;refreshShowPlan(audio);const int locked=route_.next;
 tempoPhase_=112;audio.kick=audio.onset=audio.beat=1;refreshShowPlan(audio);check("eight_beat_rehearsal_lock",route_.next==locked);
 settings_.sceneCategory=2;refreshShowPlan(audio);check("filter_invalidates_rehearsal",worldFamily(route_.next)==1);
 settings_.sceneCategory=0;route_={};routeSection_=-1;
 songSection_=BuildSection;sectionRevision_=10;tempoPhase_=160;nextWorldAt_=60;refreshShowPlan(audio);
 const int preparedDrop=composer_.branches()[1].route.next;preparedWorld_=-1;predictedSection_=DropSection;preparePredictedWorld(audio,elapsed());check("forecast_uses_same_composer_branch",preparedWorld_==preparedDrop);
 songSection_=DropSection;++sectionRevision_;tempoPhase_=164;refreshShowPlan(audio);
 check("confirmed_branch_preserved",route_.next==preparedDrop&&preparedOrChosenWorld(audio)==preparedDrop);
 selectWorld(world_);check("manual_cue_invalidates_composer",!composer_.valid(world_,settings_.sceneCategory,sectionRevision_,tempoPhase_));
 using rarestage::control::Command;using rarestage::control::PackCommand;using rarestage::control::CommandStatus;
 settings_.syncMode=0;const bool autoBefore=automatic_;
 handleControlCommand(Command::HoldTheme,PackCommand(1,100));check("theme_hold_command",themeMemory_.held()&&automatic_==autoBefore&&lastCommandStatus_==CommandStatus::Accepted);
 handleControlCommand(Command::HoldTheme,PackCommand(2,101));check("invalid_theme_hold_rejected",themeMemory_.held()&&lastCommandStatus_==CommandStatus::Rejected);
 const int themeBefore=themeMemory_.current();handleControlCommand(Command::NextTheme,PackCommand(0,102));themeMemory_.update(192,true,true);check("next_theme_preserves_hold",themeMemory_.current()!=themeBefore&&themeMemory_.held());
 settings_.syncMode=2;handleControlCommand(Command::NextTheme,PackCommand(0,103));check("follower_theme_control_rejected",lastCommandStatus_==CommandStatus::Rejected);
 settings_.syncMode=0;themeMemory_.hold(false);
 world_=34;tempoPhase_=64;transitionStarted_=-100;identityStanza_=false;phraseConductor_.begin(34,0,0);
 handleControlCommand(Command::SelectProgram,PackCommand(8,104));
 check("catalog_pattern_blends_and_holds",lastCommandStatus_==CommandStatus::Accepted&&programHeld_&&phraseConductor_.sample(64).from==0&&phraseConductor_.sample(64).to==8&&phraseConductor_.sample(64).blend==0&&automatic_==autoBefore);
 handleControlCommand(Command::SelectProgram,PackCommand(9,105));check("inflight_program_change_rejected",lastCommandStatus_==CommandStatus::Rejected);
 tempoPhase_=72;handleControlCommand(Command::SelectProgram,PackCommand(10,106));check("out_of_range_program_rejected",lastCommandStatus_==CommandStatus::Rejected);
 identityStanza_=true;handleControlCommand(Command::SelectProgram,PackCommand(2,107));check("identity_protects_program_cue",lastCommandStatus_==CommandStatus::Rejected);identityStanza_=false;
 transitionStarted_=elapsed();handleControlCommand(Command::SelectProgram,PackCommand(2,108));check("handoff_protects_program_cue",lastCommandStatus_==CommandStatus::Rejected);transitionStarted_=-100;
 settings_.syncMode=2;handleControlCommand(Command::SelectProgram,PackCommand(2,109));check("follower_program_selection_rejected",lastCommandStatus_==CommandStatus::Rejected);settings_.syncMode=0;
 handleControlCommand(Command::ReleaseProgram,PackCommand(0,110));check("pattern_director_resumes",lastCommandStatus_==CommandStatus::Accepted&&!programHeld_);
 // Every ordered pair, at 60 fps over a two-second handoff, with authored geometry flow.
 settings_.quality=2;settings_.spout=0;settings_.vsync=0;settings_.djName=L"RARESIX";if(!rebuildIdentityTexture())return 18;resize(320,180);
 D3D11_TEXTURE2D_DESC desc{};backBuffer_->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
 ComPtr<ID3D11Texture2D>readback;if(FAILED(device_->CreateTexture2D(&desc,nullptr,&readback)))return 18;
 auto pixels=[&](){std::vector<float>result;context_->CopyResource(readback.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE map{};if(FAILED(context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)))return result;
  for(UINT y=0;y<height_;y+=4)for(UINT x=0;x<width_;x+=4){const BYTE*p=static_cast<const BYTE*>(map.pData)+size_t(y)*map.RowPitch+x*4;for(int k=0;k<3;++k)result.push_back(p[k]/255.f);}context_->Unmap(readback.Get(),0);return result;};
 auto delta=[](const std::vector<float>&a,const std::vector<float>&b){if(a.empty()||a.size()!=b.size())return 1.0;double sum=0;for(size_t i=0;i<a.size();++i)sum+=std::abs(a[i]-b[i]);return sum/a.size();};
 auto fixture=[&](float seconds){Constants c{};c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);c.time=seconds;c.tempoPhase=seconds*128.f/60.f;c.logoScale=1;c.palette=5;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.songSection=4;c.bass=.3f;c.mid=.35f;c.audioEnergy=.4f;c.patternSeed=36;c.previousPatternSeed=12;c.transition=1;return c;};
 std::ofstream pairs(output/"all-pairs.csv");pairs<<"from,to,max_temporal_delta,endpoint_delta,near_endpoint_delta\n";
 std::ofstream nativeFrames(output/"native-frame-deltas.csv");nativeFrames<<"from,to,frame,delta\n";
 bool pairPassed=true;
 for(int from=0;from<WorldCount;++from)for(int to=0;to<WorldCount;++to){if(from==to)continue;
  double maximum=0,endError=0,nearError=0;std::vector<float>previous;const auto&theme=rarestage::show::Themes[(from+to)%6].shape;
  for(int frame=0;frame<=120;++frame){if(std::wcsstr(GetCommandLineW(),L"--endpoints-only")&&frame!=0&&frame!=120)continue;Constants c=fixture(14.f+frame/60.f);c.themeBraid=theme[0];c.themeFan=theme[1];c.themeOrbit=theme[2];c.themeAccent=theme[3];c.previousScene=float(from);c.scene=float(to);c.transition=frame/120.f;c.transitionStyle=float(rarestage::show::HandoffStyle(from,to));drawScene(c);auto current=pixels();if(!previous.empty()&&!std::wcsstr(GetCommandLineW(),L"--endpoints-only"))maximum=std::max(maximum,delta(current,previous));if(!previous.empty()&&rarestage::show::NativeBridge(from,to)&&(from>=21||to>=21))nativeFrames<<from<<','<<to<<','<<frame<<','<<delta(current,previous)<<'\n';previous=current;
   if(frame==0||frame==120){Constants endpoint=c;endpoint.scene=endpoint.previousScene=frame==0?c.previousScene:c.scene;endpoint.patternSeed=frame==0?c.previousPatternSeed:c.patternSeed;endpoint.transition=1;drawScene(endpoint);endError=std::max(endError,delta(current,pixels()));c.transition=frame==0?.0001f:.9999f;drawScene(c);const double neighborDifference=delta(current,pixels());nearError=std::max(nearError,neighborDifference);
    if(neighborDifference>.0025){const std::string tag=std::to_string(from)+"-"+std::to_string(to)+"-"+std::to_string(frame);saveBackBufferPng(output/(tag+"-near.png"));drawScene(endpoint);saveBackBufferPng(output/(tag+"-endpoint.png"));}
}
  }
  pairs<<from<<','<<to<<','<<maximum<<','<<endError<<','<<nearError<<'\n';pairs.flush();if(maximum>.06||endError>.00001||nearError>.003)pairPassed=false;
 }
 check("all_552_ordered_scene_pairs",pairPassed);
 if(std::wcsstr(GetCommandLineW(),L"--endpoints-only"))return pairPassed?0:19;
 std::ofstream motion(output/"sculpture-motion.csv");motion<<"world,max_silent_delta,mean_motion,mean_hit_delta\n";
 bool motionPassed=true;
 for(int world:{6,8,10,12,16,18,19,20,21,22,23}){std::vector<float>previous;double maximum=0,sum=0,hits=0;
  for(int frame=0;frame<1920;++frame){Constants c=fixture(frame/60.f);c.scene=c.previousScene=float(world);drawScene(c);auto current=pixels();if(!previous.empty()){double change=delta(current,previous);sum+=change;maximum=std::max(maximum,change);}previous=current;
   if(frame%60==0){c.kick=c.beat=c.snare=1;drawScene(c);hits+=delta(current,pixels());}
  }
  motion<<world<<','<<maximum<<','<<sum/1919<<','<<hits/32<<'\n';if(maximum>.04||sum/1919<.0001||hits/32>.035)motionPassed=false;
 }
 check("sustained_motion_and_restrained_hits",motionPassed);

 // New native bridges at landscape, portrait and wide aspect ratios, across all themes.
 std::ofstream aspects(output/"composer-aspects.csv");aspects<<"width,height,from,to,theme,max_delta,endpoint_delta,near_endpoint_delta\n";
 bool aspectsPassed=true;
 for(const auto size:std::array<std::array<UINT,2>,3>{{{320,180},{180,320},{480,180}}}){
  readback.Reset();resize(size[0],size[1]);backBuffer_->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
  if(FAILED(device_->CreateTexture2D(&desc,nullptr,&readback)))return 18;
  for(const auto pair:std::array<std::array<int,2>,7>{{{3,21},{21,3},{18,22},{22,18},{22,23},{23,22},{23,18}}})for(int themeIndex=0;themeIndex<6;++themeIndex){
   const auto&shape=rarestage::show::Themes[themeIndex].shape;double maximum=0,endpointError=0,nearError=0;std::vector<float>previous;
   for(int frame=0;frame<=120;++frame){Constants c=fixture(12.f+frame/60.f);c.scene=float(pair[1]);c.previousScene=float(pair[0]);c.transition=frame/120.f;
    c.themeBraid=shape[0];c.themeFan=shape[1];c.themeOrbit=shape[2];c.themeAccent=shape[3];drawScene(c);auto current=pixels();if(!previous.empty())maximum=std::max(maximum,delta(previous,current));previous=current;
    if(frame==0||frame==120){Constants end=c;end.scene=end.previousScene=frame==0?c.previousScene:c.scene;end.patternSeed=frame==0?c.previousPatternSeed:c.patternSeed;end.transition=1;drawScene(end);endpointError=std::max(endpointError,delta(current,pixels()));c.transition=frame==0?.0001f:.9999f;drawScene(c);nearError=std::max(nearError,delta(current,pixels()));}
    if(frame==60&&themeIndex==2&&pair[0]!=23)saveBackBufferPng(output/("bridge-"+std::to_string(pair[0])+"-"+std::to_string(pair[1])+"-"+std::to_string(size[0])+".png"));
   }
   aspects<<size[0]<<','<<size[1]<<','<<pair[0]<<','<<pair[1]<<','<<themeIndex<<','<<maximum<<','<<endpointError<<','<<nearError<<'\n';aspects.flush();aspectsPassed&=maximum<.06&&endpointError<.00001&&nearError<.003;
  }
 }
 check("native_bridges_all_themes_three_aspects",aspectsPassed);
 // A reviewable 40-second reel of the actual shaders; palette changes stay continuous.
 resize(960,540);
 const int reelWorlds[]={3,16,18,19,20,3};
 for(int frame=0;std::wcsstr(GetCommandLineW(),L"--preview")&&frame<2400;++frame){Constants c=fixture(frame/60.f);int chapter=std::min(4,frame/480),local=frame%480;c.scene=c.previousScene=float(reelWorlds[chapter]);c.palette=4+frame/800.f;
  if(local>=360){c.scene=float(reelWorlds[chapter+1]);c.transition=(local-360)/120.f;c.transitionStyle=float(rarestage::show::HandoffStyle(reelWorlds[chapter],reelWorlds[chapter+1]));}
  if(local>=100&&local<300){float stanza=(local-100)/200.f;c.identityWeight=liquidProgress(stanza/.35f)*(1-liquidProgress((stanza-.7f)/.3f));c.identityKind=float(chapter%2);}
  c.kick=std::exp(-std::fmod(c.tempoPhase,1.f)*8)*.8f;c.snare=std::exp(-std::fmod(c.tempoPhase+1,2.f)*12)*.6f;drawScene(c);wchar_t name[40]{};swprintf_s(name,L"frame-%04d.png",frame);if(!saveBackBufferPng(output/"preview"/name))return 18;
 }
 return passed?0:19;
}

// Included in App: this fixture exercises production selection, cue timing and rendering.
int runProfessionalValidation(){
 const std::filesystem::path output="RareStage-professional-validation";std::filesystem::create_directories(output/"preview");
 std::ofstream checks(output/"contracts.csv");checks<<"check,passed\n";bool passed=true;
 auto check=[&](const char*name,bool ok){checks<<name<<','<<ok<<'\n';checks.flush();passed&=ok;};
 rarestage::show::Director d;d.visit(3);d.visit(9);
 rarestage::show::Context context;context.current=9;context.section=4;context.energy=.5f;context.fit.fill(1);
 auto route=d.plan(context);check("contrast_after_two_surface_visits",rarestage::show::Scenes[route.next].motion!=0);
 std::array<bool,21>seen{};std::ofstream decisions(output/"route-trace.csv");decisions<<"cue,section,from,next,then,compatibility\n";
 for(int category=0;category<5;++category){
  rarestage::show::Director director;context.category=category;context.current=3;director.visit(3);
  for(int cue=0;cue<100;++cue){context.section=(cue/4)%5;context.energy=context.section==2?.65f:context.section==3?.22f:.48f;context.trend=context.section==1?.08f:context.section==3?-.08f:0;
   for(int w=0;w<21;++w)context.fit[w]=1.f-std::abs(context.energy-rarestage::show::Scenes[w].energy);
   const auto plan=director.plan(context);bool legal=plan.next>=0&&plan.next<21&&plan.then>=0&&plan.then<21&&plan.next!=context.current&&plan.then!=plan.next;
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
 // Every ordered pair, at 60 fps over a two-second handoff, with authored geometry flow.
 settings_.quality=2;settings_.spout=0;settings_.vsync=0;settings_.djName=L"RARESIX";if(!rebuildIdentityTexture())return 18;resize(320,180);
 D3D11_TEXTURE2D_DESC desc{};backBuffer_->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
 ComPtr<ID3D11Texture2D>readback;if(FAILED(device_->CreateTexture2D(&desc,nullptr,&readback)))return 18;
 auto pixels=[&](){std::vector<float>result;context_->CopyResource(readback.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE map{};if(FAILED(context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)))return result;
  for(UINT y=0;y<height_;y+=4)for(UINT x=0;x<width_;x+=4){const BYTE*p=static_cast<const BYTE*>(map.pData)+size_t(y)*map.RowPitch+x*4;for(int k=0;k<3;++k)result.push_back(p[k]/255.f);}context_->Unmap(readback.Get(),0);return result;};
 auto delta=[](const std::vector<float>&a,const std::vector<float>&b){if(a.empty()||a.size()!=b.size())return 1.0;double sum=0;for(size_t i=0;i<a.size();++i)sum+=std::abs(a[i]-b[i]);return sum/a.size();};
 auto fixture=[&](float seconds){Constants c{};c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);c.time=seconds;c.tempoPhase=seconds*128.f/60.f;c.logoScale=1;c.palette=5;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.songSection=4;c.bass=.3f;c.mid=.35f;c.audioEnergy=.4f;c.patternSeed=36;c.previousPatternSeed=12;c.transition=1;return c;};
 std::ofstream pairs(output/"all-pairs.csv");pairs<<"from,to,max_temporal_delta,endpoint_delta,near_endpoint_delta\n";
 bool pairPassed=true;
 for(int from=0;from<WorldCount;++from)for(int to=0;to<WorldCount;++to){if(from==to)continue;
  double maximum=0,endError=0,nearError=0;std::vector<float>previous;
  for(int frame=0;frame<=120;++frame){if(std::wcsstr(GetCommandLineW(),L"--endpoints-only")&&frame!=0&&frame!=120)continue;Constants c=fixture(14.f+frame/60.f);c.previousScene=float(from);c.scene=float(to);c.transition=frame/120.f;c.transitionStyle=float(rarestage::show::HandoffStyle(from,to));drawScene(c);auto current=pixels();if(!previous.empty()&&!std::wcsstr(GetCommandLineW(),L"--endpoints-only"))maximum=std::max(maximum,delta(current,previous));previous=current;
   if(frame==0||frame==120){Constants endpoint=c;endpoint.scene=endpoint.previousScene=frame==0?c.previousScene:c.scene;endpoint.patternSeed=frame==0?c.previousPatternSeed:c.patternSeed;endpoint.transition=1;drawScene(endpoint);endError=std::max(endError,delta(current,pixels()));c.transition=frame==0?.0001f:.9999f;drawScene(c);const double neighborDifference=delta(current,pixels());nearError=std::max(nearError,neighborDifference);
    if(neighborDifference>.0025){const std::string tag=std::to_string(from)+"-"+std::to_string(to)+"-"+std::to_string(frame);saveBackBufferPng(output/(tag+"-near.png"));drawScene(endpoint);saveBackBufferPng(output/(tag+"-endpoint.png"));}
}
  }
  pairs<<from<<','<<to<<','<<maximum<<','<<endError<<','<<nearError<<'\n';pairs.flush();if(maximum>.06||endError>.00001||nearError>.003)pairPassed=false;
 }
 check("all_420_ordered_scene_pairs",pairPassed);
 if(std::wcsstr(GetCommandLineW(),L"--endpoints-only"))return pairPassed?0:19;
 std::ofstream motion(output/"sculpture-motion.csv");motion<<"world,max_silent_delta,mean_motion,mean_hit_delta\n";
 bool motionPassed=true;
 for(int world:{16,18,19,20}){std::vector<float>previous;double maximum=0,sum=0,hits=0;
  for(int frame=0;frame<1920;++frame){Constants c=fixture(frame/60.f);c.scene=c.previousScene=float(world);drawScene(c);auto current=pixels();if(!previous.empty()){double change=delta(current,previous);sum+=change;maximum=std::max(maximum,change);}previous=current;
   if(frame%60==0){c.kick=c.beat=c.snare=1;drawScene(c);hits+=delta(current,pixels());}
  }
  motion<<world<<','<<maximum<<','<<sum/1919<<','<<hits/32<<'\n';if(maximum>.04||sum/1919<.0001||hits/32>.035)motionPassed=false;
 }
 check("sustained_motion_and_restrained_hits",motionPassed);
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

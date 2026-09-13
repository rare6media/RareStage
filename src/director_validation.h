// Synthetic inputs, production scheduler and production offscreen render path.
int runDirectorValidation(){
 using namespace rarestage::show;
 const std::filesystem::path output="RareStage-director-validation";std::filesystem::create_directories(output/"stills");
 std::ofstream checks(output/"contracts.csv"),metrics(output/"rehearsals.csv");checks<<"check,passed\n";metrics<<"from,to,program,sample,luma,coverage,gpu_ms\n";
 bool passed=true;auto check=[&](const char*label,bool ok){checks<<label<<','<<ok<<'\n';checks.flush();passed&=ok;};
 settings_.spout=0;settings_.quality=2;settings_.vsync=0;settings_.sceneCategory=0;settings_.timeline=0;settings_.syncMode=0;resize(384,216);
 settings_.djName=L"RARESIX";if(!rebuildIdentityTexture())return 36;
 started_=Clock::now();world_=38;previousWorld_=38;worldBeatOrigin_=0;transitionStarted_=-100;automatic_=true;frozen_=false;identityStanza_=false;identityWeight_=0;programHeld_=false;tempoPhase_=104;visualBpm_=120;phraseConductor_.begin(38,0,0);phraseConductor_.select(1,100);sectionEvidence_=.7f;songSection_=4;worldChangeArmed_=false;nextWorldAt_=-1;route_={};
 AudioFrame audio{};audio.bass=.4f;audio.mid=.3f;updateTimedDirector(audio,elapsed());float exitBeat=worldCueBeat_;
 check("automatic_exit_waits_for_program_payoff",world_==38&&worldChangeArmed_&&exitBeat>=phraseConductor_.settledAt()+PatternContract(38,1).payoff);
 for(int frame=0;frame<60;++frame){tempoPhase_=104+frame*.05f;updateTimedDirector(audio,elapsed());}
 check("armed_exit_deadline_does_not_chase_the_clock",worldCueBeat_==exitBeat&&world_==38);
 tempoPhase_=exitBeat;frozen_=true;updateTimedDirector(audio,elapsed());check("operator_hold_precedes_scene_deadline",world_==38);frozen_=false;programHeld_=true;updateTimedDirector(audio,elapsed());check("held_pattern_precedes_scene_deadline",world_==38);programHeld_=false;updateTimedDirector(audio,elapsed());check("release_commits_prepared_continuation",world_!=38);
 world_=previousWorld_=38;transitionStarted_=-100;identityStanza_=false;tempoPhase_=130;phraseConductor_.begin(38,0,0);worldChangeArmed_=true;worldCueBeat_=130;nextWorldAt_=-1;
 handleControlCommand(rarestage::control::Command::SelectProgram,rarestage::control::PackCommand(2,300));
 check("manual_program_invalidates_old_departure",programHeld_&&!worldChangeArmed_&&worldCueBeat_<0);
 tempoPhase_=132;handleControlCommand(rarestage::control::Command::ReleaseProgram,rarestage::control::PackCommand(0,301));updateTimedDirector(audio,elapsed());
 check("resuming_during_blend_finishes_new_program",world_==38&&worldCueBeat_>=phraseConductor_.settledAt()+PatternContract(38,2).payoff);
 worldChangeArmed_=false;world_=previousWorld_=38;transitionStarted_=-100;worldBeatOrigin_=0;tempoPhase_=64;phraseConductor_.begin(38,0,0);identityStanza_=false;identityWeight_=0;frozen_=programHeld_=blackout_=false;route_.next=40;route_.alternative=41;nextWorldAt_=elapsed()+12;gpuFrameMs_=.3f;gpuPeakMs_=.4f;
 auto fixture=[&](int world,float beat,int mode){Constants c{};c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);c.scene=c.previousScene=float(world);c.time=beat*.5f;c.tempoPhase=beat;c.transition=1;c.logoScale=1;c.palette=2;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.phrase=c.previousPhrase={float(mode),float(mode),1,PhraseFamily(world)>=0?1.f:0.f,1,1,0,0};return c;};
 auto drain=[&](){context_->Flush();const auto deadline=Clock::now()+std::chrono::seconds(3);while(rehearsalSurface_.pending&&Clock::now()<deadline){pollVisualRehearsal(elapsed());Sleep(1);}return !rehearsalSurface_.pending&&!rehearsalSurface_.failed;};
 check("offscreen_resources_created",prepareRehearsalSurface());if(!passed)return 36;
 auto live=fixture(38,64,0);drawScene(live,false);if(!saveBackBufferPng(output/"stills/live-before.png"))return 36;
 auto*liveBuffer=backBuffer_.Get();const auto liveWidth=width_,liveHeight=height_;const auto timerWrite=gpuTimerWrite_;
 for(int source:{3,35,38,39,40,41}){
  world_=source;phraseConductor_.begin(source,0,0);const auto key=visualContext();
  int first=source==38?40:source==39?41:38,second=source==40?38:source==41?39:41;
  visualRehearsal_.begin(key,tempoPhase_,first,second,1,3);
  for(int candidate=0;candidate<2;++candidate)for(int sample=0;sample<5;++sample){const auto&choice=visualRehearsal_.candidates()[candidate];auto c=fixture(choice.scene,80+sample*2.f,choice.program);c.previousScene=float(source);c.previousPhrase=fixture(source,c.tempoPhase,0).phrase;c.transition=sample/4.f;
   submitVisualRehearsal(c,visualRehearsal_.epoch(),candidate,sample,elapsed());if(!drain())return 36;
   check("live_target_restored_after_rehearsal",backBuffer_.Get()==liveBuffer&&width_==liveWidth&&height_==liveHeight&&gpuTimerWrite_==timerWrite);
   const auto&image=visualRehearsal_.candidates()[candidate].frames[sample];metrics<<source<<','<<choice.scene<<','<<choice.program<<','<<sample<<','<<image.luma<<','<<image.coverage<<','<<rehearsalSurface_.lastGpuMs<<'\n';metrics.flush();
   swapRehearsalSurface();bool saved=saveBackBufferPng(output/"stills"/("rehearsal-"+std::to_string(source)+"-"+std::to_string(choice.scene)+"-"+std::to_string(sample)+".png"));swapRehearsalSurface();if(!saved)return 36;
  }
  check("both_candidates_complete_five_production_samples",visualRehearsal_.done());
 }
 drawScene(live,false);if(!saveBackBufferPng(output/"stills/live-after.png"))return 36;
 auto bytes=[](const std::filesystem::path&path){std::ifstream input(path,std::ios::binary);return std::vector<char>(std::istreambuf_iterator<char>(input),{});};
 check("background_work_does_not_change_live_pixels",bytes(output/"stills/live-before.png")==bytes(output/"stills/live-after.png"));
 world_=38;phraseConductor_.begin(38,0,0);visualRehearsal_.begin(visualContext(),tempoPhase_,40,41,0,0);auto stale=visualRehearsal_.epoch();submitVisualRehearsal(live,stale,0,0,elapsed());visualRehearsal_.cancel();if(!drain())return 36;check("cancelled_gpu_result_cannot_enter_new_context",visualRehearsal_.candidates()[0].samples==0);
 // Actual service path, with explicit synthetic budget evidence. Readback is
 // drained only by the fixture; production polling always uses DONOTFLUSH.
 visualRehearsal_.cancel();rehearsalSurface_.submittedAt=-100;rehearsalSurface_.lastGpuMs=0;nextWorldAt_=elapsed()+12;gpuFrameMs_=.3f;gpuPeakMs_=.4f;world_=38;route_.next=40;route_.alternative=41;
 auto before=visualRehearsal_.submitted;serviceVisualRehearsal(live,elapsed());check("idle_capacity_submits_bounded_work",visualRehearsal_.submitted==before+1);if(!drain())return 36;
 rehearsalSurface_.submittedAt=-100;gpuFrameMs_=20;gpuPeakMs_=25;before=visualRehearsal_.submitted;serviceVisualRehearsal(live,elapsed());check("gpu_pressure_skips_rehearsal",visualRehearsal_.submitted==before);
 gpuFrameMs_=.3f;gpuPeakMs_=.4f;frozen_=true;serviceVisualRehearsal(live,elapsed());check("manual_hold_cancels_rehearsal",!visualRehearsal_.valid(visualContext(),tempoPhase_));frozen_=false;
 // Exercise a complete batch through the coordinator with a fixed departure.
 // Cadence/headroom inputs are controlled here; actual samples still use the GPU.
 world_=previousWorld_=38;tempoPhase_=64;transitionStarted_=-100;worldChangeArmed_=true;worldCueBeat_=96;
 phraseConductor_.begin(38,0,0);route_.next=40;route_.alternative=41;identityStanza_=false;
 before=visualRehearsal_.submitted;
 for(int frame=0;frame<10;++frame){tempoPhase_=64+frame*.6f;rehearsalSurface_.submittedAt=elapsed()-.3f;rehearsalSurface_.lastGpuMs=0;
  serviceVisualRehearsal(fixture(38,tempoPhase_,0),elapsed());if(!drain())return 36;
 }
 check("service_completes_both_candidates_in_one_stable_context",visualRehearsal_.done()&&visualRehearsal_.submitted==before+10&&visualRehearsal_.valid(visualContext(),tempoPhase_));
 Context actual;applyRouteRehearsal(actual);
 check("completed_gpu_candidates_bound_the_actual_app_route",actual.boundedNext&&actual.shortlist[40]&&actual.shortlist[41]&&std::count(actual.shortlist.begin(),actual.shortlist.end(),true)==2);
 const int entry=visualRehearsal_.entry(40,visualContext(),tempoPhase_);tempoPhase_=worldCueBeat_;selectWorld(40,true);
 check("actual_scene_commit_uses_its_rehearsed_entry",entry>=0&&world_==40&&int(phraseConductor_.sample(tempoPhase_).to)==entry&&!visualRehearsal_.valid(visualContext(),tempoPhase_));
 bool musicFit=true;for(int scene=38;scene<=41;++scene){AudioFrame quiet{},bass{},mid{},high{};bass.bass=mid.mid=high.high=1;
  const float baseline=baseWorldFitScore(scene,quiet,.6f,false,4);
  musicFit&=baseWorldFitScore(scene,bass,.6f,false,4)>baseline+.02f&&baseWorldFitScore(scene,mid,.6f,false,4)>baseline+.02f&&baseWorldFitScore(scene,high,.6f,false,4)>baseline+.02f;
  musicFit&=std::abs(baseWorldFitScore(scene,quiet,.6f,false,0)-baseline)>.02f;
 }check("new_scenes_have_authored_instrument_and_section_affinities",musicFit);
 // One hour of prescribed musical chapters, with the actual App scheduler and
 // conductor. This checks coordination, not audio classification or listening.
 std::ofstream schedule(output/"synthetic-set.csv");schedule<<"seconds,beat,section,from,to,required_payoff,identity_weight\n";
 world_=previousWorld_=38;worldBeatOrigin_=0;worldStartedAt_=0;transitionStarted_=-100;tempoPhase_=0;visualBpm_=120;nextWorldAt_=44;
 phraseConductor_.begin(38,0,0);showDirector_={};composer_.invalidate();route_={};routeSection_=-1;sequence_={};phraseMemory_={};identitySpacing_={};
 visualRehearsal_.cancel();worldChangeArmed_=false;worldCueBeat_=-1;pixelExitWorld_=queuedWorld_=-1;settings_.sceneSeconds=48;identityCycle_=movementRevision_=-1;identityStanza_=false;identityWeight_=0;
 int previousSection=-1,visits=0;float lastVisit=0;bool deadlines=true,holds=true,progress=true;std::array<bool,SceneCount>setScenes{};
 for(int tick=0;tick<28800;++tick){const float seconds=tick*.125f;tempoPhase_=seconds*2;started_=Clock::now()-std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(seconds));
  const int phrase=int(tempoPhase_/32),section=std::array<int,8>{0,4,4,1,2,4,3,4}[phrase%8];
  if(section!=previousSection){enterSongSection(section,seconds);previousSection=section;}sectionEvidence_=phrase%7==0?.15f:.75f;sectionLongEnergy_=sectionShortEnergy_=section==2?.72f:section==3?.18f:.45f;
  frozen_=std::fmod(seconds,240.f)>=180&&std::fmod(seconds,240.f)<188;programHeld_=std::fmod(seconds,300.f)>=252&&std::fmod(seconds,300.f)<260;
  const int from=world_;const float required=phraseConductor_.settledAt()+PatternContract(from,int(phraseConductor_.sample(tempoPhase_).to)).payoff;const float identity=identityWeight_;
  serviceQueuedWorld();cornerMotion_.update(seconds,false,0,dropCount_);servicePixelExit(audio,seconds);updateTimedDirector(audio,seconds);updateWorldProgression();
  const bool changed=from!=world_;holds&=!(changed&&(frozen_||programHeld_));
  if(changed){++visits;setScenes[world_]=true;lastVisit=seconds;deadlines&=from==5||tempoPhase_+.001f>=required;schedule<<seconds<<','<<tempoPhase_<<','<<section<<','<<from<<','<<world_<<','<<required<<','<<identity<<'\n';}
  progress&=seconds-lastVisit<180&&AutoScene(world_);
 }
 check("one_hour_prescribed_set_finishes_program_payoffs",deadlines);
 check("one_hour_prescribed_set_respects_operator_holds",holds);
 check("one_hour_prescribed_set_keeps_departing_and_excludes_rejected_scenes",progress&&visits>35&&std::count(setScenes.begin(),setScenes.end(),true)>15);
 return passed?0:37;
}

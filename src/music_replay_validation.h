// Offline 48 kHz mono float PCM. Decoding is external; the production analyzer,
// tempo clock, section detector and phrase conductor run on sample time.
int runMusicReplayValidation(){
 using namespace rarestage::show;
 const std::filesystem::path output="RareStage-phrase-validation",input=output/"audio";
 std::filesystem::create_directories(output/"music");if(!std::filesystem::exists(input))return 27;
 std::vector<std::filesystem::path>tracks;for(const auto&entry:std::filesystem::directory_iterator(input))if(entry.is_regular_file()&&entry.path().extension()==".f32")tracks.push_back(entry.path());std::sort(tracks.begin(),tracks.end());if(tracks.empty())return 27;
 settings_.quality=2;settings_.spout=0;settings_.vsync=0;settings_.djName=L"RARESIX";if(!rebuildIdentityTexture())return 27;resize(320,180);
 std::ofstream summary(output/"music-summary.csv");summary<<"track,seconds,hops,section_changes,program_changes,mean_tempo_confidence,mean_bpm,min_change_beats,valid\n";
 PhraseMemory memory;bool passed=true;int trackIndex=0;float setBeat=0;
 for(const auto&path:tracks){
  std::ifstream audio(path,std::ios::binary);AudioEngine analyzer;analyzer.beginReplay();analyzer.setProfile(0);std::array<float,AudioEngine::FftSize>window{};
  if(!audio.read(reinterpret_cast<char*>(window.data()),sizeof(window)))return 27;
  const int worlds[]={6,8,10,12,19,18,22,23};const int world=worlds[trackIndex++%8];PhraseConductor conductor;conductor.begin(world,20,setBeat);
  // A separate track is a fresh musical detector session, with shared program memory.
  tempoPhase_=phaseCorrection_=sectionShortEnergy_=sectionPhraseEnergy_=sectionLongEnergy_=sectionCandidateSeconds_=0;
  sectionStartedAt_=sectionBeatOrigin_=sectionLockUntil_=dropPulse_=manualSectionOverrideUntil_=endingConfidence_=lowEnergySeconds_=sectionEvidence_=buildTension_=dropReadiness_=musicalNovelty_=songArc_=0;
  lastDropAt_=-100;musicStartedAt_=-1;lastReliableTempoAt_=tempoRecoveryConfidence_=0;visualBpm_=stableBpm_=128;
  heardActiveMusic_=beatLatched_=false;sectionRevision_=dropCount_=0;songSection_=sectionCandidate_=0;lastDirectorPhrase_=-1;effectiveTempoAnchor_=128;settings_.tempoAnchor=0;automatic_=false;
  std::ofstream trace(output/"music"/(path.stem().string()+".csv"));trace<<"seconds,beat,bpm,tempo_confidence,energy,section,section_confidence,from,to,action,blend,kick,snare\n";
  int hops=0,changes=0,previousMode=int(conductor.sample(0).to);float lastChange=-1000,minSpacing=1000;double confidence=0,bpm=0;bool valid=true;
  do{
   const auto af=analyzer.replayHop(window,48000);const float dt=float(AudioEngine::HopSize)/48000,seconds=hops*dt;
   advanceMusicalClock(af,dt,seconds);updateSongSection(af,dt,seconds);
   PhraseEvidence e;e.section=songSection_;e.revision=sectionRevision_;e.sectionAge=tempoPhase_-sectionBeatOrigin_;e.energy=sectionLongEnergy_;e.confidence=sectionEvidence_;e.trend=sectionPhraseEnergy_-sectionLongEnergy_;
   conductor.update(setBeat+tempoPhase_,e,memory);auto phrase=conductor.sample(setBeat+tempoPhase_);valid&=ValidPhraseFrame(phrase)&&std::isfinite(tempoPhase_)&&std::isfinite(visualBpm_);
   if(int(phrase.to)!=previousMode){if(changes)minSpacing=std::min(minSpacing,tempoPhase_-lastChange);lastChange=tempoPhase_;previousMode=int(phrase.to);++changes;}
   confidence+=af.tempoConfidence;bpm+=visualBpm_;
   if(hops%12==0)trace<<seconds<<','<<tempoPhase_<<','<<visualBpm_<<','<<af.tempoConfidence<<','<<sectionLongEnergy_<<','<<songSection_<<','<<sectionEvidence_<<','<<phrase.from<<','<<phrase.to<<','<<phrase.action<<','<<phrase.blend<<','<<af.kick<<','<<af.snare<<'\n';
   if(hops%1500==0){Constants c{};c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);c.scene=c.previousScene=float(world);c.transition=1;c.time=seconds;c.tempoPhase=tempoPhase_;c.patternSeed=c.previousPatternSeed=20;c.phrase=phrase;c.palette=2;c.logoScale=1;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.bass=af.bass;c.snare=af.snare;c.percussion=af.percussion;
    drawScene(c);if(!saveBackBufferPng(output/"music"/(path.stem().string()+"-"+std::to_string(hops)+".png")))return 27;}
   ++hops;std::move(window.begin()+AudioEngine::HopSize,window.end(),window.begin());
  }while(audio.read(reinterpret_cast<char*>(window.data()+AudioEngine::HopSize),AudioEngine::HopSize*sizeof(float)));
  valid&=hops>=100&&minSpacing>=7.9f;
  summary<<path.stem().string()<<','<<hops*float(AudioEngine::HopSize)/48000<<','<<hops<<','<<sectionRevision_<<','<<changes<<','<<confidence/hops<<','<<bpm/hops<<','<<minSpacing<<','<<valid<<'\n';summary.flush();passed&=valid;setBeat+=std::ceil(tempoPhase_/8)*8+8;
 }
 return passed?0:28;
}

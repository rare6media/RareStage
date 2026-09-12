// Production-pipeline captures: the fixture substitutes timing/audio, never rendering.
int runGraphicValidation(){
 const std::filesystem::path output="RareStage-graphic-validation";std::filesystem::create_directories(output/"stills");
 settings_.quality=2;settings_.spout=0;settings_.vsync=0;settings_.djName=L"RARESIX";if(!rebuildIdentityTexture())return 31;
 resize(960,540);
 auto fixture=[&](int world,float beat,int mode){Constants c{};c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);c.scene=c.previousScene=float(world);c.transition=1;c.time=beat*.5f;c.tempoPhase=beat;c.palette=2;c.logoScale=1;c.masterBrightness=.92f;c.masteringProfile=1;c.cueDensity=1;c.phrase=c.previousPhrase={float(mode),float(mode),1,rarestage::show::PhraseFamily(world)>=0?1.f:0.f,1,1,0,0};return c;};
 auto save=[&](const Constants&c,const std::string&name){drawScene(c);return saveBackBufferPng(output/"stills"/(name+".png"));};
 if(std::wcsstr(GetCommandLineW(),L"--preview-only")){
  std::filesystem::create_directories(output/"preview");
  const int worlds[]{3,35,36,37,13,35,37,36,35,3};
  const int programs[]{0,0,0,0,0,1,5,4,8,0};
  for(int frame=0;frame<7200;++frame){int chapter=frame/720,local=frame%720;float beat=frame/30.f;auto c=fixture(worlds[chapter],beat,programs[chapter]);
   c.palette=2+frame/1800.f;c.snare=std::exp(-std::fmod(beat+1,2.f)*12)*.5f;c.percussion=std::exp(-std::fmod(beat*2,1.f)*12)*.4f;c.bass=.25f;
   if(chapter&&local<240){c.previousScene=float(worlds[chapter-1]);c.transition=local/240.f;c.previousPhrase=fixture(worlds[chapter-1],beat,(programs[chapter-1]+3)%rarestage::show::ProgramCount(worlds[chapter-1])).phrase;}
   if(local>=360){c.phrase.to=float((programs[chapter]+3)%rarestage::show::ProgramCount(worlds[chapter]));c.phrase.blend=liquidProgress((local-360)/180.f);}
   if((chapter==5||chapter==7)&&local>=270){c.identityWeight=liquidProgress((local-270)/120.f)*(1-liquidProgress((local-540)/150.f));c.identityKind=chapter==5?0.f:1.f;}
   drawScene(c);wchar_t name[40]{};swprintf_s(name,L"frame-%04d.png",frame);if(!saveBackBufferPng(output/"preview"/name))return 31;
  }return 0;
 }
 std::ofstream manifest(output/"visual-checks.csv");manifest<<"file,kind,from,to,program,beat,weight\n";
 for(int world=35;world<38;++world)for(int mode=0;mode<10;++mode)for(int sample=0;sample<4;++sample){float beat=sample*4+1.5f;auto c=fixture(world,beat,mode);c.palette=float(mode%8);
  auto name="scene-"+std::to_string(world)+"-mode-"+std::to_string(mode)+"-sample-"+std::to_string(sample);if(!save(c,name))return 31;manifest<<name<<",pattern,"<<world<<','<<world<<','<<mode<<','<<beat<<",0\n";
 }
 for(int from:{3,13,35,36,37})for(int to:{3,13,35,36,37}){if(from==to||(from<35&&to<35))continue;
  for(int frame=0;frame<=8;++frame){auto c=fixture(to,16+frame*.5f,1);c.previousScene=float(from);c.previousPhrase=fixture(from,c.tempoPhase,0).phrase;c.transition=frame/8.f;
   auto name="join-"+std::to_string(from)+"-"+std::to_string(to)+"-"+std::to_string(frame);if(!save(c,name))return 31;manifest<<name<<",handoff,"<<from<<','<<to<<",1,"<<c.tempoPhase<<','<<c.transition<<'\n';
  }
 }
 for(int world=35;world<38;++world)for(int kind=0;kind<2;++kind)for(int style=0;style<3;++style)for(int frame=0;frame<=8;++frame){auto c=fixture(world,5.5f+frame*.25f,style);c.identityKind=float(kind);c.identityWeight=frame/8.f;
  auto name="identity-"+std::to_string(world)+"-"+std::to_string(kind)+"-"+std::to_string(style)+"-"+std::to_string(frame);if(!save(c,name))return 31;manifest<<name<<",identity,"<<world<<','<<world<<','<<style<<','<<c.tempoPhase<<','<<c.identityWeight<<'\n';
 }
 return 0;
}

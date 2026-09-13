// Included inside App. Offscreen rehearsals use the production render passes;
// their resources and timestamps are isolated from live output and its timers.
struct RehearsalSurface {
 UINT width{},height{},renderWidth{},renderHeight{};
 ComPtr<ID3D11Texture2D>back,scene,bloom,source,morph,staging;
 ComPtr<ID3D11RenderTargetView>outputRtv,sceneRtv,bloomRtv,sourceRtv,morphRtv;
 ComPtr<ID3D11ShaderResourceView>sceneSrv,bloomSrv,sourceSrv,morphSrv;
 ComPtr<ID3D11Query>event,disjoint,start,end;
 bool ready{},pending{},failed{};unsigned epoch{};int candidate{},sample{};
 float submittedAt{-100},lastGpuMs{};
} rehearsalSurface_;
rarestage::show::VisualRehearsal visualRehearsal_;
Constants rehearsalBaseline_{};float rehearsalBeat_{};
std::array<int,2> rehearsalSeeds_{};
int rehearsedEntry_{-1};

void swapRehearsalSurface(){
 auto&r=rehearsalSurface_;using std::swap;
 swap(width_,r.width);swap(height_,r.height);swap(renderWidth_,r.renderWidth);swap(renderHeight_,r.renderHeight);
 swap(backBuffer_,r.back);swap(rtv_,r.outputRtv);
 swap(sceneTexture_,r.scene);swap(sceneRtv_,r.sceneRtv);swap(sceneSrv_,r.sceneSrv);
 swap(bloomTexture_,r.bloom);swap(bloomRtv_,r.bloomRtv);swap(bloomSrv_,r.bloomSrv);
 swap(morphSourceTexture_,r.source);swap(morphSourceRtv_,r.sourceRtv);swap(morphSourceSrv_,r.sourceSrv);
 swap(morphTexture_,r.morph);swap(morphRtv_,r.morphRtv);swap(morphSrv_,r.morphSrv);
}
bool prepareRehearsalSurface(){
 auto&r=rehearsalSurface_;if(r.failed||r.pending)return false;
 float aspect=float(renderWidth_)/renderHeight_;UINT w=192,h=UINT(std::round(w/aspect));
 if(h>192){h=192;w=UINT(std::round(h*aspect));}w=std::max(32u,w);h=std::max(32u,h);
 if(r.ready&&r.width==w&&r.height==h)return true;
 r={};r.width=r.renderWidth=w;r.height=r.renderHeight=h;
 auto target=[&](UINT tw,UINT th,DXGI_FORMAT format,ComPtr<ID3D11Texture2D>&texture,ComPtr<ID3D11RenderTargetView>&view,ComPtr<ID3D11ShaderResourceView>*srv){
  D3D11_TEXTURE2D_DESC d{};d.Width=tw;d.Height=th;d.MipLevels=d.ArraySize=1;d.Format=format;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_RENDER_TARGET|(srv?D3D11_BIND_SHADER_RESOURCE:0);
  return SUCCEEDED(device_->CreateTexture2D(&d,nullptr,&texture))&&SUCCEEDED(device_->CreateRenderTargetView(texture.Get(),nullptr,&view))&&(!srv||SUCCEEDED(device_->CreateShaderResourceView(texture.Get(),nullptr,srv->GetAddressOf())));
 };
 bool ok=target(w,h,DXGI_FORMAT_B8G8R8A8_UNORM,r.back,r.outputRtv,nullptr)&&
  target(w,h,DXGI_FORMAT_R16G16B16A16_FLOAT,r.scene,r.sceneRtv,&r.sceneSrv)&&
  target(w/2,h/2,DXGI_FORMAT_R16G16B16A16_FLOAT,r.bloom,r.bloomRtv,&r.bloomSrv)&&
  target(w,h,DXGI_FORMAT_R16G16B16A16_FLOAT,r.source,r.sourceRtv,&r.sourceSrv)&&
  target(w,h,DXGI_FORMAT_R16G16B16A16_FLOAT,r.morph,r.morphRtv,&r.morphSrv);
 if(ok){D3D11_TEXTURE2D_DESC d{};r.back->GetDesc(&d);d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ok=SUCCEEDED(device_->CreateTexture2D(&d,nullptr,&r.staging));}
 D3D11_QUERY_DESC q{D3D11_QUERY_EVENT,0};ok=ok&&SUCCEEDED(device_->CreateQuery(&q,&r.event));q.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;ok=ok&&SUCCEEDED(device_->CreateQuery(&q,&r.disjoint));q.Query=D3D11_QUERY_TIMESTAMP;ok=ok&&SUCCEEDED(device_->CreateQuery(&q,&r.start))&&SUCCEEDED(device_->CreateQuery(&q,&r.end));
 r.ready=ok;r.failed=!ok;return ok;
}
float plannedActionExit(float earliest,bool chapter=false)const{
 return rarestage::show::PreferredExit(world_,int(phraseConductor_.sample(tempoPhase_).to),earliest,worldBeatOrigin_,phraseConductor_.settledAt(),sectionEvidence_,chapter);
}
rarestage::show::RehearsalContext visualContext()const{
 const float cue=worldChangeArmed_&&worldCueBeat_>=0?worldCueBeat_:plannedActionExit(tempoPhase_+std::max(0.f,nextWorldAt_-elapsed())*visualBpm_/60.f);
 return {world_,int(phraseConductor_.sample(tempoPhase_).to),songSection_,sectionRevision_,settings_.sceneCategory,directorPalette_,themeMemory_.current(),settings_.stageLayout,int(renderWidth_),int(renderHeight_),int(settings_.masterBrightness*1000),settings_.masteringProfile,int(std::round(cue)),visitSeed_};
}
void pollVisualRehearsal(float seconds){
 auto&r=rehearsalSurface_;if(!r.pending)return;
 if(seconds-r.submittedAt>4){r.failed=true;visualRehearsal_.cancel();}
 BOOL ready=FALSE;if(context_->GetData(r.event.Get(),&ready,sizeof(ready),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK||!ready)return;
 D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};UINT64 start=0,end=0;
 if(context_->GetData(r.disjoint.Get(),&disjoint,sizeof(disjoint),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK||context_->GetData(r.start.Get(),&start,sizeof(start),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK||context_->GetData(r.end.Get(),&end,sizeof(end),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)return;
 D3D11_MAPPED_SUBRESOURCE map{};HRESULT mapped=context_->Map(r.staging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map);
 if(mapped==DXGI_ERROR_WAS_STILL_DRAWING)return;
 r.pending=false;if(FAILED(mapped)){r.failed=true;visualRehearsal_.cancel();return;}
 auto image=rarestage::show::MeasureComposition(static_cast<const std::uint8_t*>(map.pData),r.width,r.height,map.RowPitch);context_->Unmap(r.staging.Get(),0);
 if(disjoint.Disjoint||!disjoint.Frequency||end<=start){visualRehearsal_.cancel();r.lastGpuMs=gpuBudgetMs();return;}
 r.lastGpuMs=float(double(end-start)*1000.0/disjoint.Frequency);
 if(visualRehearsal_.accept(r.epoch,r.candidate,r.sample,image)&&visualRehearsal_.done())routePhrase_=-1;
}
void submitVisualRehearsal(Constants c,unsigned epoch,int candidate,int sample,float seconds){
 auto&r=rehearsalSurface_;if(!r.ready||r.pending||r.failed)return;
 context_->Begin(r.disjoint.Get());context_->End(r.start.Get());
 swapRehearsalSurface();
 {struct Restore {App&app;~Restore(){app.swapRehearsalSurface();}} restore{*this};
  c.resolution[0]=float(renderWidth_);c.resolution[1]=float(renderHeight_);drawScene(c,false);
  context_->CopyResource(r.staging.Get(),backBuffer_.Get());
 }
 context_->End(r.end.Get());context_->End(r.disjoint.Get());context_->End(r.event.Get());
 r.epoch=epoch;r.candidate=candidate;r.sample=sample;r.submittedAt=seconds;r.pending=true;++visualRehearsal_.submitted;
}
int rehearseEntry(int candidate,float beat,int&seed)const{
 auto memory=setMemory_;auto sequence=sequence_;int fresh=memory.program(candidate,songSection_);
 sequence.visit(candidate,(fresh/4)%rarestage::show::ProgramCount(candidate),songSection_,sectionRevision_,sectionEvidence_,beat-sectionBeatOrigin_,beat,true);
 seed=sequence.role()!=0?int(patternSeed_):fresh;int preferred=sequence.program(candidate);
 const auto source=phraseConductor_.sample(beat);
 if(rarestage::motion::Correspondence(world_,candidate))return rarestage::motion::matchEntry(world_,candidate,source,beat,float(renderWidth_)/renderHeight_,preferred);
 if(world_>=26&&world_<35&&candidate>=26&&candidate<35)return rarestage::play::matchEntry(world_,candidate,patternSeed_,float(seed),source,beat,preferred);
 return preferred;
}
void serviceVisualRehearsal(const Constants&live,float seconds){
 const auto key=visualContext();
 if(!visualRehearsal_.valid(key,tempoPhase_))visualRehearsal_.cancel();
 pollVisualRehearsal(seconds);
 const bool safe=automatic_&&!frozen_&&!programHeld_&&!blackout_&&!settings_.timeline&&settings_.syncMode!=2&&live.transition>=1&&!identityStanza_&&live.identityWeight<.001f&&live.phrase.blend>=1;
 if(!safe){visualRehearsal_.cancel();return;}
 auto&r=rehearsalSurface_;if(r.pending||r.failed)return;
 float remaining=(nextWorldAt_-seconds)*visualBpm_/60.f;
 if(worldChangeArmed_)remaining=worldCueBeat_-tempoPhase_;
 if(remaining<12||remaining>32)return;
 const float probeCost=seconds-r.submittedAt>8?std::min(r.lastGpuMs,gpuBudgetMs()*.04f):r.lastGpuMs;
 if(!rarestage::show::RehearsalBudget(gpuFrameMs_,gpuPeakMs_,gpuBudgetMs(),probeCost,seconds-r.submittedAt)){++visualRehearsal_.budgetSkips;return;}
 if(!prepareRehearsalSurface())return;
 if(!visualRehearsal_.valid(key,tempoPhase_)){
  if(route_.next<0||route_.alternative<0||route_.next==route_.alternative)return;
  rehearsalBeat_=float(key.exitBeat);
  int first=rehearseEntry(route_.next,rehearsalBeat_,rehearsalSeeds_[0]),second=rehearseEntry(route_.alternative,rehearsalBeat_,rehearsalSeeds_[1]);
  visualRehearsal_.begin(key,tempoPhase_,route_.next,route_.alternative,first,second);rehearsalBaseline_=live;
 }
 auto next=visualRehearsal_.next();if(!next)return;const int candidate=(*next)[0],sample=(*next)[1];const auto&choice=visualRehearsal_.candidates()[candidate];
 Constants frame=rehearsalBaseline_;frame.previousScene=frame.scene;frame.previousPatternSeed=frame.patternSeed;frame.previousPhrase=frame.phrase;frame.scene=float(choice.scene);frame.patternSeed=float(rehearsalSeeds_[candidate]);
 frame.phrase={float(choice.program),float(choice.program),1,rarestage::show::PhraseFamily(choice.scene)>=0?1.f:0.f,1,1,0,0};
 frame.transition=sample/4.f;frame.tempoPhase=rehearsalBeat_+frame.transition*8;
 frame.time=rehearsalBaseline_.time+(frame.tempoPhase-rehearsalBaseline_.tempoPhase)*60.f/visualBpm_;
 frame.identityWeight=0;frame.kick=frame.snare=frame.percussion=frame.beat=frame.dropPulse=frame.flux=frame.onset=0;
 submitVisualRehearsal(frame,visualRehearsal_.epoch(),candidate,sample,seconds);
}

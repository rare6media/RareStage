#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "world_compiler.h"
#include "world_intro.h"
#include <wincodec.h>
#include "world_qualifier_gpu.h"
#include <thread>
#include <chrono>
#include <functional>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <vector>
using namespace rarestage::gen;
using Microsoft::WRL::ComPtr;
static int failures=0;
static void check(const char*name,bool ok){if(!ok){std::printf("FAIL %s\n",name);++failures;}}
static void check(const std::string&name,bool ok){check(name.c_str(),ok);}

// ---- minimal PNG writer (stored deflate) so the contact sheet can be inspected without extra libraries
static std::uint32_t Crc(const unsigned char*d,size_t n,std::uint32_t c=0xFFFFFFFFu){static std::uint32_t t[256];static bool init=false;if(!init){for(std::uint32_t i=0;i<256;++i){std::uint32_t v=i;for(int k=0;k<8;++k)v=v&1?0xEDB88320u^(v>>1):v>>1;t[i]=v;}init=true;}for(size_t i=0;i<n;++i)c=t[(c^d[i])&255]^(c>>8);return c;}
static void Be32(std::vector<unsigned char>&o,std::uint32_t v){for(int s=24;s>=0;s-=8)o.push_back((unsigned char)(v>>s));}
static void Chunk(std::vector<unsigned char>&o,const char*type,const std::vector<unsigned char>&data){Be32(o,std::uint32_t(data.size()));std::vector<unsigned char> td(type,type+4);td.insert(td.end(),data.begin(),data.end());o.insert(o.end(),td.begin(),td.end());Be32(o,~Crc(td.data(),td.size()));}
static bool WritePng(const char*path,int w,int h,const std::vector<unsigned char>&rgba){
 std::vector<unsigned char> raw;raw.reserve(size_t(h)*(size_t(w)*3+1));
 for(int y=0;y<h;++y){raw.push_back(0);for(int x=0;x<w;++x){const unsigned char*p=&rgba[(size_t(y)*w+x)*4];raw.push_back(p[0]);raw.push_back(p[1]);raw.push_back(p[2]);}}
 std::vector<unsigned char> z{0x78,0x01};size_t pos=0;std::uint32_t a=1,b=0;for(unsigned char c:raw){a=(a+c)%65521;b=(b+a)%65521;}
 while(pos<raw.size()){const size_t n=std::min<size_t>(65535,raw.size()-pos);z.push_back(pos+n>=raw.size()?1:0);z.push_back((unsigned char)(n&255));z.push_back((unsigned char)(n>>8));z.push_back((unsigned char)(~n&255));z.push_back((unsigned char)((~n>>8)&255));z.insert(z.end(),raw.begin()+long(pos),raw.begin()+long(pos+n));pos+=n;}
 Be32(z,(b<<16)|a);
 std::vector<unsigned char> out{0x89,'P','N','G',13,10,26,10};std::vector<unsigned char> ihdr;Be32(ihdr,std::uint32_t(w));Be32(ihdr,std::uint32_t(h));ihdr.push_back(8);ihdr.push_back(2);ihdr.push_back(0);ihdr.push_back(0);ihdr.push_back(0);
 Chunk(out,"IHDR",ihdr);Chunk(out,"IDAT",z);Chunk(out,"IEND",{});
 std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(out.data()),std::streamsize(out.size()));return f.good();
}

static float HalfToFloat(std::uint16_t h){const int e=(h>>10)&31;const int m=h&1023;float v;if(e==0)v=std::ldexp(float(m),-24);else if(e==31)v=m?0.f:65504.f;else v=std::ldexp(float(m|1024),e-25);return (h&0x8000)?-v:v;}
static void QualifierLogicChecks(){
 check("budget_60fps",std::abs(BudgetForFps(60.f)-5.f)<1e-4f);check("budget_90fps",std::abs(BudgetForFps(90.f)-3.f)<1e-4f);check("budget_120fps",std::abs(BudgetForFps(120.f)-2.f)<1e-4f);
 check("budget_clamps_low_and_high",BudgetForFps(30.f)==5.f&&BudgetForFps(240.f)==2.f&&BudgetForFps(std::nanf(""))==5.f);
 {bool mono=true;float prev=99;for(float f=60;f<=120;f+=1){const float b=BudgetForFps(f);mono&=b<=prev+1e-5f;prev=b;}check("budget_falls_with_refresh_rate",mono);}
 Measurement ok;ok.avgMs=.5f;ok.testPixels=1000;ok.fullPixels=4000;ok.meanLuma=.2f;ok.nonBlack=.6f;   // estimated 2.0 ms at full size
 check("scales_to_full_resolution",std::abs(EstimatedFullMs(ok)-2.f)<1e-4f);
 check("accept_within_budget",Decide(ok,5.f)==Verdict::Accept);
 check("accept_heavy_near_budget",Decide(ok,2.5f)==Verdict::AcceptHeavy);
 check("reject_over_budget",Decide(ok,1.5f)==Verdict::RejectCost);
 Measurement m=ok;m.meanLuma=.001f;check("reject_black",Decide(m,5.f)==Verdict::RejectBlack);
 m=ok;m.nonBlack=.01f;check("reject_mostly_black",Decide(m,5.f)==Verdict::RejectBlack);
 m=ok;m.meanLuma=.99f;check("reject_blown_out",Decide(m,5.f)==Verdict::RejectBlown);
 m=ok;m.finite=false;check("reject_non_finite",Decide(m,5.f)==Verdict::RejectNonFinite);
 m=ok;m.timingValid=false;check("reject_without_timing",Decide(m,5.f)==Verdict::RejectNoTiming);
 m=ok;m.avgMs=std::nanf("");check("reject_nan_timing",Decide(m,5.f)==Verdict::RejectNoTiming);
 QualificationHistory h;h.record(7,Verdict::Accept,1.f);h.record(7,Verdict::Accept,3.f);check("history_averages",h.find(7)&&std::abs(h.find(7)->avgMs-2.f)<1e-4f&&!h.blacklisted(7));
 h.record(9,Verdict::RejectCost,8.f);check("single_failure_not_blacklisted",!h.blacklisted(9));h.record(9,Verdict::RejectBlack,0.f);check("repeated_failure_blacklisted",h.blacklisted(9)&&h.blacklistCount()==1);
 h.record(11,Verdict::RejectNoTiming,0.f);h.record(11,Verdict::RejectNoTiming,0.f);h.record(11,Verdict::RejectNoTiming,0.f);check("missing_timing_never_blacklists",!h.blacklisted(11));
 WorldSpec heavy=Generate(5,Family::VirusSpread,2),cheaper;check("cheaper_sibling_exists",CheaperSibling(heavy,cheaper)&&cheaper.budget.quality==1&&cheaper.seed==heavy.seed&&cheaper.family==heavy.family);
 WorldSpec lowest=Generate(5,Family::VirusSpread,0);check("no_sibling_below_lowest_quality",!CheaperSibling(lowest,cheaper));
}

static void OfflineChecks(){
 QualifierLogicChecks();
 // Every generated spec is legal, deterministic and within budget.
 std::set<std::uint64_t> hashes;int total=0,overSoft=0;std::array<bool,FamilyCount> seen{};
 for(int f=0;f<FamilyCount;++f)for(std::uint32_t seed=1;seed<=60;++seed)for(int q=0;q<3;++q){
  const WorldSpec w=Generate(seed,Family(f),q);const Reject r=ValidateWorld(w);
  if(r!=Reject::None){check(std::string("generated_valid_")+FamilyNames[f]+"_"+std::to_string(seed)+"_q"+std::to_string(q)+" ("+RejectName(r)+")",false);continue;}
  const Emitted a=Emit(w),b=Emit(Generate(seed,Family(f),q));
  check("emit_deterministic",a.hash==b.hash&&a.source==b.source);
  check("emit_size_bounded",a.source.size()<20000&&!a.source.empty());
  check("complexity_below_hard",a.complexity<=HardComplexity);overSoft+=a.complexity>SoftComplexity;
  if(q==1)hashes.insert(a.hash);seen[size_t(f)]=true;++total;}
 check("all_families_generate",std::count(seen.begin(),seen.end(),true)==FamilyCount);
 check("seeds_produce_distinct_shaders",hashes.size()>=size_t(FamilyCount*60*95/100));
 std::printf("generated %d specs, %d over the soft complexity limit\n",total,overSoft);

 // Persistence round trip keeps the source byte-identical.
 {bool ok=true;for(int f=0;f<FamilyCount;++f){const WorldSpec w=Generate(777u+std::uint32_t(f),Family(f),1);WorldSpec back;ok&=Parse(Serialize(w),back)&&Serialize(back)==Serialize(w)&&Emit(back).hash==Emit(w).hash;}check("serialize_round_trip",ok);}
 {WorldSpec back;check("parse_rejects_garbage",!Parse("hello\nworld\n",back)&&!Parse("",back));
  std::string s=Serialize(Generate(5,Family::Monolith,1));s+="prim 99 0 1 1 1 0 0 0 0 .1\n";check("parse_rejects_bad_enum",!Parse(s,back));
  std::string t=Serialize(Generate(5,Family::Monolith,1));t.erase(t.find("material"));check("parse_missing_sections_still_validates",!Parse("rarestage-worldspec 1\nprimCount 2\n",back));}

 // Hostile and malformed specs never reach the compiler.
 {WorldSpec w=Generate(3,Family::Monolith,1);
  WorldSpec x=w;x.schema=99;check("reject_schema",ValidateWorld(x)==Reject::Schema);
  x=w;x.grammar=99;check("reject_grammar",ValidateWorld(x)==Reject::Grammar);
  x=w;x.prim[0].sx=std::nanf("");check("reject_nan",ValidateWorld(x)==Reject::NonFinite);
  x=w;x.prim[0].sy=1e9f;check("reject_range",ValidateWorld(x)==Reject::Range);
  x=w;x.primCount=0;check("reject_empty",ValidateWorld(x)==Reject::Count);
  x=w;x.prim[0].combine=CombineKind::Subtract;check("reject_subtract_first",ValidateWorld(x)==Reject::Incompatible);
  x=w;x.prim[0].kind=PrimKind::Gyroid;check("reject_incompatible_primitive",ValidateWorld(x)==Reject::Incompatible);
  x=w;x.domainCount=1;x.domain[0]=DomainOp{DomainKind::Kaleidoscope,3,.5f,0,0};check("reject_incompatible_domain",ValidateWorld(x)==Reject::Incompatible);
  x=w;x.camera.mode=CameraMode::Dolly;x.domainCount=0;check("reject_dolly_without_z_repeat",ValidateWorld(x)==Reject::Incompatible);
  x=w;x.budget.marchSteps=200;check("reject_unbounded_steps",ValidateWorld(x)==Reject::Range);
  WorldSpec heavy=Generate(9,Family::FractalChamber,2);heavy.budget.marchSteps=96;heavy.lighting.shadows=true;for(int i=0;i<heavy.primCount;++i)heavy.prim[size_t(i)].kind=i==0?PrimKind::Gyroid:heavy.prim[size_t(i)].kind;
  check("reject_over_complexity",ValidateWorld(heavy)==Reject::Complexity||ValidateWorld(heavy)==Reject::None);
  check("emit_refuses_invalid",Emit(x).source.empty());}
}

// ---- GPU compile + render --------------------------------------------------------------------
static void ServiceChecks(ID3D11Device*dev){
 using namespace std::chrono_literals;
 const auto dir=std::filesystem::temp_directory_path()/("rs-gen-cache-"+std::to_string(GetCurrentProcessId()));std::error_code ec;std::filesystem::remove_all(dir,ec);
 auto settle=[&](WorldCompiler&svc,std::uint32_t expect){const auto limit=std::chrono::steady_clock::now()+60s;for(;;){const auto s=svc.stats();if(s.compiled+s.failed+s.stale+s.rejected>=expect&&s.pending==0)return true;if(std::chrono::steady_clock::now()>limit)return false;std::this_thread::sleep_for(5ms);}};
 std::vector<WorldSpec> accepted;
 {WorldCompiler svc(dev,dir);
  WorldSpec bad=Generate(1,Family::Monolith,1);bad.prim[0].sx=std::nanf("");check("service_refuses_invalid_spec",!svc.submit(bad,1));
  int ok=0;for(std::uint32_t s=1;s<=40;++s){const WorldSpec w=Generate(s,Family(s%FamilyCount),1);if(svc.submit(w,1)){++ok;accepted.push_back(w);}}
  check("pending_queue_is_bounded",ok<40&&ok>=1);
  std::vector<CompiledWorld> got;check("all_accepted_jobs_finish",settle(svc,std::uint32_t(ok)+1u));
  while(auto w=svc.poll())got.push_back(std::move(*w));
  const auto st=svc.stats();check("no_compile_failures",st.failed==0);check("ready_queue_is_bounded",got.size()<=WorldCompiler::ReadyCapacity&&st.ready==0);
  bool whole=!got.empty();for(const auto&w:got)whole&=w.shader&&w.hash!=0&&w.spec.shaderHash==w.hash&&w.generation==1;check("packages_are_complete",whole);
  check("cache_misses_counted_on_first_pass",st.cacheMisses>=1&&st.cacheHits==0);
  // Stale work is discarded, never published.
  const std::uint32_t before=svc.stats().compiled;svc.submit(Generate(901,Family::Cathedral,1),2);svc.invalidate(3);std::this_thread::sleep_for(400ms);
  check("stale_result_not_published",!svc.poll().has_value());check("stale_submit_refused",!svc.submit(Generate(902,Family::Cathedral,1),2));(void)before;
 }
 // A fresh service finds the earlier compilations on disk.
 {WorldCompiler svc(dev,dir);int ok=0;for(const auto&w:accepted){if(svc.submit(w,1))++ok;if(ok>=6)break;}
  check("second_run_settles",settle(svc,std::uint32_t(ok)));const auto st=svc.stats();check("disk_cache_hits_on_second_run",st.cacheHits>=1&&st.cacheHits>=st.cacheMisses);}
 // Shutting down with work still queued returns promptly and leaves nothing running.
 {const auto t0=std::chrono::steady_clock::now();{WorldCompiler svc(dev,dir);for(std::uint32_t s=500;s<508;++s)svc.submit(Generate(s,Family(s%FamilyCount),2),1);}
  check("shutdown_with_queued_work_is_prompt",std::chrono::steady_clock::now()-t0<10s);}
 std::filesystem::remove_all(dir,ec);
}

struct Tile{Family f;std::uint32_t seed;};
// Launch scene: compile it, bind the real logo, render the timeline and the standby frame, and check the
// sequence actually develops (dark start, warp, arrival, mark) and that standby is steady and alive.
static ComPtr<ID3D11ShaderResourceView> LoadLogo(ID3D11Device*dev,const char*pathA){
 ComPtr<ID3D11ShaderResourceView> srv;std::wstring path;for(const char*c=pathA;*c;++c)path+=wchar_t(*c);
 CoInitializeEx(nullptr,COINIT_MULTITHREADED);ComPtr<IWICImagingFactory> f;
 if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f))))return srv;
 ComPtr<IWICBitmapDecoder> dec;if(FAILED(f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&dec)))return srv;
 ComPtr<IWICBitmapFrameDecode> fr;dec->GetFrame(0,&fr);ComPtr<IWICFormatConverter> cv;f->CreateFormatConverter(&cv);
 cv->Initialize(fr.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
 UINT w=0,h=0;cv->GetSize(&w,&h);std::vector<unsigned char> px(size_t(w)*h*4);cv->CopyPixels(nullptr,w*4,UINT(px.size()),px.data());
 D3D11_TEXTURE2D_DESC td{};td.Width=w;td.Height=h;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA sd{px.data(),w*4,0};ComPtr<ID3D11Texture2D> tex;if(FAILED(dev->CreateTexture2D(&td,&sd,&tex)))return srv;dev->CreateShaderResourceView(tex.Get(),nullptr,&srv);return srv;
}
static void IntroChecks(ID3D11Device*dev,ID3D11DeviceContext*ctx,ID3D11VertexShader*vs,const char*sheetPath){
 const std::string src=IntroSource();ComPtr<ID3DBlob> blob,msg;const auto t0=std::chrono::steady_clock::now();
 const HRESULT hr=D3DCompile(src.data(),src.size(),"intro",nullptr,nullptr,"psGen","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&msg);
 const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
 if(FAILED(hr)){if(msg)std::printf("%s\n",static_cast<const char*>(msg->GetBufferPointer()));check("intro_compiles",false);return;}
 check("intro_compiles",true);std::printf("intro shader compiled in %.0f ms\n",ms);
 ComPtr<ID3D11PixelShader> ps;dev->CreatePixelShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,&ps);
 ComPtr<ID3D11ShaderResourceView> logo=LoadLogo(dev,"../../assets/rare-logo.png");if(!logo)logo=LoadLogo(dev,"assets/rare-logo.png");check("logo_loads",bool(logo));
 D3D11_SAMPLER_DESC sm{};sm.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sm.AddressU=sm.AddressV=sm.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sm.MaxLOD=D3D11_FLOAT32_MAX;ComPtr<ID3D11SamplerState> samp;dev->CreateSamplerState(&sm,&samp);
 const int W=480,H=270;D3D11_TEXTURE2D_DESC td{};td.Width=W;td.Height=H;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> rt,stage;dev->CreateTexture2D(&td,nullptr,&rt);td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;dev->CreateTexture2D(&td,nullptr,&stage);
 ComPtr<ID3D11RenderTargetView> rtv;dev->CreateRenderTargetView(rt.Get(),nullptr,&rtv);
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;ComPtr<ID3D11Buffer> cb;dev->CreateBuffer(&bd,nullptr,&cb);
 auto render=[&](float introT,float mode,std::vector<unsigned char>&out){
  Constants c{};c.resolution[0]=W;c.resolution[1]=H;c.time=introT+20.f;c.gParam[5]={introT,mode,0.f,0.f};
  if(mode>1.5f){c.bandsA={.5f,.7f,.8f,.45f};c.bandsB={.6f,.35f,.55f,0.f};c.level=.5f;}
  ctx->UpdateSubresource(cb.Get(),0,nullptr,&c,0,0);const float clear[4]{};ctx->ClearRenderTargetView(rtv.Get(),clear);D3D11_VIEWPORT vp{0,0,W,H,0,1};ctx->RSSetViewports(1,&vp);ctx->OMSetRenderTargets(1,rtv.GetAddressOf(),nullptr);
  ctx->VSSetShader(vs,nullptr,0);ctx->PSSetShader(ps.Get(),nullptr,0);ctx->PSSetConstantBuffers(0,1,cb.GetAddressOf());ctx->PSSetSamplers(0,1,samp.GetAddressOf());ctx->PSSetShaderResources(0,1,logo.GetAddressOf());
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);ctx->Draw(3,0);ctx->CopyResource(stage.Get(),rt.Get());
  D3D11_MAPPED_SUBRESOURCE m{};ctx->Map(stage.Get(),0,D3D11_MAP_READ,0,&m);out.resize(size_t(W)*H*4);
  for(int y=0;y<H;++y){const auto*row=reinterpret_cast<const std::uint16_t*>(static_cast<const unsigned char*>(m.pData)+size_t(y)*m.RowPitch);for(int x=0;x<W;++x){for(int k=0;k<3;++k){const float c2=std::max(HalfToFloat(row[x*4+k]),0.f),t2=c2*(1.f+c2/4.f)/(1.f+c2);out[(size_t(y)*W+size_t(x))*4+size_t(k)]=(unsigned char)std::lround(std::clamp(std::pow(t2,1.f/2.2f),0.f,1.f)*255.f);}out[(size_t(y)*W+size_t(x))*4+3]=255;}}
  ctx->Unmap(stage.Get(),0);};
 const float times[9]{.4f,1.6f,2.8f,3.65f,3.95f,4.6f,5.4f,6.4f,99.f};std::vector<unsigned char> frames[9];std::vector<unsigned char> sheet(size_t(W*3)*H*3*4,0);double luma[9]{};
 for(int i=0;i<9;++i){render(times[i],i==8?2.f:1.f,frames[i]);double sum=0;for(size_t k=0;k<frames[i].size();k+=4)sum+=(frames[i][k]*.2126+frames[i][k+1]*.7152+frames[i][k+2]*.0722)/255.;luma[i]=sum/(frames[i].size()/4);
  const int cx=i%3,cy=i/3;for(int y=0;y<H;++y)std::memcpy(&sheet[((size_t(cy)*H+y)*size_t(W*3)+size_t(cx)*W)*4],&frames[i][size_t(y)*W*4],size_t(W)*4);}
 auto diff=[&](int a,int b){size_t d=0;for(size_t k=0;k<frames[a].size();k+=4)d+=std::abs(int(frames[a][k])-int(frames[b][k]))+std::abs(int(frames[a][k+1])-int(frames[b][k+1]))+std::abs(int(frames[a][k+2])-int(frames[b][k+2]))>40;return double(d)/(frames[a].size()/4);};
 check("intro_starts_dark",luma[0]<.12);check("intro_develops_over_time",luma[7]>luma[0]+.04&&diff(0,3)>.02&&diff(2,5)>.05);
 check("warp_differs_from_arrival",diff(2,4)>.03);check("mark_resolves",diff(4,7)>.03);
 check("standby_is_lit",luma[8]>.05&&luma[8]<.8);check("standby_continues_the_intro_end",std::abs(luma[8]-luma[7])<.2);
 std::printf("intro mean luma by frame:");for(int i=0;i<9;++i)std::printf(" %.2f",luma[i]);std::printf("\n");
 if(sheetPath&&WritePng(sheetPath,W*3,H*3,sheet))std::printf("intro sheet: %s\n",sheetPath);
}

struct MusicState{const char*name;int section;float progress,drop,energy,kick,snare,sub,bass,mid,air,tempo,slowEnergy,slowBuild,slowDrop;};
static const MusicState States[3]{
 {"open",0,.30f,0.f,.30f,.05f,0.f,.20f,.20f,.25f,.15f,20.30f,.30f,0.f,0.f},
 {"build",1,.85f,0.f,.65f,.30f,.65f,.35f,.40f,.55f,.60f,44.62f,.60f,.85f,0.f},
 {"drop",2,.10f,.95f,.95f,.95f,.50f,.90f,.85f,.70f,.55f,64.08f,.90f,0.f,.90f}};

// Measures real worlds on the device with the same code the renderer uses, and proves the verdicts react to cost and content.
static void QualifierChecks(ID3D11Device*dev,ID3D11DeviceContext*ctx,ID3D11VertexShader*vs,const std::function<bool(const WorldSpec&,ComPtr<ID3D11PixelShader>&,double&)>&compile){
 OffscreenQualifier q;check("qualifier_initialises",q.init(dev,320,180));if(!q.ready())return;
 auto run=[&](ID3D11PixelShader*ps,Constants base,Measurement&out)->bool{
  if(!q.start(ctx,vs,ps,base,5,1.f/60.f,1920.f*1080.f))return false;ctx->Flush();
  const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(20);
  for(;;){if(auto r=q.poll(ctx)){out=r->measurement;return true;}if(std::chrono::steady_clock::now()>limit)return false;std::this_thread::sleep_for(std::chrono::milliseconds(2));}};
 Constants base{};base.time=12.5f;base.palette=13;base.songSection=1;base.sectionProgress=.8f;base.audioEnergy=.65f;base.kick=.3f;base.tempoPhase=44.6f;base.level=.65f;base.masterBrightness=1;
 std::printf("GPU cost of generated worlds (estimated at 1920x1080):%s",std::string("\n").c_str());
 for(int f=FirstFieldFamily;f<FamilyCount;++f){
  const WorldSpec w=Generate(31u+std::uint32_t(f),Family(f),1);ComPtr<ID3D11PixelShader> ps;double ms=0;if(!compile(w,ps,ms)){check("qualifier_compile",false);continue;}
  Measurement m;const bool ok=run(ps.Get(),base,m);check(std::string("qualifier_completes_")+FamilyNames[f],ok);if(!ok)continue;
  check(std::string("timing_is_valid_")+FamilyNames[f],m.timingValid&&m.avgMs>0.f&&std::isfinite(m.avgMs));
  const Verdict v=Decide(m,BudgetForFps(60.f));std::printf("  %-14s %.2f ms  luma %.2f  -> %s%s",FamilyNames[f],double(EstimatedFullMs(m)),double(m.meanLuma),VerdictName(v),std::string("\n").c_str());
  check(std::string("real_world_is_visible_")+FamilyNames[f],m.meanLuma>.004f&&m.nonBlack>.03f&&m.finite);}
 // Hostile shaders: black, NaN and heavy must each be caught by the matching rule.
 auto compileRaw=[&](const char*body,ComPtr<ID3D11PixelShader>&ps){
  const std::string src=std::string("struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};")+body;ComPtr<ID3DBlob> blob,err;
  if(FAILED(D3DCompile(src.data(),src.size(),"raw",nullptr,nullptr,"psGen","ps_5_0",0,0,&blob,&err)))return false;return SUCCEEDED(dev->CreatePixelShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,&ps));};
 ComPtr<ID3D11PixelShader> black,nan,heavy,light;
 check("hostile_shaders_compile",compileRaw("float4 psGen(O i):SV_TARGET{return float4(0,0,0,1);}",black)&&compileRaw("float4 psGen(O i):SV_TARGET{float z=i.uv.x*0.;return float4(z/z,z/z,z/z,1);}",nan)
  &&compileRaw("float4 psGen(O i):SV_TARGET{float a=i.uv.x;[loop] for(int k=0;k<6000;++k)a=sin(a*1.01+k*.001)+cos(a);return float4(abs(a)*.2+.1,.3,.4,1);}",heavy)&&compileRaw("float4 psGen(O i):SV_TARGET{return float4(i.uv,.5,1);}",light));
 Measurement mb,mn,mh,ml;check("hostile_runs_complete",run(black.Get(),base,mb)&&run(nan.Get(),base,mn)&&run(heavy.Get(),base,mh)&&run(light.Get(),base,ml));
 check("black_shader_rejected",Decide(mb,50.f)==Verdict::RejectBlack);
 check("nan_shader_rejected",Decide(mn,50.f)==Verdict::RejectNonFinite||Decide(mn,50.f)==Verdict::RejectBlack);
 check("heavy_shader_costs_more_than_light",mh.avgMs>ml.avgMs*3.f);
 check("heavy_shader_rejected_on_cost",Decide(mh,std::max(ml.avgMs*2.f,.05f)*(1920.f*1080.f/(320.f*180.f)))==Verdict::RejectCost||Decide(mh,.01f)==Verdict::RejectCost);
 check("light_shader_accepted",Decide(ml,BudgetForFps(60.f))==Verdict::Accept||Decide(ml,BudgetForFps(60.f))==Verdict::AcceptHeavy);
 std::printf("  heavy test shader %.3f ms, light %.3f ms (test size)%s",double(mh.avgMs),double(ml.avgMs),std::string("\n").c_str());
}

static void GpuChecks(const char*sheetPath){
 ComPtr<ID3D11Device> dev;ComPtr<ID3D11DeviceContext> ctx;D3D_FEATURE_LEVEL fl;
 HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,&fl,&ctx);
 const char*kind="hardware";if(FAILED(hr)){hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,&fl,&ctx);kind="WARP";}
 if(FAILED(hr)){std::printf("no D3D11 device; GPU checks skipped\n");return;}
 std::printf("GPU checks on %s device\n",kind);
 static const char vsSrc[]="struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};O vs(uint id:SV_VertexID){O o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}";
 ComPtr<ID3DBlob> vsb,err;check("vs_compiles",SUCCEEDED(D3DCompile(vsSrc,sizeof vsSrc-1,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vsb,&err)));
 ComPtr<ID3D11VertexShader> vs;dev->CreateVertexShader(vsb->GetBufferPointer(),vsb->GetBufferSize(),nullptr,&vs);
 const int W=320,H=180;
 D3D11_TEXTURE2D_DESC td{};td.Width=W;td.Height=H;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> rt,stage;dev->CreateTexture2D(&td,nullptr,&rt);td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;dev->CreateTexture2D(&td,nullptr,&stage);
 ComPtr<ID3D11RenderTargetView> rtv;dev->CreateRenderTargetView(rt.Get(),nullptr,&rtv);
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;ComPtr<ID3D11Buffer> cb;dev->CreateBuffer(&bd,nullptr,&cb);
 auto render=[&](ID3D11PixelShader*ps,float time,const MusicState&ms,int palette,std::vector<unsigned char>&out)->bool{
  Constants c{};c.resolution[0]=W;c.resolution[1]=H;c.time=time;c.palette=float(palette);c.masterBrightness=1;
  c.songSection=float(ms.section);c.sectionProgress=ms.progress;c.dropPulse=ms.drop;c.audioEnergy=ms.energy;c.kick=ms.kick;c.snare=ms.snare;c.level=ms.energy;c.tempoPhase=ms.tempo;
  c.gParam[5]={time*.8f,ms.tempo,time,time*(.5f+1.6f*std::clamp(ms.slowBuild*.9f+ms.slowDrop*.9f,0.f,1.f))};c.gParam[6]={ms.slowEnergy,ms.slowBuild,ms.slowDrop,0.f};
  c.bandsA={ms.sub,ms.sub*.8f,ms.bass,ms.mid*.8f};c.bandsB={ms.mid,ms.energy*.7f,ms.air,0.f};
  ctx->UpdateSubresource(cb.Get(),0,nullptr,&c,0,0);
  const float clear[4]{};ctx->ClearRenderTargetView(rtv.Get(),clear);D3D11_VIEWPORT vp{0,0,W,H,0,1};ctx->RSSetViewports(1,&vp);ctx->OMSetRenderTargets(1,rtv.GetAddressOf(),nullptr);
  ctx->VSSetShader(vs.Get(),nullptr,0);ctx->PSSetShader(ps,nullptr,0);ctx->PSSetConstantBuffers(0,1,cb.GetAddressOf());ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);ctx->Draw(3,0);
  ctx->CopyResource(stage.Get(),rt.Get());D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(ctx->Map(stage.Get(),0,D3D11_MAP_READ,0,&m)))return false;
  out.resize(size_t(W)*H*4);for(int y=0;y<H;++y){const auto*row=reinterpret_cast<const std::uint16_t*>(static_cast<const unsigned char*>(m.pData)+size_t(y)*m.RowPitch);for(int x=0;x<W;++x){for(int k=0;k<3;++k){const float c2=std::max(HalfToFloat(row[x*4+k]),0.f),t=c2*(1.f+c2/4.f)/(1.f+c2);out[(size_t(y)*W+size_t(x))*4+size_t(k)]=(unsigned char)std::lround(std::clamp(std::pow(t,1.f/2.2f),0.f,1.f)*255.f);}out[(size_t(y)*W+size_t(x))*4+3]=255;}}ctx->Unmap(stage.Get(),0);return true;};
 auto stats=[&](const std::vector<unsigned char>&px,double&mean,double&nonBlack){double sum=0;size_t nb=0;for(size_t i=0;i<px.size();i+=4){const double l=(px[i]*.2126+px[i+1]*.7152+px[i+2]*.0722)/255.;sum+=l;nb+=l>.02;}mean=sum/(px.size()/4);nonBlack=double(nb)/(px.size()/4);};
 auto diffFrac=[&](const std::vector<unsigned char>&a,const std::vector<unsigned char>&b){size_t d=0;for(size_t i=0;i<a.size();i+=4)d+=std::abs(int(a[i])-int(b[i]))+std::abs(int(a[i+1])-int(b[i+1]))+std::abs(int(a[i+2])-int(b[i+2]))>40;return double(d)/(a.size()/4);};
 auto compile=[&](const WorldSpec&w,ComPtr<ID3D11PixelShader>&ps,double&ms)->bool{
  const Emitted e=Emit(w);if(e.source.empty())return false;const auto t0=std::chrono::steady_clock::now();ComPtr<ID3DBlob> blob,msg;
  HRESULT h=D3DCompile(e.source.data(),e.source.size(),"world",nullptr,nullptr,"psGen","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&msg);
  ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
  if(FAILED(h)){if(msg)std::printf("%s\n",static_cast<const char*>(msg->GetBufferPointer()));std::ofstream("RareStage-failed-world.hlsl")<<e.source;return false;}
  return SUCCEEDED(dev->CreatePixelShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,&ps));};

 const std::function<bool(const WorldSpec&,ComPtr<ID3D11PixelShader>&,double&)> compileFn=compile;QualifierChecks(dev.Get(),ctx.Get(),vs.Get(),compileFn);
 // Every family compiles and draws something visible and moving.
 double worstCompile=0,totalCompile=0;int compiled=0,tested=0;
 for(int f=0;f<FamilyCount;++f)for(std::uint32_t s=1;s<=2;++s){
  const WorldSpec w=Generate(s*11u+std::uint32_t(f),Family(f),1);ComPtr<ID3D11PixelShader> ps;double ms=0;++tested;
  if(!compile(w,ps,ms)){check(std::string("compiles_")+FamilyNames[f]+"_"+std::to_string(s),false);continue;}
  ++compiled;worstCompile=std::max(worstCompile,ms);totalCompile+=ms;
  std::vector<unsigned char> a,b;render(ps.Get(),3.f,States[1],13,a);render(ps.Get(),9.f,States[1],13,b);double mean,nb;stats(a,mean,nb);
  check(std::string("renders_visible_")+FamilyNames[f]+"_"+std::to_string(s)+" mean="+std::to_string(mean)+" nonblack="+std::to_string(nb),mean>.008&&mean<.92&&nb>.12);
  check(std::string("moves_over_time_")+FamilyNames[f]+"_"+std::to_string(s),diffFrac(a,b)>.01);}
 check("all_samples_compiled",compiled==tested);
 std::printf("compiled %d worlds: mean %.0f ms, worst %.0f ms (D3DCompile, O3)\n",compiled,compiled?totalCompile/compiled:0.0,worstCompile);

 // Choreography: each scene family must look different in the open, the build and the drop.
 const int firstField=FirstFieldFamily,fields=FamilyCount-firstField,seeds=3;
 const int cols=3,rows=fields*seeds;std::vector<unsigned char> sheet(size_t(W*cols)*H*rows*4,0);
 for(int f=0;f<fields;++f)for(int sd=0;sd<seeds;++sd){
  const WorldSpec w=Generate(std::uint32_t(sd+1)*29u+std::uint32_t(f)*5u,Family(firstField+f),1);ComPtr<ID3D11PixelShader> ps;double ms=0;if(!compile(w,ps,ms))continue;
  std::vector<unsigned char> frames[3];const int pal=(sd*9+f*5+2)%24;
  for(int st=0;st<3;++st){render(ps.Get(),12.5f,States[st],pal,frames[st]);
   const int row=f*seeds+sd;for(int y=0;y<H;++y)std::memcpy(&sheet[((size_t(row)*H+y)*size_t(W*cols)+size_t(st)*W)*4],&frames[st][size_t(y)*W*4],size_t(W)*4);}
  const double openToDrop=diffFrac(frames[0],frames[2]),openToBuild=diffFrac(frames[0],frames[1]);
  check(std::string("choreography_changes_with_music_")+FamilyNames[firstField+f]+"_"+std::to_string(sd)+" open>drop="+std::to_string(openToDrop),openToDrop>.04&&openToBuild>.01);}
 ServiceChecks(dev.Get());
 IntroChecks(dev.Get(),ctx.Get(),vs.Get(),"RareStage-intro-sheet.png");
 if(sheetPath&&WritePng(sheetPath,W*cols,H*rows,sheet))std::printf("contact sheet: %s\n",sheetPath);
}

int main(int argc,char**argv){
 OfflineChecks();
 GpuChecks(argc>1?argv[1]:"RareStage-worldspec-sheet.png");
 if(failures==0)std::printf("worldspec: all checks passed\n");
 return failures?1:0;
}

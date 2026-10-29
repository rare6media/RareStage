#pragma once
#include "shader_blob_cache.h"
#include "world_grammar.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
// Background compilation of generated worlds. One jthread turns validated specs into ready pixel shaders;
// the render thread only submits specs and polls finished packages, so it never waits on D3DCompile.
//
// Threading contract
//  - The worker only calls ID3D11Device::CreatePixelShader. The immediate context belongs to the renderer.
//  - No lock is held while compiling or touching the disk.
//  - Both queues are bounded. The pending queue refuses new work when full; the ready queue drops its
//    oldest package, because the newest candidate is the one the music is most likely to want.
//  - Packages carry a generation. invalidate(g) makes every package older than g unusable, whether it is
//    still pending, already compiled or in flight.
//  - The service must be destroyed (or stop()ped) before the device it was given is released.
namespace rarestage::gen {

struct CompiledWorld{
 WorldSpec spec{};std::uint64_t hash{0};Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
 float compileMs{0};float complexity{0};bool fromCache{false};std::uint32_t generation{0};
};

struct CompilerStats{
 std::uint32_t submitted{0},compiled{0},failed{0},rejected{0},cacheHits{0},cacheMisses{0},stale{0},dropped{0},refused{0};
 std::uint32_t pending{0},ready{0};
 float lastCompileMs{0};std::uint64_t lastHash{0};int lastFamily{-1};std::uint32_t lastSeed{0};
 bool deviceLost{false};
};

class WorldCompiler{
public:
 static constexpr size_t PendingCapacity=8,ReadyCapacity=4,CacheFileLimit=256;
 static constexpr std::uintmax_t CacheByteLimit=64ull*1024*1024;

 WorldCompiler(ID3D11Device*device,std::filesystem::path cacheDir):device_(device),cacheDir_(std::move(cacheDir)){
  worker_=std::jthread([this](std::stop_token st){run(st);});
 }
 ~WorldCompiler(){stop();}
 WorldCompiler(const WorldCompiler&)=delete;WorldCompiler&operator=(const WorldCompiler&)=delete;

 // Stops accepting work and joins the worker. Safe to call more than once.
 void stop(){
  {std::lock_guard<std::mutex> lock(m_);stopped_=true;pending_.clear();}
  worker_.request_stop();cv_.notify_all();if(worker_.joinable())worker_.join();
 }
 // False when the spec is invalid, the queue is full, or the service is stopping or has lost its device.
 bool submit(const WorldSpec&spec,std::uint32_t generation){
  if(ValidateWorld(spec)!=Reject::None){std::lock_guard<std::mutex> lock(m_);++stats_.rejected;setError("rejected spec");return false;}
  {std::lock_guard<std::mutex> lock(m_);
   if(stopped_||stats_.deviceLost||generation<minGeneration_){++stats_.refused;return false;}
   if(pending_.size()>=PendingCapacity){++stats_.refused;return false;}
   pending_.push_back({spec,generation});++stats_.submitted;}
  cv_.notify_one();return true;
 }
 // Takes the oldest usable package, or nothing. Never blocks on compilation.
 std::optional<CompiledWorld> poll(){
  std::lock_guard<std::mutex> lock(m_);
  while(!ready_.empty()){CompiledWorld w=std::move(ready_.front());ready_.pop_front();if(w.generation<minGeneration_){++stats_.stale;continue;}return w;}
  return std::nullopt;
 }
 // Everything submitted before this call is obsolete. Work already in flight finishes but is discarded.
 void invalidate(std::uint32_t newGeneration){
  std::lock_guard<std::mutex> lock(m_);minGeneration_=std::max(minGeneration_,newGeneration);
  for(auto it=pending_.begin();it!=pending_.end();){if(it->generation<minGeneration_){++stats_.stale;it=pending_.erase(it);}else ++it;}
  for(auto it=ready_.begin();it!=ready_.end();){if(it->generation<minGeneration_){++stats_.stale;it=ready_.erase(it);}else ++it;}
 }
 CompilerStats stats()const{std::lock_guard<std::mutex> lock(m_);CompilerStats s=stats_;s.pending=std::uint32_t(pending_.size());s.ready=std::uint32_t(ready_.size());return s;}
 std::string lastError()const{std::lock_guard<std::mutex> lock(m_);return error_;}

private:
 struct Job{WorldSpec spec;std::uint32_t generation;};
 Microsoft::WRL::ComPtr<ID3D11Device> device_;std::filesystem::path cacheDir_;
 mutable std::mutex m_;std::condition_variable_any cv_;
 std::deque<Job> pending_;std::deque<CompiledWorld> ready_;CompilerStats stats_{};std::string error_;
 std::uint32_t minGeneration_{0};bool stopped_{false};
 std::jthread worker_;   // declared last: destroyed (and joined) before the members it uses

 void setError(const std::string&e){error_=e.substr(0,159);}
 static std::uint64_t CacheKey(std::uint64_t hash){const UINT flags=D3DCOMPILE_OPTIMIZATION_LEVEL3;std::uint64_t k=ShaderHash("psGen|ps_5_0",12,hash);return ShaderHash(&flags,sizeof flags,k);}
 std::filesystem::path CachePath(std::uint64_t hash)const{char n[48];std::snprintf(n,sizeof n,"gen-%016llx.rsc",(unsigned long long)hash);return cacheDir_/n;}

 // Keeps the on-disk cache bounded; runs once on the worker before the first job.
 void pruneCache(){
  std::error_code ec;if(!std::filesystem::exists(cacheDir_,ec))return;
  struct Entry{std::filesystem::path path;std::filesystem::file_time_type time;std::uintmax_t size;};std::vector<Entry> files;std::uintmax_t total=0;
  for(const auto&e:std::filesystem::directory_iterator(cacheDir_,ec)){const auto name=e.path().filename().string();if(name.rfind("gen-",0)!=0||e.path().extension()!=".rsc")continue;
   Entry en{e.path(),e.last_write_time(ec),e.file_size(ec)};total+=en.size;files.push_back(std::move(en));}
  std::sort(files.begin(),files.end(),[](const Entry&a,const Entry&b){return a.time<b.time;});
  size_t i=0;while(i<files.size()&&(files.size()-i>CacheFileLimit||total>CacheByteLimit)){total-=std::min(total,files[i].size);std::filesystem::remove(files[i].path,ec);++i;}
 }

 void run(std::stop_token st){
  pruneCache();
  while(!st.stop_requested()){
   Job job;
   {std::unique_lock<std::mutex> lock(m_);cv_.wait(lock,st,[&]{return !pending_.empty();});if(st.stop_requested())return;job=std::move(pending_.front());pending_.pop_front();
    if(job.generation<minGeneration_){++stats_.stale;continue;}}
   process(job);
  }
 }

 void process(const Job&job){
  const Emitted e=Emit(job.spec);
  if(e.reject!=Reject::None){std::lock_guard<std::mutex> lock(m_);++stats_.rejected;setError(std::string("rejected: ")+RejectName(e.reject));return;}
  const auto start=std::chrono::steady_clock::now();
  Microsoft::WRL::ComPtr<ID3DBlob> blob,diag;bool cached=false;const auto key=CacheKey(e.hash);
  cached=ReadShaderCache(CachePath(e.hash),key,&blob);
  HRESULT hr=S_OK;
  if(!cached){
   hr=D3DCompile(e.source.data(),e.source.size(),"generated-world",nullptr,nullptr,"psGen","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&diag);
   if(SUCCEEDED(hr)){std::error_code ec;std::filesystem::create_directories(cacheDir_,ec);WriteShaderCache(CachePath(e.hash),key,blob.Get());}
  }
  if(FAILED(hr)||!blob){
   std::lock_guard<std::mutex> lock(m_);++stats_.failed;++stats_.cacheMisses;setError(diag?std::string(static_cast<const char*>(diag->GetBufferPointer()),diag->GetBufferSize()):"compile failed");return;}
  CompiledWorld w;w.spec=job.spec;w.spec.shaderHash=e.hash;w.hash=e.hash;w.complexity=e.complexity;w.fromCache=cached;w.generation=job.generation;
  const HRESULT created=device_->CreatePixelShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,&w.shader);
  w.compileMs=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count();
  std::lock_guard<std::mutex> lock(m_);
  if(cached)++stats_.cacheHits;else ++stats_.cacheMisses;
  if(FAILED(created)||!w.shader){++stats_.failed;if(created==DXGI_ERROR_DEVICE_REMOVED||created==DXGI_ERROR_DEVICE_RESET){stats_.deviceLost=true;setError("device lost");}else setError("CreatePixelShader failed");return;}
  if(job.generation<minGeneration_){++stats_.stale;return;}
  // Publish only after the package is complete; drop the oldest ready package rather than grow.
  if(ready_.size()>=ReadyCapacity){ready_.pop_front();++stats_.dropped;}
  ++stats_.compiled;stats_.lastCompileMs=w.compileMs;stats_.lastHash=w.hash;stats_.lastFamily=int(w.spec.family);stats_.lastSeed=w.spec.seed;
  ready_.push_back(std::move(w));
 }
};
}

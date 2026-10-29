#pragma once
#include "frame_constants.h"
#include "world_qualifier.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <optional>
#include <vector>
// Offscreen GPU qualification of one pixel shader. Runs on the renderer's immediate context (it owns it),
// measures with D3D11 timestamp queries and never waits for the GPU: results are collected by polling with
// D3D11_ASYNC_GETDATA_DONOTFLUSH and a DO_NOT_WAIT map, a frame or more after submission.
// The qualifier uses its own render target and constant buffer; the caller restores whatever pipeline state
// it needs afterwards (the renderer sets all of it at the start of each frame).
namespace rarestage::gen {

struct QualifierResult{Measurement measurement;int frames{0};};

class OffscreenQualifier{
public:
 static constexpr int MaxFrames=6;
 bool init(ID3D11Device*device,int width,int height){
  using namespace Microsoft::WRL;
  w_=width;h_=height;D3D11_TEXTURE2D_DESC td{};td.Width=UINT(w_);td.Height=UINT(h_);td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
  if(FAILED(device->CreateTexture2D(&td,nullptr,&rt_))||FAILED(device->CreateRenderTargetView(rt_.Get(),nullptr,&rtv_)))return false;
  td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;if(FAILED(device->CreateTexture2D(&td,nullptr,&stage_)))return false;
  D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;if(FAILED(device->CreateBuffer(&bd,nullptr,&cb_)))return false;
  D3D11_QUERY_DESC qd{};for(auto&f:frames_){qd.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;if(FAILED(device->CreateQuery(&qd,&f.disjoint)))return false;qd.Query=D3D11_QUERY_TIMESTAMP;if(FAILED(device->CreateQuery(&qd,&f.begin))||FAILED(device->CreateQuery(&qd,&f.end)))return false;}
  ready_=true;return true;
 }
 bool ready()const{return ready_;}bool busy()const{return busy_;}
 int width()const{return w_;}int height()const{return h_;}
 // Issues the test frames. Time advances by dt per frame so a moving shader is measured in motion.
 bool start(ID3D11DeviceContext*ctx,ID3D11VertexShader*vs,ID3D11PixelShader*ps,const Constants&base,int frames,float dt,float fullPixels){
  if(!ready_||busy_||!vs||!ps)return false;frames=std::clamp(frames,1,MaxFrames);count_=frames;fullPixels_=fullPixels;
  Constants c=base;c.resolution[0]=float(w_);c.resolution[1]=float(h_);
  D3D11_VIEWPORT vp{0,0,float(w_),float(h_),0,1};ctx->RSSetViewports(1,&vp);ctx->OMSetRenderTargets(1,rtv_.GetAddressOf(),nullptr);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);ctx->VSSetShader(vs,nullptr,0);ctx->PSSetShader(ps,nullptr,0);ctx->PSSetConstantBuffers(0,1,cb_.GetAddressOf());
  for(int i=0;i<frames;++i){
   c.time=base.time+dt*float(i);ctx->UpdateSubresource(cb_.Get(),0,nullptr,&c,0,0);
   ctx->Begin(frames_[size_t(i)].disjoint.Get());ctx->End(frames_[size_t(i)].begin.Get());ctx->Draw(3,0);ctx->End(frames_[size_t(i)].end.Get());ctx->End(frames_[size_t(i)].disjoint.Get());
  }
  ctx->CopyResource(stage_.Get(),rt_.Get());busy_=true;copied_=true;return true;
 }
 // Never blocks. Returns a result once every query and the readback are available.
 std::optional<QualifierResult> poll(ID3D11DeviceContext*ctx){
  if(!busy_)return std::nullopt;
  double sum=0,peak=0;bool valid=true;
  for(int i=0;i<count_;++i){
   D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};UINT64 t0=0,t1=0;auto&f=frames_[size_t(i)];
   if(ctx->GetData(f.disjoint.Get(),&dj,sizeof dj,D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)return std::nullopt;
   if(ctx->GetData(f.begin.Get(),&t0,sizeof t0,D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK||ctx->GetData(f.end.Get(),&t1,sizeof t1,D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)return std::nullopt;
   if(dj.Disjoint||dj.Frequency==0||t1<t0){valid=false;continue;}
   const double ms=double(t1-t0)*1000.0/double(dj.Frequency);sum+=ms;peak=std::max(peak,ms);
  }
  D3D11_MAPPED_SUBRESOURCE m{};const HRESULT hr=ctx->Map(stage_.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&m);
  if(hr==DXGI_ERROR_WAS_STILL_DRAWING)return std::nullopt;
  QualifierResult r;r.frames=count_;Measurement&me=r.measurement;me.testPixels=float(w_)*float(h_);me.fullPixels=fullPixels_;me.timingValid=valid&&count_>0;
  me.avgMs=count_>0?float(sum/count_):0.f;me.maxMs=float(peak);
  if(SUCCEEDED(hr)){
   // Sample a sparse grid of half-float pixels: enough to tell black, blown-out and NaN output apart.
   double luma=0;size_t seen=0,lit=0;bool finite=true;
   for(int y=0;y<h_;y+=4)for(int x=0;x<w_;x+=4){
    const auto*row=reinterpret_cast<const std::uint16_t*>(static_cast<const unsigned char*>(m.pData)+size_t(y)*m.RowPitch);
    float c3[3];for(int k=0;k<3;++k){const std::uint16_t h=row[x*4+k];const int e=(h>>10)&31;if(e==31){finite=false;c3[k]=0;continue;}c3[k]=std::max(0.f,Half(h));}
    const float tm=[&]{const float l=c3[0]*.2126f+c3[1]*.7152f+c3[2]*.0722f;return l*(1.f+l/4.f)/(1.f+l);}();
    luma+=tm;++seen;lit+=tm>.02f;
   }
   ctx->Unmap(stage_.Get(),0);me.meanLuma=seen?float(luma/double(seen)):0.f;me.nonBlack=seen?float(lit)/float(seen):0.f;me.finite=finite;
  }else me.timingValid=false;
  busy_=false;return r;
 }
 // Gives up on an in-flight measurement (device removed, shader replaced). Queries are simply abandoned.
 void cancel(){busy_=false;}
private:
 static float Half(std::uint16_t h){const int e=(h>>10)&31,m=h&1023;float v=e==0?std::ldexp(float(m),-24):std::ldexp(float(m|1024),e-25);return (h&0x8000)?-v:v;}
 struct FrameQueries{Microsoft::WRL::ComPtr<ID3D11Query> disjoint,begin,end;};
 Microsoft::WRL::ComPtr<ID3D11Texture2D> rt_,stage_;Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;Microsoft::WRL::ComPtr<ID3D11Buffer> cb_;
 FrameQueries frames_[MaxFrames];int w_{0},h_{0},count_{0};float fullPixels_{0};bool ready_{false},busy_{false},copied_{false};
};
}

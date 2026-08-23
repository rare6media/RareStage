#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <wincodec.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstring>
#include <cstdint>
#include <cwchar>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;
constexpr float PI = 3.14159265358979323846f;

static std::filesystem::path StartupLogPath() {
    wchar_t module[MAX_PATH]{}; GetModuleFileNameW(nullptr,module,MAX_PATH);
    return std::filesystem::path(module).replace_filename(L"RareStage-startup.log");
}
static void StartupLog(const char* message) {
    std::ofstream log(StartupLogPath(),std::ios::app); log << message << "\n"; log.flush();
}
static LONG WINAPI CrashLog(EXCEPTION_POINTERS* info) {
    std::ofstream log(StartupLogPath(),std::ios::app); log << "FATAL Windows exception: 0x" << std::hex << info->ExceptionRecord->ExceptionCode << "\n"; log.flush();
    return EXCEPTION_EXECUTE_HANDLER;
}

struct AudioFrame { float bass{}, mid{}, high{}, level{}, peak{}, stereo{}; };

static std::wstring DeviceName(IMMDevice* device) {
    ComPtr<IPropertyStore> properties;
    PROPVARIANT value{}; PropVariantInit(&value);
    std::wstring name = L"Unknown output";
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties)) &&
        SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR) name = value.pwszVal;
    PropVariantClear(&value); return name;
}

class AudioEngine {
public:
    ~AudioEngine() { stop(); }
    bool start(size_t requested = 0) {
        stop(); deviceIndex_ = requested; running_ = true;
        worker_ = std::thread([this] { capture(); }); return true;
    }
    void stop() { running_ = false; if (worker_.joinable()) worker_.join(); }
    void cycle() { auto count = deviceCount(); start(count ? (deviceIndex_ + 1) % count : 0); }
    AudioFrame frame() const { std::scoped_lock lock(mutex_); return frame_; }
    std::wstring name() const { std::scoped_lock lock(mutex_); return name_; }

private:
    size_t deviceCount() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ComPtr<IMMDeviceEnumerator> e; ComPtr<IMMDeviceCollection> c; UINT n = 0;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&e))) &&
            SUCCEEDED(e->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &c))) c->GetCount(&n);
        CoUninitialize(); return n;
    }
    static void fft(std::array<std::complex<float>, 1024>& a) {
        for (size_t i=1,j=0;i<a.size();++i) { size_t bit=a.size()>>1; for(;j&bit;bit>>=1)j^=bit; j^=bit; if(i<j)std::swap(a[i],a[j]); }
        for(size_t len=2;len<=a.size();len<<=1) { auto wlen=std::polar(1.0f,-2.0f*PI/float(len));
            for(size_t i=0;i<a.size();i+=len){std::complex<float>w(1);for(size_t j=0;j<len/2;++j){auto u=a[i+j],v=a[i+j+len/2]*w;a[i+j]=u+v;a[i+j+len/2]=u-v;w*=wlen;}}
        }
    }
    void analyze(const std::array<float,1024>& mono, float stereo) {
        std::array<std::complex<float>,1024> bins{}; float level=0, peak=0;
        for(size_t i=0;i<mono.size();++i){float s=mono[i]; level+=s*s; peak=std::max(peak,std::abs(s)); float w=.5f-.5f*std::cos(2*PI*float(i)/1023); bins[i]={s*w,0};}
        fft(bins); auto band=[&](int lo,int hi){float v=0;for(int i=lo;i<=hi;++i)v+=std::abs(bins[i]);return v/float(hi-lo+1);};
        AudioFrame next{band(1,7)*2.8f, band(8,70)*5.0f, band(71,300)*9.0f, std::sqrt(level/1024), peak, stereo};
        std::scoped_lock lock(mutex_); constexpr float attack=.48f, release=.12f;
        auto smooth=[](float old,float value){float k=value>old?attack:release;return old+(value-old)*k;};
        frame_.bass=smooth(frame_.bass,std::min(next.bass,1.5f)); frame_.mid=smooth(frame_.mid,std::min(next.mid,1.5f));
        frame_.high=smooth(frame_.high,std::min(next.high,1.5f)); frame_.level=smooth(frame_.level,std::min(next.level*4,1.5f));
        frame_.peak=smooth(frame_.peak,next.peak); frame_.stereo=smooth(frame_.stereo,stereo);
    }
    void capture() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED); DWORD task=0; HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Audio",&task);
        ComPtr<IMMDeviceEnumerator> enumerator; ComPtr<IMMDeviceCollection> devices; ComPtr<IMMDevice> device;
        if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator))) ||
           FAILED(enumerator->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&devices))) goto done;
        UINT count; devices->GetCount(&count); if(!count) goto done; deviceIndex_%=count; if(FAILED(devices->Item((UINT)deviceIndex_,&device)))goto done;
        { std::scoped_lock lock(mutex_); name_=DeviceName(device.Get()); }
        { ComPtr<IAudioClient> client; ComPtr<IAudioCaptureClient> captureClient; WAVEFORMATEX* format=nullptr;
          if(FAILED(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void**)&client)) || FAILED(client->GetMixFormat(&format)))goto done;
          auto freeFormat=[&]{if(format)CoTaskMemFree(format);format=nullptr;};
          REFERENCE_TIME duration=10000000; if(FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,duration,0,format,nullptr)) ||
             FAILED(client->GetService(IID_PPV_ARGS(&captureClient))) || FAILED(client->Start())){freeFormat();goto done;}
          std::array<float,1024> ring{}; size_t pos=0; float stereoAccum=0; size_t stereoCount=0;
          bool isFloat=format->wFormatTag==WAVE_FORMAT_IEEE_FLOAT || (format->wFormatTag==WAVE_FORMAT_EXTENSIBLE && IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));
          while(running_){UINT32 packets=0;captureClient->GetNextPacketSize(&packets);if(!packets){Sleep(3);continue;} BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;
            if(FAILED(captureClient->GetBuffer(&data,&frames,&flags,nullptr,nullptr)))continue;
            for(UINT32 f=0;f<frames;++f){float l=0,r=0;if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT)){if(isFloat){auto*p=reinterpret_cast<float*>(data)+f*format->nChannels;l=p[0];r=format->nChannels>1?p[1]:l;}else if(format->wBitsPerSample==16){auto*p=reinterpret_cast<int16_t*>(data)+f*format->nChannels;l=p[0]/32768.f;r=format->nChannels>1?p[1]/32768.f:l;}}
              ring[pos++]=(l+r)*.5f;stereoAccum+=std::abs(l-r);++stereoCount;if(pos==ring.size()){analyze(ring,stereoCount?std::min(stereoAccum/stereoCount*4,1.f):0);pos=0;stereoAccum=0;stereoCount=0;}}
            captureClient->ReleaseBuffer(frames);
          } client->Stop();freeFormat(); }
    done: if(mmcss)AvRevertMmThreadCharacteristics(mmcss); CoUninitialize();
    }
    std::atomic<bool> running_{false}; std::thread worker_; size_t deviceIndex_{};
    mutable std::mutex mutex_; AudioFrame frame_{}; std::wstring name_=L"Starting audio capture...";
};

static const char* Shader = R"(
cbuffer Frame : register(b0){float2 resolution;float time;float scene;float bass;float mid;float high;float level;float peak;float stereo;float pad0;float pad1;}
Texture2D logoTex:register(t0);SamplerState logoSampler:register(s0);
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
O vs(uint id:SV_VertexID){O o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float3 pal(float x){return .48+.48*cos(6.28318*(x+float3(.02,.22,.58)));}
float ring(float2 p,float r,float w){return w/max(abs(length(p)-r),.001);}
float4 ps(O i):SV_TARGET{
 float2 p=(i.uv-.5)*float2(resolution.x/resolution.y,1);float t=time;float3 c=float3(.002,.008,.018);float s=scene;float r=length(p),a=atan2(p.y,p.x);
 float impact=saturate(bass*.72+peak*.55),detail=saturate(high*.8+peak*.25),motion=saturate(mid*.75+level*.25);
 if(s<.5){
   float core=ring(p,.31+impact*.045,.0025+peak*.008);float orbit=ring(p,.46+sin(a*3+t*(.35+motion))*.035,.0015+mid*.003);
   float arcs=pow(saturate(.028/max(abs(sin(a*6+t*(.4+motion))-r*8),.002)),1.15);
   float sparks=step(.985,hash(floor(float2(a*46,r*80-t*18))))*detail/max(r,.12);
   c+=float3(.02,.82,1.2)*core*(1+impact*2.2)+float3(.65,1.1,.05)*orbit*(.3+motion)+pal(a/6.28)*arcs*.10+sparks*float3(.3,.9,1.2);
 }else if(s<1.5){
   float horizon=p.y-.16+sin(p.x*3+t*.35)*.01*motion;float depth=max(.05,-horizon);float2 gp=float2(p.x/depth,1/depth+t*(.45+motion));
   float2 q=abs(frac(gp*(1.6+impact*.2))-.5);float grid=.008/max(min(q.x,q.y)*depth,.002);float skyline=step(.92,hash(floor(float2(p.x*22,0))))*.012/max(abs(horizon-.05-hash(floor(float2(p.x*22,1)))*.22),.006);
   float scan=pow(saturate(sin(p.x*30+t*5)*.5+.5),20)*detail*.35;float bassWave=.01/max(abs(horizon+impact*.035),.004);
   c+=grid*float3(.01,.45,.8)+skyline*float3(.55,1,.05)+bassWave*float3(.05,.85,1)+scan*float3(.5,.1,1);c*=saturate(.85-p.y);
 }else if(s<2.5){
   float tunnel=pow(abs(sin(a*36+t*(1+motion))),18)*detail/max(r,.09);float gate=ring(p,.38+impact*.06,.003+peak*.012);float inner=ring(p,.27+sin(t*.8)*.012,.002+mid*.004);
   float stars=step(.987,hash(floor(float2(a*90,r*110-t*(18+high*30)))))*(detail+.12)/max(r,.15);
   c+=pal(a/6.28+t*.035)*(tunnel*.16+gate*1.5)+inner*float3(.65,1,.05)+stars*float3(.3,.8,1.2);
 }else if(s<3.5){
   float2 cell=floor(p*11),f=frac(p*11)-.5;float h=hash(cell),nodeD=length(f-float2(sin(h*19+t*(.4+motion)),cos(h*23+t*.45))*.22);
   float nodes=.013/max(nodeD,.012)*(.25+motion);float links=.004/max(abs(f.y-sin(f.x*4+h*8+t*.5)*.13),.01)*(.15+mid);float pulse=ring(p,frac(t*.18)*.85,.003+impact*.006);
   float bits=step(.975,hash(cell+floor(t*8)))*detail*.7;c+=pal(h*.5+t*.02)*(nodes+links*.3)+pulse*float3(.1,1,.65)+bits*float3(.7,1,.1);
 }else{
   float burst=pow(abs(sin(a*20+t*.35)),28)*(impact+peak*1.6)/max(r,.1);float speed=.008/max(abs(frac(r*9-t*(.45+level))-0.5),.018);float petals=ring(p,.42+sin(a*7-t)*.035*motion,.002+mid*.004);
   float cuts=step(.96,hash(floor(float2(a*35,t*8))))*detail*.15/max(r,.12);c+=float3(1,.06,.3)*burst+float3(.02,.65,1.2)*speed*detail+float3(.65,1,.05)*petals+cuts;
 }
 float logoSize=.76+impact*.035;float2 luv=float2(p.x/logoSize+.5,.5-p.y/logoSize);float inside=step(0,luv.x)*step(luv.x,1)*step(0,luv.y)*step(luv.y,1);
 float3 rawLogo=logoTex.Sample(logoSampler,saturate(luv)).rgb;float3 logo=saturate((rawLogo-float3(.015,.035,.065))*1.28);float logoL=max(logo.r,max(logo.g,logo.b));float logoMask=inside*saturate((logoL-.018)*3.5);
 float halo=ring(p,.39+impact*.035,.007+impact*.012);c+=halo*float3(.02,.75,1.0)*(.25+impact);
 logo*=.72+detail*.18;c=1-(1-saturate(c))*(1-logo*logoMask);c+=logoMask*impact*.05;
 c=1-exp(-c);c+=hash(i.uv*resolution+time)*.008;return float4(pow(saturate(c),.86),1);
})";

struct Constants { float resolution[2],time,scene,bass,mid,high,level,peak,stereo,pad0,pad1; };

class App {
public:
    bool init(HINSTANCE instance) {
        StartupLog("Creating RareStage window class");
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=WndProc;wc.lpszClassName=L"RareStageWindow";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
        hwnd_=CreateWindowExW(0,wc.lpszClassName,L"RareStage",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,720,nullptr,nullptr,instance,this);
        if(!hwnd_){StartupLog("CreateWindowExW failed");return false;}
        StartupLog("Initializing DirectX renderer");
        if(!createGpu()){MessageBoxW(hwnd_,L"RareStage could not initialize DirectX. Update your graphics driver and try again.",L"RareStage startup error",MB_OK|MB_ICONERROR);return false;}
        StartupLog("DirectX renderer ready");
        ShowWindow(hwnd_,SW_SHOW);UpdateWindow(hwnd_);StartupLog("Window shown");
        audio_.start();StartupLog("Audio capture thread started");started_=Clock::now();return true;
    }
    void requestClose(){PostMessageW(hwnd_,WM_CLOSE,0,0);}
    int run(){MSG msg{};while(msg.message!=WM_QUIT){if(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}else render();}audio_.stop();return(int)msg.wParam;}
private:
    static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){App*a=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){a=reinterpret_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)a);}if(a)return a->message(h,m,w,l);return DefWindowProcW(h,m,w,l);}
    LRESULT message(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_DESTROY){PostQuitMessage(0);return 0;}if(m==WM_SIZE&&swap_&&w!=SIZE_MINIMIZED){resize(LOWORD(l),HIWORD(l));return 0;}if(m==WM_SYSKEYDOWN&&w==VK_RETURN){fullscreen();return 0;}if(m==WM_KEYDOWN){if(w>=L'1'&&w<=L'5'){scene_=int(w-L'1');automatic_=false;}else if(w==L'0')automatic_=true;else if(w==L'D')audio_.cycle();else if(w==VK_F11)fullscreen();else if(w==VK_SPACE)frozen_=!frozen_;else if(w==VK_ESCAPE){if(fullscreen_)fullscreen();else DestroyWindow(h);}return 0;}return DefWindowProcW(h,m,w,l);}
    bool loadLogo(){
        wchar_t module[MAX_PATH]{};GetModuleFileNameW(nullptr,module,MAX_PATH);auto path=std::filesystem::path(module).parent_path()/L"assets"/L"rare-logo.png";
        ComPtr<IWICImagingFactory>factory;ComPtr<IWICBitmapDecoder>decoder;ComPtr<IWICBitmapFrameDecode>frame;ComPtr<IWICFormatConverter>converter;
        if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||
           FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||
           FAILED(decoder->GetFrame(0,&frame))||FAILED(factory->CreateFormatConverter(&converter))||
           FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))){StartupLog("Rare logo decode failed");return false;}
        UINT w=0,h=0;converter->GetSize(&w,&h);if(!w||!h)return false;std::vector<BYTE>pixels(size_t(w)*h*4);
        if(FAILED(converter->CopyPixels(nullptr,w*4,(UINT)pixels.size(),pixels.data())))return false;
        D3D11_TEXTURE2D_DESC td{};td.Width=w;td.Height=h;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{pixels.data(),w*4,0};ComPtr<ID3D11Texture2D>texture;
        if(FAILED(device_->CreateTexture2D(&td,&data,&texture))||FAILED(device_->CreateShaderResourceView(texture.Get(),nullptr,&logoSrv_)))return false;
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        if(FAILED(device_->CreateSamplerState(&sd,&logoSampler_)))return false;StartupLog("Rare logo loaded");return true;
    }
    bool createGpu(){DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=hwnd_;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        D3D_FEATURE_LEVEL fl;UINT flags=0;
#ifdef _DEBUG
        flags|=D3D11_CREATE_DEVICE_DEBUG;
#endif
        HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&sd,&swap_,&device_,&fl,&context_);
        if(FAILED(hr)){swap_.Reset();device_.Reset();context_.Reset();hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&sd,&swap_,&device_,&fl,&context_);}
        if(FAILED(hr)){StartupLog("D3D11 device and swap chain creation failed");return false;}
        ComPtr<ID3DBlob> vsb,psb,err;
        if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vsb,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage vertex shader error",MB_OK|MB_ICONERROR);return false;}
        err.Reset();if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&psb,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage pixel shader error",MB_OK|MB_ICONERROR);return false;}
        if(FAILED(device_->CreateVertexShader(vsb->GetBufferPointer(),vsb->GetBufferSize(),nullptr,&vs_))){StartupLog("Vertex shader creation failed");return false;}
        if(FAILED(device_->CreatePixelShader(psb->GetBufferPointer(),psb->GetBufferSize(),nullptr,&ps_))){StartupLog("Pixel shader creation failed");return false;}
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(device_->CreateBuffer(&bd,nullptr,&constants_))){StartupLog("Frame constant buffer creation failed");return false;}if(!loadLogo())return false;RECT r;GetClientRect(hwnd_,&r);resize(r.right,r.bottom);return rtv_.Get()!=nullptr;}
    void resize(UINT w,UINT h){if(!w||!h)return;rtv_.Reset();context_->OMSetRenderTargets(0,nullptr,nullptr);swap_->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0);ComPtr<ID3D11Texture2D>b;swap_->GetBuffer(0,IID_PPV_ARGS(&b));device_->CreateRenderTargetView(b.Get(),nullptr,&rtv_);width_=w;height_=h;}
    void render(){if(!rtv_){Sleep(10);return;}float seconds=std::chrono::duration<float>(Clock::now()-started_).count();auto af=audio_.frame();if(automatic_&&!frozen_)scene_=int(seconds/18.f)%5;
        Constants c{{float(width_),float(height_)},seconds,float(scene_),af.bass,af.mid,af.high,af.level,af.peak,af.stereo,0,0};D3D11_MAPPED_SUBRESOURCE map{};context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map);memcpy(map.pData,&c,sizeof(c));context_->Unmap(constants_.Get(),0);
        D3D11_VIEWPORT vp{0,0,float(width_),float(height_),0,1};context_->RSSetViewports(1,&vp);context_->OMSetRenderTargets(1,rtv_.GetAddressOf(),nullptr);context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetShader(ps_.Get(),nullptr,0);context_->PSSetConstantBuffers(0,1,constants_.GetAddressOf());context_->PSSetShaderResources(0,1,logoSrv_.GetAddressOf());context_->PSSetSamplers(0,1,logoSampler_.GetAddressOf());context_->Draw(3,0);swap_->Present(1,0);
        ++frames_;if(seconds-lastTitle_>1){float fps=frames_/(seconds-lastTitle_);frames_=0;lastTitle_=seconds;static const wchar_t* names[]={L"Void Core",L"Neon Grid",L"Star Gate",L"Neural Space",L"Anime Burst"};std::wstring title=L"RareStage  |  "+std::wstring(names[scene_])+(automatic_?L" [AUTO]":L"")+L"  |  "+audio_.name()+L"  |  "+std::to_wstring((int)fps)+L" FPS";SetWindowTextW(hwnd_,title.c_str());}}
    void fullscreen(){fullscreen_=!fullscreen_;if(fullscreen_){GetWindowRect(hwnd_,&windowRect_);SetWindowLongW(hwnd_,GWL_STYLE,WS_POPUP);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&mi);SetWindowPos(hwnd_,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED);}else{SetWindowLongW(hwnd_,GWL_STYLE,WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd_,nullptr,windowRect_.left,windowRect_.top,windowRect_.right-windowRect_.left,windowRect_.bottom-windowRect_.top,SWP_FRAMECHANGED|SWP_NOZORDER);}}
    HWND hwnd_{};UINT width_=1280,height_=720;bool fullscreen_=false,automatic_=true,frozen_=false;RECT windowRect_{};int scene_=0;AudioEngine audio_;
    Clock::time_point started_;float lastTitle_{};int frames_{};ComPtr<ID3D11Device>device_;ComPtr<ID3D11DeviceContext>context_;ComPtr<IDXGISwapChain>swap_;ComPtr<ID3D11RenderTargetView>rtv_;ComPtr<ID3D11VertexShader>vs_;ComPtr<ID3D11PixelShader>ps_;ComPtr<ID3D11Buffer>constants_;ComPtr<ID3D11ShaderResourceView>logoSrv_;ComPtr<ID3D11SamplerState>logoSampler_;
};

int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int){
    SetUnhandledExceptionFilter(CrashLog);
    {std::ofstream clear(StartupLogPath());clear << "RareStage startup beginning\n";}
    if(std::wcsstr(GetCommandLineW(),L"--self-test")){
        ComPtr<ID3DBlob> blob,error;
        if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 2;}
        blob.Reset();error.Reset();
        if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 3;}
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);App app;
    if(!app.init(h)){StartupLog("Startup failed");if(SUCCEEDED(com))CoUninitialize();return 1;}
    if(std::wcsstr(GetCommandLineW(),L"--startup-test"))app.requestClose();
    int result=app.run();StartupLog("RareStage exited normally");if(SUCCEEDED(com))CoUninitialize();return result;
}

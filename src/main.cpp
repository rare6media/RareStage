#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;
constexpr float PI = 3.14159265358979323846f;

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
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
O vs(uint id:SV_VertexID){O o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float3 pal(float x){return .48+.48*cos(6.28318*(x+float3(.02,.22,.58)));}
float ring(float2 p,float r,float w){return w/max(abs(length(p)-r),.001);}
float4 ps(O i):SV_TARGET{
 float2 p=(i.uv-.5)*float2(resolution.x/resolution.y,1);float t=time;float3 c=0;float s=scene;
 if(s<.5){float a=atan2(p.y,p.x);float r=length(p);float core=ring(p,.22+bass*.06,.0025+peak*.009);float arcs=pow(saturate(.025/abs(sin(a*5+t*(.5+mid))-r*7)),1.2);c=pal(a/6.28+t*.03)*(core*(1.2+bass*2)+arcs*.12);c+=pal(t*.05)*.012/(r*r+.01);}
 else if(s<1.5){float3 ro=float3(0,1.2,-2.5),rd=normalize(float3(p.x,p.y-.18,1));float q=abs(frac((ro.xz+rd.xz*max(0,-ro.y/rd.y))*(2.5+bass))-0.5);float grid=.012/min(q.x,q.y);float horizon=.018/abs(p.y-.12+sin(p.x*3+t)*.008*mid);c=float3(.02,.3,1)*grid+float3(.7,.05,1)*horizon;c*=saturate(.7-p.y);}
 else if(s<2.5){float r=length(p),a=atan2(p.y,p.x);float rays=pow(saturate(sin(a*42+t*2)*.5+.5),12)*(high+.15)/max(r,.08);float gate=ring(p,.28+bass*.08,.004+peak*.012);float stars=step(.994,hash(floor((p/max(r,.05)-t*.18)*170)))*(high+level);c=pal(a/6.28+t*.08)*(rays*.16+gate*1.8)+stars;}
 else if(s<3.5){float2 cell=floor(p*9),f=frac(p*9)-.5;float h=hash(cell),d=length(f-float2(sin(h*19+t),cos(h*23+t))*.2);float nodes=.018/(d+.01)*(mid+.2);float links=.006/max(abs(f.y-sin(f.x*4+h*8+t)*.15),.012)*(bass+.1);c=pal(h+t*.03)*(nodes+links*.22);c*=.7+.3*sin(length(p)*8-t);}
 else {float r=length(p),a=atan2(p.y,p.x);float burst=pow(abs(sin(a*18+t*.5)),24)*(bass+peak*2)/max(r,.08);float speed=.01/max(abs(frac(r*8-t*(.5+level))-0.5),.02);float sun=.018/(r+.012);c=float3(1,.08,.35)*burst+float3(.1,.4,1)*speed*high+pal(t*.08)*sun*(.15+bass);}
 c=1-exp(-c);c+=hash(i.uv*resolution+time)*.012;return float4(pow(saturate(c),.82),1);
})";

struct Constants { float resolution[2],time,scene,bass,mid,high,level,peak,stereo,pad0,pad1; };

class App {
public:
    bool init(HINSTANCE instance) {
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=WndProc;wc.lpszClassName=L"RareStageWindow";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
        hwnd_=CreateWindowExW(0,wc.lpszClassName,L"RareStage",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,720,nullptr,nullptr,instance,this);
        if(!hwnd_||!createGpu())return false;ShowWindow(hwnd_,SW_SHOW);audio_.start();started_=Clock::now();return true;
    }
    int run(){MSG msg{};while(msg.message!=WM_QUIT){if(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}else render();}audio_.stop();return(int)msg.wParam;}
private:
    static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){App*a=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){a=reinterpret_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)a);}if(a)return a->message(m,w,l);return DefWindowProcW(h,m,w,l);}
    LRESULT message(UINT m,WPARAM w,LPARAM l){if(m==WM_DESTROY){PostQuitMessage(0);return 0;}if(m==WM_SIZE&&swap_&&w!=SIZE_MINIMIZED){resize(LOWORD(l),HIWORD(l));return 0;}if(m==WM_SYSKEYDOWN&&w==VK_RETURN){fullscreen();return 0;}if(m==WM_KEYDOWN){if(w>=L'1'&&w<=L'5'){scene_=int(w-L'1');automatic_=false;}else if(w==L'0')automatic_=true;else if(w==L'D')audio_.cycle();else if(w==VK_F11)fullscreen();else if(w==VK_SPACE)frozen_=!frozen_;else if(w==VK_ESCAPE){if(fullscreen_)fullscreen();else DestroyWindow(hwnd_);}return 0;}return DefWindowProcW(hwnd_,m,w,l);}
    bool createGpu(){DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=hwnd_;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        D3D_FEATURE_LEVEL fl;UINT flags=0;
#ifdef _DEBUG
        flags|=D3D11_CREATE_DEVICE_DEBUG;
#endif
        if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&sd,&swap_,&device_,&fl,&context_)))return false;
        ComPtr<ID3DBlob> vsb,psb,err;if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vsb,&err)))return false;if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&psb,&err)))return false;
        device_->CreateVertexShader(vsb->GetBufferPointer(),vsb->GetBufferSize(),nullptr,&vs_);device_->CreatePixelShader(psb->GetBufferPointer(),psb->GetBufferSize(),nullptr,&ps_);
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;device_->CreateBuffer(&bd,nullptr,&constants_);RECT r;GetClientRect(hwnd_,&r);resize(r.right,r.bottom);return true;}
    void resize(UINT w,UINT h){if(!w||!h)return;rtv_.Reset();context_->OMSetRenderTargets(0,nullptr,nullptr);swap_->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0);ComPtr<ID3D11Texture2D>b;swap_->GetBuffer(0,IID_PPV_ARGS(&b));device_->CreateRenderTargetView(b.Get(),nullptr,&rtv_);width_=w;height_=h;}
    void render(){if(!rtv_){Sleep(10);return;}float seconds=std::chrono::duration<float>(Clock::now()-started_).count();auto af=audio_.frame();if(automatic_&&!frozen_)scene_=int(seconds/18.f)%5;
        Constants c{{float(width_),float(height_)},seconds,float(scene_),af.bass,af.mid,af.high,af.level,af.peak,af.stereo,0,0};D3D11_MAPPED_SUBRESOURCE map{};context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map);memcpy(map.pData,&c,sizeof(c));context_->Unmap(constants_.Get(),0);
        D3D11_VIEWPORT vp{0,0,float(width_),float(height_),0,1};context_->RSSetViewports(1,&vp);context_->OMSetRenderTargets(1,rtv_.GetAddressOf(),nullptr);context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetShader(ps_.Get(),nullptr,0);context_->PSSetConstantBuffers(0,1,constants_.GetAddressOf());context_->Draw(3,0);swap_->Present(1,0);
        ++frames_;if(seconds-lastTitle_>1){float fps=frames_/(seconds-lastTitle_);frames_=0;lastTitle_=seconds;static const wchar_t* names[]={L"Void Core",L"Neon Grid",L"Star Gate",L"Neural Space",L"Anime Burst"};std::wstring title=L"RareStage  |  "+std::wstring(names[scene_])+(automatic_?L" [AUTO]":L"")+L"  |  "+audio_.name()+L"  |  "+std::to_wstring((int)fps)+L" FPS";SetWindowTextW(hwnd_,title.c_str());}}
    void fullscreen(){fullscreen_=!fullscreen_;if(fullscreen_){GetWindowRect(hwnd_,&windowRect_);SetWindowLongW(hwnd_,GWL_STYLE,WS_POPUP);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&mi);SetWindowPos(hwnd_,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED);}else{SetWindowLongW(hwnd_,GWL_STYLE,WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd_,nullptr,windowRect_.left,windowRect_.top,windowRect_.right-windowRect_.left,windowRect_.bottom-windowRect_.top,SWP_FRAMECHANGED|SWP_NOZORDER);}}
    HWND hwnd_{};UINT width_=1280,height_=720;bool fullscreen_=false,automatic_=true,frozen_=false;RECT windowRect_{};int scene_=0;AudioEngine audio_;
    Clock::time_point started_;float lastTitle_{};int frames_{};ComPtr<ID3D11Device>device_;ComPtr<ID3D11DeviceContext>context_;ComPtr<IDXGISwapChain>swap_;ComPtr<ID3D11RenderTargetView>rtv_;ComPtr<ID3D11VertexShader>vs_;ComPtr<ID3D11PixelShader>ps_;ComPtr<ID3D11Buffer>constants_;
};

int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int){SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);App app;return app.init(h)?app.run():1;}

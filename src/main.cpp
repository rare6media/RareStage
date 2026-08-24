#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <wincodec.h>

#ifdef RARESTAGE_SPOUT
#include <SpoutDX.h>
#endif

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
#include <sstream>
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

struct AudioFrame { float bass{}, mid{}, high{}, level{}, peak{}, stereo{}, beat{}, flux{}, onset{}, bpm{}, tempoConfidence{}; };

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
        stop(); deviceIndex_ = requested;
        ceilings_.fill(.0001f);previousBass_=previousMid_=previousRms_=previousPeak_=beatEnvelope_=fluxEnvelope_=beatInterval_=tempoConfidence_=0;beatCooldown_=0;lastBeatTime_={};
        {std::scoped_lock lock(mutex_);frame_={};name_=L"Starting audio capture...";}
        running_ = true;
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
        float rms=std::sqrt(level/1024);std::array<float,4>raw{band(1,5),band(6,60),band(61,350),rms*256};
        float audible=saturateAudio((rms-.00015f)*1200.f);std::array<float,4>normalized{};
        for(size_t n=0;n<raw.size();++n){ceilings_[n]=std::max(raw[n],std::max(ceilings_[n]*.9985f,.00001f));normalized[n]=saturateAudio(raw[n]/(ceilings_[n]*1.35f))*audible;}
        float flux=std::max(0.f,normalized[0]-previousBass_)+.45f*std::max(0.f,normalized[1]-previousMid_);float onset=saturateAudio(flux*4.8f+std::max(0.f,rms-previousRms_)*46.f+std::max(0.f,peak-previousPeak_)*1.25f);previousBass_=normalized[0];previousMid_=normalized[1];previousRms_=rms;previousPeak_=peak;
        if(beatCooldown_>0)--beatCooldown_;float beatTarget=0;bool beatDetected=((flux>.045f&&normalized[0]>.13f)||(onset>.52f&&normalized[0]>.10f))&&beatCooldown_==0;
        if(beatDetected){beatTarget=.96f;beatCooldown_=5;auto now=Clock::now();if(lastBeatTime_.time_since_epoch().count()!=0){float interval=std::chrono::duration<float>(now-lastBeatTime_).count();while(interval<.30f)interval*=2;while(interval>1.f)interval*=.5f;if(interval>=.30f&&interval<=1.f){if(beatInterval_==0){beatInterval_=interval;tempoConfidence_=.20f;}else{float consistency=saturateAudio(1-std::abs(interval-beatInterval_)/.12f);beatInterval_+=(interval-beatInterval_)*.28f;tempoConfidence_=saturateAudio(tempoConfidence_*.78f+consistency*.22f);}}}lastBeatTime_=now;}
        if(lastBeatTime_.time_since_epoch().count()!=0&&std::chrono::duration<float>(Clock::now()-lastBeatTime_).count()>3.f)tempoConfidence_*=.995f;
        beatEnvelope_=beatTarget>0?beatEnvelope_+(beatTarget-beatEnvelope_)*.82f:beatEnvelope_*.72f;fluxEnvelope_=std::max(flux,fluxEnvelope_*.72f);
        AudioFrame next{normalized[0],normalized[1],normalized[2],normalized[3],saturateAudio(peak*2.6f),stereo,beatEnvelope_,saturateAudio(fluxEnvelope_*2.8f),onset,beatInterval_>0?60.f/beatInterval_:0,tempoConfidence_};
        std::scoped_lock lock(mutex_); constexpr float attack=.52f, release=.11f;
        auto smooth=[](float old,float value){float k=value>old?attack:release;return old+(value-old)*k;};
        frame_.bass=smooth(frame_.bass,next.bass);frame_.mid=smooth(frame_.mid,next.mid);frame_.high=smooth(frame_.high,next.high);frame_.level=smooth(frame_.level,next.level);
        frame_.peak=smooth(frame_.peak,next.peak);frame_.stereo=smooth(frame_.stereo,stereo);frame_.beat=std::max(next.beat,frame_.beat*.66f);frame_.flux=std::max(next.flux,frame_.flux*.70f);frame_.onset=std::max(next.onset,frame_.onset*.52f);frame_.bpm=next.bpm;frame_.tempoConfidence=smooth(frame_.tempoConfidence,next.tempoConfidence);
    }
    static float saturateAudio(float value){return std::clamp(value,0.f,1.f);}
    void capture() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED); DWORD task=0; HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Audio",&task);bool captureStarted=false;
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
          captureStarted=true;
          std::array<float,1024> ring{}; size_t pos=0; float stereoAccum=0; size_t stereoCount=0;
          bool isFloat=format->wFormatTag==WAVE_FORMAT_IEEE_FLOAT || (format->wFormatTag==WAVE_FORMAT_EXTENSIBLE && IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));
          bool isPcm=format->wFormatTag==WAVE_FORMAT_PCM || (format->wFormatTag==WAVE_FORMAT_EXTENSIBLE && IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat,KSDATAFORMAT_SUBTYPE_PCM));
          auto sample=[&](const BYTE*base,UINT channel){const BYTE*p=base+channel*(format->wBitsPerSample/8);if(isFloat&&format->wBitsPerSample==32){float v;memcpy(&v,p,4);return v;}if(isPcm&&format->wBitsPerSample==16){int16_t v;memcpy(&v,p,2);return v/32768.f;}if(isPcm&&format->wBitsPerSample==24){int32_t v=int32_t(p[0])|(int32_t(p[1])<<8)|(int32_t(p[2])<<16);if(v&0x800000)v|=~0xffffff;return v/8388608.f;}if(isPcm&&format->wBitsPerSample==32){int32_t v;memcpy(&v,p,4);return v/2147483648.f;}return 0.f;};
          while(running_){UINT32 packets=0;captureClient->GetNextPacketSize(&packets);if(!packets){Sleep(3);continue;} BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;
            if(FAILED(captureClient->GetBuffer(&data,&frames,&flags,nullptr,nullptr)))continue;
            for(UINT32 f=0;f<frames;++f){float l=0,r=0;if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT)){const BYTE*base=data+size_t(f)*format->nBlockAlign;l=sample(base,0);r=format->nChannels>1?sample(base,1):l;}
              ring[pos++]=(l+r)*.5f;stereoAccum+=std::abs(l-r);++stereoCount;if(pos==ring.size()){analyze(ring,stereoCount?std::min(stereoAccum/stereoCount*4,1.f):0);pos=0;stereoAccum=0;stereoCount=0;}}
            captureClient->ReleaseBuffer(frames);
          } client->Stop();freeFormat(); }
    done: if(!captureStarted&&running_){std::scoped_lock lock(mutex_);name_=L"Audio capture unavailable - press D";}if(mmcss)AvRevertMmThreadCharacteristics(mmcss); CoUninitialize();
    }
    std::atomic<bool> running_{false}; std::thread worker_; size_t deviceIndex_{};
    mutable std::mutex mutex_;AudioFrame frame_{};std::wstring name_=L"Starting audio capture...";std::array<float,4>ceilings_{.0001f,.0001f,.0001f,.0001f};float previousBass_{},previousMid_{},previousRms_{},previousPeak_{},beatEnvelope_{},fluxEnvelope_{},beatInterval_{},tempoConfidence_{};int beatCooldown_{};Clock::time_point lastBeatTime_{};
};

static std::filesystem::path SettingsPath(){
    wchar_t local[MAX_PATH]{};DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH);std::filesystem::path directory;
    if(length>0&&length<MAX_PATH)directory=std::filesystem::path(local)/L"RareStage";else{wchar_t module[MAX_PATH]{};GetModuleFileNameW(nullptr,module,MAX_PATH);directory=std::filesystem::path(module).parent_path();}
    std::error_code error;std::filesystem::create_directories(directory,error);return directory/L"settings.ini";
}

struct ShowSettings {
    int profile=1,palette=0,monitor=0,automatic=1,quality=2,vsync=1,timeline=0,spout=1,transitionStyle=0;
    float sensitivity=1,bassGain=1,midGain=1,highGain=1,beatGain=1,logoScale=1,sceneSeconds=72,transitionSeconds=1.4f;
    static float readFloat(const std::wstring&path,const wchar_t*key,float fallback){wchar_t text[32]{};wchar_t fallbackText[32]{};std::swprintf(fallbackText,std::size(fallbackText),L"%.2f",fallback);GetPrivateProfileStringW(L"Show",key,fallbackText,text,(DWORD)std::size(text),path.c_str());return std::wcstof(text,nullptr);}
    static float readFloat(const std::wstring&path,const wchar_t*section,const wchar_t*key,float fallback){wchar_t text[32]{};wchar_t fallbackText[32]{};std::swprintf(fallbackText,std::size(fallbackText),L"%.2f",fallback);GetPrivateProfileStringW(section,key,fallbackText,text,(DWORD)std::size(text),path.c_str());return std::wcstof(text,nullptr);}
    void load(const wchar_t*section=L"Show"){auto path=SettingsPath().wstring();profile=std::clamp(int(GetPrivateProfileIntW(section,L"Profile",profile,path.c_str())),0,3);palette=std::clamp(int(GetPrivateProfileIntW(section,L"Palette",palette,path.c_str())),0,3);monitor=std::max(0,int(GetPrivateProfileIntW(section,L"Monitor",monitor,path.c_str())));automatic=GetPrivateProfileIntW(section,L"Automatic",automatic,path.c_str())?1:0;quality=std::clamp(int(GetPrivateProfileIntW(section,L"Quality",quality,path.c_str())),0,3);vsync=GetPrivateProfileIntW(section,L"VSync",vsync,path.c_str())?1:0;timeline=GetPrivateProfileIntW(section,L"Timeline",timeline,path.c_str())?1:0;spout=GetPrivateProfileIntW(section,L"Spout",spout,path.c_str())?1:0;transitionStyle=std::clamp(int(GetPrivateProfileIntW(section,L"TransitionStyle",transitionStyle,path.c_str())),0,3);
        sensitivity=std::clamp(readFloat(path,section,L"Sensitivity",sensitivity),.50f,1.50f);bassGain=std::clamp(readFloat(path,section,L"Bass",bassGain),.50f,1.50f);midGain=std::clamp(readFloat(path,section,L"Mids",midGain),.50f,1.50f);highGain=std::clamp(readFloat(path,section,L"Highs",highGain),.50f,1.50f);beatGain=std::clamp(readFloat(path,section,L"Beat",beatGain),.50f,1.50f);logoScale=std::clamp(readFloat(path,section,L"Logo",logoScale),.75f,1.25f);sceneSeconds=std::clamp(readFloat(path,section,L"SceneSeconds",sceneSeconds),40.f,120.f);transitionSeconds=std::clamp(readFloat(path,section,L"TransitionSeconds",transitionSeconds),.2f,3.f);}
    void save(const wchar_t*section=L"Show")const{auto path=SettingsPath().wstring();auto writeInt=[&](const wchar_t*key,int value){wchar_t text[16]{};std::swprintf(text,std::size(text),L"%d",value);WritePrivateProfileStringW(section,key,text,path.c_str());};auto writeFloat=[&](const wchar_t*key,float value){wchar_t text[32]{};std::swprintf(text,std::size(text),L"%.2f",value);WritePrivateProfileStringW(section,key,text,path.c_str());};
        writeInt(L"Profile",profile);writeInt(L"Palette",palette);writeInt(L"Monitor",monitor);writeInt(L"Automatic",automatic);writeInt(L"Quality",quality);writeInt(L"VSync",vsync);writeInt(L"Timeline",timeline);writeInt(L"Spout",spout);writeInt(L"TransitionStyle",transitionStyle);writeFloat(L"Sensitivity",sensitivity);writeFloat(L"Bass",bassGain);writeFloat(L"Mids",midGain);writeFloat(L"Highs",highGain);writeFloat(L"Beat",beatGain);writeFloat(L"Logo",logoScale);writeFloat(L"SceneSeconds",sceneSeconds);writeFloat(L"TransitionSeconds",transitionSeconds);}
};

static const char* Shader = R"(
cbuffer Frame : register(b0){float2 resolution;float time;float scene;float bass;float mid;float high;float level;float peak;float stereo;float beat;float flux;float previousScene;float transition;float logoScale;float palette;float transitionStyle;float tempoPhase;float tempoConfidence;float audioEnergy;float songSection;float sectionProgress;float dropPulse;float onset;}
Texture2D logoTex:register(t0);Texture2D postScene:register(t1);Texture2D postBloom:register(t2);SamplerState linearSampler:register(s0);
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
O vs(uint id:SV_VertexID){O o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float easeBand(float v){v=saturate(v);return v*v*(3-2*v);}
float2 rotate2(float2 p,float a){float c=cos(a),s=sin(a);return float2(c*p.x-s*p.y,s*p.x+c*p.y);}
float lineGlow(float v,float w){return saturate(w/max(abs(v),.0005));}
float boxRing(float2 p,float2 size,float w){return lineGlow(max(abs(p.x)-size.x,abs(p.y)-size.y),w);}
float fillBox(float2 p,float2 size,float feather){float2 q=abs(p)-size;return 1-smoothstep(0,feather,max(q.x,q.y));}
float softDisc(float2 p,float radius,float feather){return 1-smoothstep(radius-feather,radius+feather,length(p));}
float segmentGlow(float2 p,float2 a,float2 b,float width){float2 pa=p-a,ba=b-a;float h=saturate(dot(pa,ba)/max(dot(ba,ba),.0001));return lineGlow(length(pa-ba*h),width);}
float flowField(float2 p,float phase){float2 q=p+sin(p.yx*float2(3.1,4.2)+phase)*.12;return .5+.5*sin(q.x*4.2+sin(q.y*3.4-phase*.63)+phase);}
float2 morphCell(float2 p,float phase,float scale){float2 g=p*scale;float2 id=floor(g);float2 q=frac(g)-.5;float seed=hash(id),local=phase+seed*6.283185;float2 wander=float2(sin(phase*(.46+seed*.45)+seed*11.7),cos(phase*(.38+hash(id+3.4)*.43)+seed*8.9))*(.045+.095*hash(id+6.2));q-=wander;q=rotate2(q,sin(local*.73)*.24);float morph=.5+.5*sin(local*.67);float shape=lerp(length(q),max(abs(q.x),abs(q.y)),morph);float radius=.22+.035*sin(local*.91);float edge=1-smoothstep(.008,.026,abs(shape-radius));float fill=(1-smoothstep(radius-.025,radius+.012,shape))*(.30+.70*hash(id+2.7));return float2(edge,fill);}
float ribbonSet(float2 p,float phase,float motion){float ribbons=0;[unroll]for(int k=0;k<5;k++){float lane=(k-2)*.12;float wave=lane+sin(p.x*(1.55+k*.11)+phase+k*.81)*(.028+motion*.035);ribbons+=lineGlow(p.y-wave,.0024);}return ribbons;}
float starField(float2 p,float scale,float threshold){float2 id=floor(p*scale);float2 q=frac(p*scale)-.5;float star=softDisc(q,.025+.035*hash(id+8.1),.024);return star*step(threshold,hash(id));}
float livingDust(float2 p,float phase,float scale,float threshold){float2 g=p*scale;float2 id=floor(g);float2 q=frac(g)-.5;float seed=hash(id+4.7);float orbit=phase*(.24+seed*.42)+seed*6.283185;float2 offset=float2(cos(orbit),sin(orbit))*(.08+.19*hash(id+9.2));float mote=softDisc(q-offset,.018+.030*hash(id+2.1),.020);return mote*step(threshold,hash(id+7.4));}
float filamentSet(float2 p,float phase,float motion){float strands=0;[unroll]for(int k=0;k<4;k++){float lane=(k-1.5)*.19;float path=lane+sin(p.x*(1.15+k*.18)+phase+k*1.37)*(.08+motion*.035)+sin(p.x*3.1-phase*.43+k)*.018;strands+=lineGlow(p.y-path,.0024);}return strands;}
float transitionOldMask(float2 uv,float amount,float style,float seed){
 float progress=easeBand(saturate(amount));float2 q=uv-.5;float coordinate;
 if(style<.5){float2 cell=floor(uv*float2(14,8));coordinate=hash(cell+seed)*.74+length(q)*.22;return 1-smoothstep(coordinate-.08,coordinate+.08,progress*1.18);}
 if(style<1.5){coordinate=uv.y+sin(uv.x*7+seed)*.045;return 1-smoothstep(coordinate-.055,coordinate+.055,progress*1.18);}
 if(style<2.5){coordinate=uv.x*.64+uv.y*.36+sin(uv.y*9+seed)*.035;return 1-smoothstep(coordinate-.055,coordinate+.055,progress*1.22);}
 if(style<3.5){coordinate=max(abs(q.x)*1.08,abs(q.y)*.78)*1.48;return 1-smoothstep(coordinate-.06,coordinate+.06,progress*1.22);}
 if(style<4.5){coordinate=uv.y+sin(uv.x*13+progress*5)*.075;return 1-smoothstep(coordinate-.06,coordinate+.06,progress*1.20);}
 coordinate=length(q)*1.30+hash(float2(floor(atan2(q.y,q.x)*9),seed))*.10;return 1-smoothstep(coordinate-.065,coordinate+.065,progress*1.15);
}
void paletteKey(float selection,out float3 primary,out float3 accent,out float3 spectral){primary=float3(.00,.74,1.10);accent=float3(.58,1.04,.05);spectral=float3(.55,.07,.98);if(selection>.5&&selection<1.5){primary=float3(.10,.44,1.16);accent=float3(.02,.98,.72);spectral=float3(.66,.08,1.08);}else if(selection>1.5&&selection<2.5){primary=float3(1.04,.16,.60);accent=float3(.18,.84,1.06);spectral=float3(.56,.10,1.04);}else if(selection>2.5){primary=float3(1.12,.30,.03);accent=float3(1.04,.84,.08);spectral=float3(.74,.04,.24);}}
void rarePalette(float selection,out float3 primary,out float3 accent,out float3 spectral){float wrapped=frac(selection*.25)*4;float base=floor(wrapped),blend=easeBand(frac(wrapped));float3 p0,a0,s0,p1,a1,s1;paletteKey(base,p0,a0,s0);paletteKey(frac((base+1)*.25)*4,p1,a1,s1);primary=lerp(p0,p1,blend);accent=lerp(a0,a1,blend);spectral=lerp(s0,s1,blend);}
float4 sampleRareLogo(float2 samplePoint,float2 offset,float size,float angle){float2 q=rotate2(samplePoint-offset,-angle);float2 uv=q/size+.5;float inside=step(0,uv.x)*step(uv.x,1)*step(0,uv.y)*step(uv.y,1);float4 texel=logoTex.Sample(linearSampler,saturate(uv));return float4(texel.rgb,inside*saturate(texel.a*1.08));}
void compositeLogo(inout float3 color,float4 mark,float opacity,float light){float alpha=saturate(mark.a*opacity);color=lerp(color,mark.rgb,alpha*.72);color+=mark.rgb*alpha*light;}
float4 ps(O i):SV_TARGET{
 float aspect=resolution.x/resolution.y;float2 p=(i.uv-.5)*float2(aspect,1);float music=tempoPhase*6.283185;float t=time+sin(music)*tempoConfidence*.012;
 float oldMask=transitionOldMask(i.uv,transition,transitionStyle,scene+previousScene*1.71);float s=lerp(scene,previousScene,step(.5,oldMask));float seam=4*oldMask*(1-oldMask);
 float low=easeBand(bass),mids=easeBand(mid),treble=easeBand(high),hit=easeBand(beat),change=easeBand(flux);float openState=1-saturate(abs(songSection)),buildState=1-saturate(abs(songSection-1)),dropState=1-saturate(abs(songSection-2)),breakState=1-saturate(abs(songSection-3)),driveState=1-saturate(abs(songSection-4));
 float idle=1-smoothstep(.015,.11,audioEnergy);float impact=saturate(low*.32+hit*.22+dropPulse*.22+onset*.28+audioEnergy*.10);float motion=saturate(.18+mids*.44+audioEnergy*.22+buildState*.12+onset*.05);float detail=saturate(.10+treble*.56+change*.18+onset*.10);float actEnergy=saturate(.10+openState*.14+buildState*.56+dropState+breakState*.24+driveState*.78);float phase=t*(.22+motion*.18)+music*.024*tempoConfidence;
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);float vignette=1-smoothstep(.28,1.12,length(p));float field=flowField(p*.78,t*.085+music*.010);float breathe=.5+.5*sin(t*.24+music*.13*tempoConfidence);float3 c=float3(.0015,.004,.012)+lerp(spectral,primary,field)*(.009+.011*audioEnergy+.004*idle)*vignette;
 float dustNear=livingDust(p+float2(t*.007,-t*.004),t*.52,18,.76-detail*.035);float dustFar=livingDust(rotate2(p,-.08)-float2(t*.003,t*.002),-t*.31,11,.80-detail*.025);float2 frontP=rotate2(p,.16);float lightFront=exp(-abs(frontP.y-sin(frontP.x*1.35+t*.075)*.10-sin(t*.032)*.29)*6.5);c+=(dustNear*primary+dustFar*spectral)*(.010+.012*breathe+.010*detail);c+=lightFront*lerp(primary,spectral,.42)*(.005+.007*actEnergy+.004*audioEnergy);
 if(s<.5){
  float2 drift=float2(sin(t*.145),cos(t*.119))*(.050+.012*idle);float2 nearCell=morphCell(p+drift,phase*1.45,3.15);float2 farCell=morphCell(rotate2(p,-.12)+drift*.55,-phase*.92,1.78);float merge=.5+.5*sin(p.x*1.7-p.y*1.2+t*.22);float ribbons=ribbonSet(rotate2(p,.08),phase*.72,motion);float focus=softDisc(p-float2(sin(t*.11)*.34,cos(t*.083)*.16),.38+impact*.018,.28);
  float idleCurrent=exp(-abs(rotate2(p,.18).y-sin(p.x*1.3+t*.12)*.14-sin(t*.071)*.24)*6);c+=idleCurrent*lerp(primary,spectral,.40)*(.007+.012*idle+.010*motion);c+=focus*lerp(spectral,primary,.56)*(.018+.014*actEnergy);c+=nearCell.x*lerp(spectral,primary,merge)*(.045+.040*detail+.025*dropState);c+=farCell.x*accent*(.016+.020*motion);c+=(nearCell.y*(.014+.012*idle)+farCell.y*.009)*lerp(primary,spectral,.48);c+=ribbons*accent*(.010+.022*buildState+.016*driveState);
  float reveal=saturate(buildState*.42+dropState*.82+driveState*.48-sectionProgress*breakState);float4 mark=sampleRareLogo(p,float2(sin(t*.045)*.035,cos(t*.052)*.025),(.62+impact*.018)*logoScale,sin(t*.035)*.018);float scan=smoothstep(-.55,.55,p.y+sectionProgress*1.1);float markAlpha=reveal*lerp(.34,.68,dropState)*lerp(.45,1,scan);c*=1-mark.a*markAlpha*.24;c+=mark.rgb*mark.a*markAlpha*.62;
 }
 else if(s<1.5){
  float2 flowP=p+float2(sin(t*.041)*.08,cos(t*.036)*.055);float tissue=flowField(flowP*1.18,phase*.66);float contours=1-smoothstep(.030,.080,abs(frac(tissue*5.2-phase*.055)-.5));float strandA=filamentSet(rotate2(p,.10),phase*.94,motion);float strandB=filamentSet(rotate2(p,-.68),-phase*.72,motion);
  float2 organA=float2(-.34+sin(t*.047)*.11,-.10+cos(t*.039)*.08);float2 organB=float2(.30+cos(t*.038)*.13,.12+sin(t*.052)*.075);float2 organC=float2(sin(t*.031)*.18,-.28+sin(t*.044)*.055);float dA=length(p-organA)-(.19+impact*.012),dB=length(p-organB)-(.23+impact*.010),dC=length(p-organC)-(.14+impact*.008);float membraneDistance=min(dA,min(dB,dC));float membrane=1-smoothstep(-.025,.070,membraneDistance);float membraneEdge=lineGlow(membraneDistance,.0045);
  c+=lerp(spectral,primary,tissue)*(.013+.022*motion);c+=contours*lerp(primary,accent,.28)*(.020+.022*detail+.018*driveState);c+=strandA*primary*(.030+.030*buildState+.018*driveState)+strandB*spectral*(.018+.028*dropState);c=lerp(c,lerp(float3(.003,.012,.020),spectral*.050,tissue),membrane*.34);c+=membraneEdge*lerp(accent,primary,.38)*(.055+.045*actEnergy);
  float lifeNodes=livingDust(p+float2(t*.011,-t*.007),phase*1.3,9,.62-detail*.045);float pulseNodes=lifeNodes*(.45+.55*sin(phase*2.1+p.x*7-p.y*4));c+=pulseNodes*accent*(.045+.035*actEnergy);
  float2 carrier=float2(sin(t*.052)*.30,cos(t*.043)*.13);float tendrils=segmentGlow(p,organA,carrier,.0022)+segmentGlow(p,organB,carrier,.0022)+segmentGlow(p,organC,carrier,.0018);c+=tendrils*lerp(primary,accent,.46)*(.030+.030*buildState+.025*dropState);
  float4 livingMark=sampleRareLogo(p,carrier,(.30+impact*.010)*logoScale,sin(t*.036)*.055);float logoLife=saturate(openState*.42+buildState*.70+dropState*.88+breakState*.24+driveState*.76);compositeLogo(c,livingMark,logoLife,.038);float wake=lineGlow(rotate2(p-carrier,-.15).y-sin((p.x-carrier.x)*2.2-phase)*.055,.002);c+=wake*primary*(.012+.030*motion);
 }
 else if(s<2.5){
  float skyFlow=flowField(p*float2(.65,1.3)+float2(t*.012,0),phase*.35);float stars=starField(p+float2(t*.003,0),32, .965-detail*.012);float aurora=ribbonSet(float2(p.x,p.y+.26),phase*.32,motion);float moon=softDisc(p-float2(.35+sin(t*.025)*.035,-.24+cos(t*.021)*.018),.12+impact*.004,.025);
  c+=lerp(spectral,primary,skyFlow)*(.010+.016*breakState)+stars*lerp(primary,accent,hash(floor(p*32)))*(.030+.025*detail)+aurora*lerp(primary,accent,.38)*(.010+.018*buildState);
  c+=moon*lerp(accent,primary,.34)*(.070+.025*actEnergy);float4 crest=sampleRareLogo(p,float2(.35+sin(t*.025)*.035,-.24+cos(t*.021)*.018),.155*logoScale,0);compositeLogo(c,crest,.56+.18*dropState,.025);
  float hillA=.15+sin(p.x*1.55+t*.028)*.045+sin(p.x*3.2-t*.018)*.018;float hillB=.25+sin(p.x*1.12-t*.021+1.7)*.06;float landA=smoothstep(hillA-.015,hillA+.02,p.y),landB=smoothstep(hillB-.015,hillB+.02,p.y);c=lerp(c,lerp(float3(.003,.020,.021),primary*.035,field),landA*.86);c=lerp(c,lerp(float3(.002,.010,.017),spectral*.030,field),landB*.88);
  float garden=ribbonSet(float2(p.x,(p.y-hillA)*1.7),-phase*.52,motion)*landA;float spores=livingDust(p+float2(t*.008,-t*.010)*( .3+motion),phase*.8,22,.72-detail*.035)*landA;float fireflies=livingDust(p-float2(t*.004,t*.007),-phase*.45,12,.78-detail*.025);c+=garden*lerp(accent,primary,.45)*(.018+.025*motion)+spores*accent*(.026+.035*buildState+.025*dropState)+fireflies*primary*.022;c+=lineGlow(p.y-hillA,.0025)*primary*.055;
 }
 else if(s<3.5){
  float cue=hash(float2(floor(tempoPhase/8),3.7));float2 wallScale=float2(6.2,8.0);float2 wallGrid=p*wallScale;float2 panelId=floor(wallGrid);float2 panelQ=frac(wallGrid)-.5;float2 panelCenter=(panelId+.5)/wallScale;float panelSeed=hash(panelId+2.4);
  float column=.5+.5*sin(panelId.x*1.47-phase*1.35);float diagonal=.5+.5*sin((panelId.x+panelId.y*(cue>.5?1:-1))*1.12-phase*2.7);float radial=.5+.5*cos(length(panelCenter)*17-phase*3.4);float checker=.5+.5*sin((panelId.x+panelId.y)*3.141592+phase*2.1);float sparse=step(.72-.18*detail,panelSeed)*(.45+.55*sin(panelSeed*8+phase));
  float panelSignal=saturate(openState*(.18+.36*column)+buildState*(.20+.72*diagonal)+dropState*(.18+.82*radial)+breakState*(.08+.42*sparse)+driveState*(.18+.72*checker));panelSignal=easeBand(panelSignal);float2 physicalShift=float2(sign(panelCenter.x),sign(panelCenter.y))*(panelSignal-.5)*(.018+.034*impact);panelQ-=physicalShift;panelQ=rotate2(panelQ,(panelSeed-.5)*(.035+.10*motion));
  float2 panelSize=float2(.425-.040*panelSignal,.390-.025*panelSignal);float panelFace=fillBox(panelQ,panelSize,.035);float panelEdge=boxRing(panelQ,panelSize,.006);float3 panelColor=lerp(spectral,primary,.5+.5*sin(panelId.x*.42-panelId.y*.31+phase*.72));panelColor=lerp(panelColor,accent,dropState*radial*.42+driveState*checker*.24);
  float4 wallMark=sampleRareLogo(panelCenter,float2(0,0),1.02*logoScale,0);float markCue=saturate(buildState*.32+dropState*.78+driveState*.48);panelColor=lerp(panelColor,wallMark.rgb,wallMark.a*markCue*.78);float panelLight=.025+.16*panelSignal+.13*low*column+.14*hit*radial+wallMark.a*markCue*(.12+.16*impact);c=lerp(c,float3(.002,.004,.011)+panelColor*panelLight,panelFace*.88);c+=panelEdge*panelColor*(.018+.070*panelSignal+.045*detail);
  float chase=exp(-abs(frac((panelId.x*(cue>.5?1:-1)+panelId.y)*.115-tempoPhase*.125)-.5)*18)*panelFace;float dropPunch=exp(-length(panelCenter)*(8+impact*2))*dropState;c+=chase*lerp(primary,accent,.42)*(.020+.080*buildState+.060*driveState);c+=dropPunch*panelColor*(.045+.11*hit);
  float sideWash=exp(-abs(abs(p.x)-aspect*.46)*15);float ceilingWash=exp(-abs(p.y+.47)*22);c+=(sideWash+ceilingWash)*lerp(primary,accent,.28)*(.010+.040*actEnergy+.025*low);float panelDust=livingDust(p+float2(t*.006,-t*.004),phase,14,.78-detail*.025);c+=panelDust*accent*(.012+.025*treble);
 }
 else if(s<4.5){
  float horizon=.04+sin(p.x*1.15+t*.027)*.012;float sea=smoothstep(horizon-.008,horizon+.012,p.y);float sky=flowField(p*float2(.75,1.4)+float2(t*.010,0),phase*.28);c+=lerp(spectral,primary,sky)*(.012+.014*breakState)*(1-sea);
  float wavePhase=phase*(.55+.25*driveState);float waves=ribbonSet(float2(p.x,(p.y-horizon)*1.35),wavePhase,motion)*sea;float under=flowField(p*float2(1.0,2.2)+float2(t*.018,0),phase*.42);c+=sea*lerp(float3(.001,.012,.021),primary*.038,under)+waves*lerp(primary,accent,.30)*(.026+.025*motion+.020*hit);
  float currentA=lineGlow(p.y-horizon-.10-sin(p.x*1.8+phase)*.028,.0025);float currentB=lineGlow(p.y-horizon-.25-sin(p.x*1.25-phase*.7)*.045,.002);c+=(currentA+currentB)*sea*spectral*(.024+.024*detail);c+=lineGlow(p.y-horizon,.0025)*accent*.070;
  float2 crestPos=float2(-.31+sin(t*.032)*.045,-.18+cos(t*.026)*.018);float4 oceanMark=sampleRareLogo(p,crestPos,(.205+impact*.006)*logoScale,0);float visibility=saturate(openState*.28+buildState*.70+dropState*.88+driveState*.60+breakState*.12);compositeLogo(c,oceanMark,visibility,.035);float reflection=exp(-abs(p.x-crestPos.x)*18)*sea*(.5+.5*sin((p.y-horizon)*70-phase*2));c+=reflection*primary*(.012+.030*visibility);
  float rain=pow(saturate(.5+.5*cos((p.x*1.7+p.y-phase*.35)*17)),46);float foam=livingDust(float2(p.x,(p.y-horizon)*.65)+float2(t*.012,-t*.006),phase,18,.70-detail*.025)*sea;c+=rain*dropState*sea*accent*.025+foam*lerp(primary,accent,.55)*(.018+.026*motion);
 }
 else if(s<5.5){
  float assembly=saturate(buildState*(.24+sectionProgress*.52)+dropState*.88+driveState*.38);float disperse=saturate(openState*.42+breakState*.90);float2 dvdCycle=frac(float2(t/17+.14,t/23+.63));float2 dvdTriangle=1-abs(dvdCycle*2-1)*2;float2 dvdBounds=float2(max(.30,aspect*.5-.18*logoScale),.32);float2 logoPos=dvdTriangle*dvdBounds;float2 pixelDrift=float2(t*(.018+.012*motion),sin(t*.041)*.055);
  float2 currentP=rotate2(p,.14+sin(t*.019)*.025);float currentA=exp(-abs(currentP.y-sin(currentP.x*1.55+phase*.62)*.13-sin(t*.031)*.23)*7);float currentB=exp(-abs(currentP.y+sin(currentP.x*1.18-phase*.47)*.10+cos(t*.026)*.25)*9);c+=(currentA*lerp(primary,spectral,.28)+currentB*lerp(spectral,accent,.26))*(.012+.020*motion+.016*buildState);
  float gridScale=18;float2 grid=(p+pixelDrift)*gridScale;float2 pixelId=floor(grid);float2 pixelQ=frac(grid)-.5;float pixelSeed=hash(pixelId);float pixelDepth=.5+.5*sin(t*(.24+pixelSeed*.34)+pixelSeed*9.6);float2 pixelShift=float2(sin(t*(.31+pixelSeed*.27)+pixelSeed*13.1),cos(t*(.25+hash(pixelId+4.1)*.30)+pixelSeed*7.7))*(.07+.19*disperse+.05*pixelDepth);pixelQ-=pixelShift;float pixelSize=.10+.10*pixelDepth+.035*impact;float boxPixel=fillBox(pixelQ,float2(pixelSize,pixelSize*(.72+.28*pixelSeed)),.045);float discPixel=softDisc(pixelQ,pixelSize*.86,.045);float shapePhase=.5+.5*sin(t*(.18+pixelSeed*.12)+pixelSeed*9);float pixelShape=lerp(boxPixel,discPixel,shapePhase);
  float2 pixelCenter=(pixelId+.5)/gridScale-pixelDrift;float ribbonFormation=exp(-abs(rotate2(pixelCenter,.18).y-sin(pixelCenter.x*1.6+phase)*.15)*7);float diamondFormation=exp(-abs(abs(pixelCenter.x)*.56+abs(pixelCenter.y)-(.28+.07*sin(phase*.8)))*11);float edgeFormation=smoothstep(.24,.72,abs(pixelCenter.x));float formation=saturate(openState*(.10+.52*ribbonFormation)+buildState*(.12+.78*diamondFormation)+dropState*(.18+.82*diamondFormation)+breakState*(.08+.58*edgeFormation)+driveState*(.12+.68*max(ribbonFormation,edgeFormation)));
  float4 assembledCell=sampleRareLogo(pixelCenter,logoPos,.58*logoScale,sin(t*.018)*.025);float logoPixel=assembledCell.a*assembly;float aliveGate=step(.26,pixelSeed)*(.18+.82*formation)*(.38+.62*pixelDepth);float pixelWave=.5+.5*sin(pixelId.x*.47+pixelId.y*.31-phase*1.4+pixelSeed*5.2);float3 pixelColor=lerp(spectral,primary,pixelWave);pixelColor=lerp(pixelColor,assembledCell.rgb,logoPixel*.82);c+=pixelShape*aliveGate*pixelColor*(.022+.030*motion+.040*logoPixel);
  float farScale=9;float2 farGrid=(rotate2(p,-.10)-pixelDrift*.34)*farScale;float2 farId=floor(farGrid);float2 farQ=frac(farGrid)-.5;float farSeed=hash(farId+5.6);float2 farCenter=(farId+.5)/farScale;float2 farShift=float2(cos(t*(.13+farSeed*.19)+farSeed*8),sin(t*(.17+farSeed*.16)+farSeed*12))*(.08+.12*disperse);farQ-=farShift;float farSize=.065+.070*farSeed;float farPixel=fillBox(farQ,float2(farSize,farSize),.050)*step(.40,farSeed);float farFormation=exp(-abs(rotate2(farCenter,-.22).y+sin(farCenter.x*1.3-phase*.55)*.18)*6);float farWave=.5+.5*sin(farId.x*.6-farId.y*.4+phase*.65);c+=farPixel*farFormation*lerp(primary,spectral,farWave)*(.010+.018*breathe);
  float2 streamP=rotate2(p,.16);float pixelStream=lineGlow(streamP.y-sin(streamP.x*1.4+phase*.72)*(.13+.04*motion)-sin(t*.027)*.26,.0025);float streamPixels=step(.58,hash(floor((streamP+float2(t*.024,0))*float2(22,15))))*pixelStream;c+=streamPixels*accent*(.045+.050*dropState);
  float edgePulseX=exp(-abs(sin(dvdCycle.x*6.283185))*12);float edgePulseY=exp(-abs(sin(dvdCycle.y*6.283185))*12);float cornerPulse=edgePulseX*edgePulseY;float bouncePulse=saturate(max(edgePulseX,edgePulseY)+cornerPulse*2.5);float bounceIndex=floor(t/8.5)+floor(t/11.5);float bounceHue=hash(float2(bounceIndex,6.2));float3 bounceColor=lerp(primary,accent,bounceHue);float reactiveLight=.18+.42*audioEnergy+.24*low+.20*hit;
  float wallX=exp(-abs(p.x-sign(logoPos.x)*(aspect*.5-.012))*18)*edgePulseX;float wallY=exp(-abs(p.y-sign(logoPos.y)*.488)*20)*edgePulseY;float localLight=exp(-length(p-logoPos)*(5.5-1.4*impact));c+=(wallX+wallY)*bounceColor*(.035+.11*reactiveLight);c+=localLight*bounceColor*bouncePulse*(.018+.085*reactiveLight)+cornerPulse*localLight*accent*(.12+.16*reactiveLight);
  float2 dvdVelocity=float2(sign(cos(dvdCycle.x*6.283185)),sign(cos(dvdCycle.y*6.283185)));float lightTrail=segmentGlow(p,logoPos,logoPos-dvdVelocity*float2(.28,.16),.0025);c+=lightTrail*bounceColor*(.014+.035*motion+.055*bouncePulse*reactiveLight);float4 dvdMark=sampleRareLogo(p,logoPos,(.285+impact*.010)*logoScale,sin(t*.19)*.022);compositeLogo(c,dvdMark,.76+.16*dropState,.025+.070*bouncePulse*reactiveLight);
  float dissolveEdge=boxRing(p-logoPos,float2(.25,.20),.0022);c+=dissolveEdge*primary*assembly*(.016+.018*detail);float travelers=livingDust(p+float2(t*.016,-t*.009),phase*1.2,16,.72-detail*.030);c+=travelers*lerp(primary,accent,.34)*(.014+.022*driveState+.016*disperse);
 }
 else if(s<6.5){
  float2 prismA=rotate2(p,-.46+sin(t*.021)*.035);float2 prismB=rotate2(p,.52+cos(t*.018)*.030);float stripeA=frac(prismA.x*4.6+phase*.16);float stripeB=frac(prismB.x*5.4-phase*.13);float bandA=1-smoothstep(.25,.42,abs(stripeA-.5));float bandB=1-smoothstep(.29,.44,abs(stripeB-.5));float laneA=floor(prismA.x*4.6+phase*.16);float laneB=floor(prismB.x*5.4-phase*.13);float pulseA=.18+.82*(.5+.5*sin(laneA*1.37-phase*2.2));float pulseB=.18+.82*(.5+.5*sin(laneB*.91+phase*1.8));
  float shutterA=bandA*smoothstep(-.58,.10+sin(laneA*1.7+phase)*.30,prismA.y);float shutterB=bandB*(1-smoothstep(-.12+cos(laneB+phase)*.26,.58,prismB.y));float prismEnergy=saturate(openState*.25+buildState*.72+dropState+breakState*.18+driveState*.82);c=lerp(c,lerp(float3(.002,.004,.012),primary*.16,pulseA),shutterA*(.28+.28*prismEnergy));c=lerp(c,lerp(float3(.003,.002,.011),spectral*.15,pulseB),shutterB*(.18+.24*prismEnergy));
  float seamA=lineGlow(abs(stripeA-.5)-.30,.003);float seamB=lineGlow(abs(stripeB-.5)-.33,.0025);c+=(seamA*primary+seamB*accent)*(.025+.050*detail+.045*dropState);float sliceId=floor((p.y+.5)*14);float2 slicedPoint=p+float2(sin(sliceId*1.73+phase*1.8)*(.006+.026*change),0);float4 prismMark=sampleRareLogo(slicedPoint,float2(0,0),(.86+impact*.015)*logoScale,0);float prismReveal=saturate(buildState*.22+dropState*.62+driveState*.34);c=lerp(c,prismMark.rgb,prismMark.a*prismReveal*.28);c+=prismMark.rgb*prismMark.a*prismReveal*(.018+.040*hit);
  float shutterHit=exp(-abs(frac(tempoPhase*.25)-.5)*18)*dropState;c+=shutterHit*(bandA+bandB)*lerp(primary,accent,.5)*(.025+.060*impact);
 }
 else if(s<7.5){
  float weavePresence=saturate(openState*.38+buildState*.82+dropState+breakState*.22+driveState*.88);float2 weaveLogoPos=float2(sin(t*.034)*.38,sin(t*.027+1.2)*.12);float4 weaveMark=sampleRareLogo(p,weaveLogoPos,(.34+impact*.008)*logoScale,sin(t*.028)*.04);float weaveLogo=saturate(buildState*.22+dropState*.58+driveState*.34);c=lerp(c,weaveMark.rgb,weaveMark.a*weaveLogo*.42);
  [unroll]for(int k=0;k<7;k++){float lane=(k-3)*.115;float path=lane+sin(p.x*(1.20+k*.10)+phase*(.72+(k%2)*.18)+k*.83)*(.055+.035*motion);float distance=abs(p.y-path);float ribbon=1-smoothstep(.010+.006*impact,.032+.010*impact,distance);float halo=exp(-distance*(18+detail*8));float weaveDepth=.5+.5*sin(k*1.14+phase*.55);float3 ribbonColor=lerp(primary,spectral,weaveDepth);ribbonColor=lerp(ribbonColor,accent,dropState*(.18+.18*weaveDepth));c+=ribbonColor*(ribbon*(.030+.038*weavePresence)+halo*.004);}
  float2 crossP=rotate2(p,1.06);[unroll]for(int j=0;j<4;j++){float lane=(j-1.5)*.19;float path=lane+sin(crossP.x*(1.05+j*.16)-phase*.68+j*1.3)*(.040+.025*motion);float thread=lineGlow(crossP.y-path,.0022);c+=thread*lerp(accent,primary,j/3.0)*(.020+.032*buildState+.028*driveState);}
  float junctions=livingDust(p+float2(t*.010,-t*.006),phase*1.25,12,.70-detail*.030);c+=junctions*accent*(.022+.045*treble+.030*hit);float carrierWake=lineGlow(p.y-weaveLogoPos.y-sin((p.x-weaveLogoPos.x)*1.8-phase)*.07,.002);c+=carrierWake*primary*weaveLogo*(.018+.030*motion);
 }
 else{
  float2 eclipsePos=float2(sin(t*.024)*.31,cos(t*.019)*.12);float2 eclipseP=p-eclipsePos;float eclipseRadius=.27+.035*sin(t*.031)+impact*.012;float body=softDisc(eclipseP,eclipseRadius,.018);float2 cutOffset=float2(.10+.05*sin(t*.017),-.025+.035*cos(t*.023));float cut=softDisc(eclipseP-cutOffset,eclipseRadius*.94,.022);float crescent=saturate(body-cut);c=lerp(c,float3(.001,.002,.008)+spectral*.012,body*.82);c+=crescent*lerp(primary,accent,.42)*(.18+.12*actEnergy+.08*impact);
  float eclipseAngle=atan2(eclipseP.y,eclipseP.x);float eclipseDistance=length(eclipseP);float fan=pow(saturate(.5+.5*cos(eclipseAngle*14-phase*1.8)),34)*smoothstep(eclipseRadius*.82,eclipseRadius*1.08,eclipseDistance)*(1-smoothstep(.82,1.25,eclipseDistance));float sweepMask=.35+.65*smoothstep(-.4,.6,sin(eclipseAngle*2.0+phase*.36));c+=fan*sweepMask*lerp(primary,spectral,.36)*(.022+.055*buildState+.090*dropState+.045*driveState);
  float arc=lineGlow(eclipseDistance-eclipseRadius*(1.22+.10*sin(phase*.55)),.003)*smoothstep(-.25,.35,sin(eclipseAngle*2.5-phase*.42));c+=arc*accent*(.030+.040*detail+.035*breakState);float horizonBeam=lineGlow(rotate2(p-eclipsePos,-.12).y-sin(t*.026)*.19,.0022);c+=horizonBeam*lerp(primary,accent,.30)*(.018+.040*buildState+.060*dropState);
  float eclipseLogo=saturate(buildState*.12+dropState*.48+driveState*.26);float4 eclipseMark=sampleRareLogo(p,eclipsePos,(.31+impact*.008)*logoScale,0);compositeLogo(c,eclipseMark,eclipseLogo,.030+.035*hit);float eclipseDust=livingDust(rotate2(p,.08)+float2(t*.004,-t*.005),phase*.8,20,.80-detail*.020);c+=eclipseDust*lerp(primary,spectral,.46)*(.012+.020*treble);
 }
 float2 foreground=rotate2(p,-.24);float livingSweep=lineGlow(foreground.y-sin(foreground.x*1.25+t*.068)*.20-sin(t*.029)*.30,.0017);float edgeTravel=smoothstep(.24,.82,abs(foreground.x));c+=livingSweep*edgeTravel*lerp(primary,spectral,.32)*(.010+.012*motion);c+=seam*lerp(primary,accent,.28)*.045;c+=hit*float3(.003,.007,.011)*(1-smoothstep(.18,1.05,length(p)));return float4(max(c,0),1);
}
float3 brightPass(float3 color){float luminance=dot(color,float3(.2126,.7152,.0722));return color*saturate((luminance-.60)/max(luminance,.001));}
float4 psBloom(O i):SV_TARGET{float2 texel=1/resolution;float3 bloom=brightPass(postScene.Sample(linearSampler,i.uv).rgb)*.22;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+float2(texel.x*2,0)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-float2(texel.x*2,0)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+float2(0,texel.y*2)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-float2(0,texel.y*2)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(4,4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(-4,4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(4,-4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-texel*4).rgb)*.055;return float4(bloom,1);}
float3 filmic(float3 x){float a=2.51,b=.03,c=2.43,d=.59,e=.14;return saturate((x*(a*x+b))/(x*(c*x+d)+e));}
float4 psComposite(O i):SV_TARGET{float2 texel=1/resolution;float3 center=postScene.Sample(linearSampler,i.uv).rgb;float3 neighbors=postScene.Sample(linearSampler,i.uv+float2(texel.x,0)).rgb+postScene.Sample(linearSampler,i.uv-float2(texel.x,0)).rgb+postScene.Sample(linearSampler,i.uv+float2(0,texel.y)).rgb+postScene.Sample(linearSampler,i.uv-float2(0,texel.y)).rgb;float3 sharp=max(center*1.10-neighbors*.025,0);float3 bloom=postBloom.Sample(linearSampler,i.uv).rgb;float2 q=i.uv-.5;float vignette=1-smoothstep(.34,.84,length(q));float3 color=(sharp+bloom*.90)*(.86+.14*vignette);color=filmic(color);color=pow(color,1/2.2);color+=(hash(i.uv*resolution+floor(time*20))-.5)*.0025;return float4(saturate(color),1);}
)";

struct Constants { float resolution[2],time,scene,bass,mid,high,level,peak,stereo,beat,flux,previousScene,transition,logoScale,palette,transitionStyle,tempoPhase,tempoConfidence,audioEnergy,songSection,sectionProgress,dropPulse,onset; };

class App {
public:
    bool init(HINSTANCE instance) {
        settings_.load();automatic_=settings_.automatic!=0;visualPalette_=float(settings_.palette);directorPalette_=settings_.palette;activeTransitionSeconds_=settings_.transitionSeconds;activeTransitionStyle_=settings_.transitionStyle;
        StartupLog("Creating RareStage window class");
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=WndProc;wc.lpszClassName=L"RareStageWindow";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
        hwnd_=CreateWindowExW(0,wc.lpszClassName,L"RareStage",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,720,nullptr,nullptr,instance,this);
        if(!hwnd_){StartupLog("CreateWindowExW failed");return false;}
        StartupLog("Initializing DirectX renderer");
        if(!createGpu()){MessageBoxW(hwnd_,L"RareStage could not initialize DirectX. Update your graphics driver and try again.",L"RareStage startup error",MB_OK|MB_ICONERROR);return false;}
        monitorCount_=(int)monitorRects().size();
        StartupLog("DirectX renderer ready");
        ShowWindow(hwnd_,SW_SHOW);UpdateWindow(hwnd_);StartupLog("Window shown");
        audio_.start();StartupLog("Audio capture thread started");started_=Clock::now();transitionStarted_=-100;scheduleNextWorld(audio_.frame(),0);updateHud(audio_.frame());return true;
    }
    void requestClose(){PostMessageW(hwnd_,WM_CLOSE,0,0);}
    int run(){MSG msg{};while(msg.message!=WM_QUIT){if(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}else render();}audio_.stop();return(int)msg.wParam;}
private:
    static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){App*a=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){a=reinterpret_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)a);}if(a)return a->message(h,m,w,l);return DefWindowProcW(h,m,w,l);}
    LRESULT message(HWND h,UINT m,WPARAM w,LPARAM l){
        if(m==WM_DESTROY){settings_.automatic=automatic_?1:0;settings_.save();shutdownSpout();PostQuitMessage(0);return 0;}
        if(m==WM_SIZE&&w!=SIZE_MINIMIZED){if(swap_)resize(LOWORD(l),HIWORD(l));return 0;}
        if(m==WM_SYSKEYDOWN&&w==VK_RETURN){fullscreen();return 0;}
        if(m==WM_LBUTTONDOWN||m==WM_RBUTTONDOWN){POINT point{static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};handleHudPointer(point,m==WM_RBUTTONDOWN?-1:0);return 0;}
        if(m==WM_MOUSEWHEEL){POINT point{static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};ScreenToClient(h,&point);handleHudPointer(point,GET_WHEEL_DELTA_WPARAM(w)>0?1:-1);return 0;}
        if(m==WM_KEYDOWN){
            if(w>=L'1'&&w<=L'9'){selectWorld(int(w-L'1'));automatic_=false;settings_.automatic=0;settings_.save();}
            else if(w==VK_OEM_4||w==VK_OEM_6){selectWorld((world_+(w==VK_OEM_6?1:WorldCount-1))%WorldCount);automatic_=false;settings_.automatic=0;settings_.save();}
            else if(w==L'0'||w==L'A'){automatic_=!automatic_;settings_.automatic=automatic_?1:0;if(automatic_){chooseDirectorPalette(true);scheduleNextWorld(tuned(audio_.frame()),elapsed());}settings_.save();}
            else if(w==L'D')audio_.cycle();else if(w==L'M')cycleMonitor();else if(w==L'P'){settings_.palette=(settings_.palette+1)%4;directorPalette_=settings_.palette;settings_.save();}
            else if(w==L'T'){settings_.timeline=!settings_.timeline;settings_.save();timelineStep_=0;}
            else if(w==L'V'){settings_.vsync=!settings_.vsync;settings_.save();}
            else if(w==L'S'){settings_.spout=!settings_.spout;settings_.save();}
            else if(w>=VK_F1&&w<=VK_F4){int slot=int(w-VK_F1);if(GetKeyState(VK_SHIFT)&0x8000)savePreset(slot);else loadPreset(slot);}
            else if(w==VK_TAB){hudVisible_=!hudVisible_;}
            else if(w==VK_UP)hudSelection_=(hudSelection_+SettingCount-1)%SettingCount;else if(w==VK_DOWN)hudSelection_=(hudSelection_+1)%SettingCount;
            else if(w==VK_LEFT)adjustSetting(-1);else if(w==VK_RIGHT||w==VK_RETURN)adjustSetting(1);
            else if(w==VK_F11)fullscreen();else if(w==VK_SPACE)frozen_=!frozen_;else if(w==VK_ESCAPE){if(fullscreen_)fullscreen();else DestroyWindow(h);}
            updateHud(audio_.frame());return 0;
        }
        return DefWindowProcW(h,m,w,l);
    }
    static constexpr int WorldCount=9;
    enum SongSection { OpenSection, BuildSection, DropSection, BreakdownSection, DriveSection, SongSectionCount };
    static const wchar_t* worldName(int index){static const wchar_t*names[]={L"Morph Dimension",L"Rare Biosphere",L"Celestial Garden",L"Stagewall",L"Neon Ocean",L"Pixel Current",L"Prism Cascade",L"Kinetic Weave",L"Rare Eclipse"};return names[std::clamp(index,0,WorldCount-1)];}
    static const wchar_t* sectionName(int section){static const wchar_t*names[]={L"OPEN",L"BUILD",L"DROP",L"BREAKDOWN",L"DRIVE"};return names[std::clamp(section,0,SongSectionCount-1)];}
    static const wchar_t* paletteName(int palette){static const wchar_t*names[]={L"RARE CYAN",L"NEBULA",L"SAKURA",L"SOLAR"};return names[std::clamp(palette,0,3)];}
    float elapsed()const{return started_.time_since_epoch().count()?std::chrono::duration<float>(Clock::now()-started_).count():0;}
    void chooseDirectorPalette(bool sectionChange){static constexpr int choices[SongSectionCount][3]={{0,1,2},{2,0,1},{3,0,2},{1,2,0},{0,3,1}};int phase=sectionRevision_+(sectionChange?0:autoSequence_/2);directorPalette_=choices[songSection_][phase%3];}
    void selectWorld(int next,bool directed=false){next=std::clamp(next,0,WorldCount-1);if(next==world_){worldChangeArmed_=false;sectionChangePending_=false;return;}if(directed){static constexpr int authoredStyle[WorldCount]={0,4,1,2,1,0,2,4,5};static constexpr float authoredSeconds[WorldCount]={1.55f,1.75f,1.90f,1.45f,2.05f,1.60f,1.35f,1.85f,2.10f};activeTransitionStyle_=authoredStyle[next];activeTransitionSeconds_=std::clamp(authoredSeconds[next]+(settings_.transitionSeconds-1.4f)*.35f,.9f,2.5f);if(autoSequence_%2==0)chooseDirectorPalette(false);}else{activeTransitionStyle_=settings_.transitionStyle;activeTransitionSeconds_=settings_.transitionSeconds;}previousWorld_=world_;world_=next;transitionStarted_=elapsed();worldStartedAt_=transitionStarted_;worldChangeArmed_=false;sectionChangePending_=false;for(size_t i=recentWorlds_.size()-1;i>0;--i)recentWorlds_[i]=recentWorlds_[i-1];recentWorlds_[0]=next;}
    static float directorEnergy(const AudioFrame&af){return std::clamp(af.level*.28f+af.bass*.28f+af.mid*.10f+af.beat*.14f+af.flux*.08f+af.onset*.12f,0.f,1.f);}
    void enterSongSection(int next,float seconds){songSection_=std::clamp(next,0,SongSectionCount-1);sectionCandidate_=songSection_;sectionCandidateSeconds_=0;sectionStartedAt_=seconds;++sectionRevision_;chooseDirectorPalette(true);if(songSection_==DropSection){dropPulse_=1;lastDropAt_=seconds;}sectionLockUntil_=seconds+(songSection_==DropSection?3.5f:5.5f);float dwell=seconds-worldStartedAt_;bool chapterBoundary=songSection_==DropSection||songSection_==BreakdownSection||songSection_==OpenSection;if(automatic_&&chapterBoundary&&dwell>=40.f&&seconds>=nextWorldAt_-12.f){sectionChangePending_=true;worldChangeArmed_=true;worldChangeReadyAt_=seconds;}}
    void updateSongSection(const AudioFrame&af,float dt,float seconds){float energy=directorEnergy(af);float shortRate=std::clamp(dt*5.5f,0.f,1.f),longRate=std::clamp(dt*.11f,0.f,1.f);sectionShortEnergy_+=(energy-sectionShortEnergy_)*shortRate;sectionLongEnergy_+=(energy-sectionLongEnergy_)*longRate;if(sectionLongEnergy_>.24f)heardActiveMusic_=true;dropPulse_=std::max(dropPulse_*std::exp(-dt*3.3f),std::max(af.beat*.72f,af.onset*.92f));int phrase=int(std::floor(tempoPhase_/8.f));bool phraseBoundary=af.tempoConfidence>.14f&&phrase!=lastDirectorPhrase_;if(phraseBoundary)lastDirectorPhrase_=phrase;float rise=sectionShortEnergy_-sectionLongEnergy_;bool dropArmed=songSection_==BuildSection||songSection_==BreakdownSection||songSection_==OpenSection||rise>.060f;bool fastDrop=dropArmed&&af.onset>.48f&&af.bass>.26f&&(af.flux>.10f||energy>.46f)&&seconds-lastDropAt_>8.f;if(fastDrop&&songSection_!=DropSection){enterSongSection(DropSection,seconds);return;}int proposed=songSection_;bool dropHit=dropArmed&&energy>.48f&&af.bass>.32f&&af.beat>.30f&&af.flux>.10f&&rise>.025f&&seconds-lastDropAt_>8.f;if(songSection_==DropSection&&seconds-sectionStartedAt_>8.f&&energy>.38f)proposed=DriveSection;else if(dropHit)proposed=DropSection;else if(songSection_==DropSection&&seconds-sectionStartedAt_<6.f)proposed=DropSection;else if(energy>.52f&&af.bass>.28f)proposed=DriveSection;else if((rise>.045f&&energy>.28f)||(af.flux>.18f&&af.mid>.24f))proposed=BuildSection;else if(energy<.18f||rise<-.085f)proposed=heardActiveMusic_?BreakdownSection:OpenSection;else if(songSection_==DropSection)proposed=DriveSection;if(proposed!=sectionCandidate_){sectionCandidate_=proposed;sectionCandidateSeconds_=0;}else sectionCandidateSeconds_+=dt;float required=proposed==DropSection?.04f:(phraseBoundary?.32f:.68f);bool unlocked=seconds>=sectionLockUntil_||proposed==DropSection||songSection_==OpenSection;if(proposed!=songSection_&&unlocked&&sectionCandidateSeconds_>=required)enterSongSection(proposed,seconds);}
    int chooseAutoWorld(const AudioFrame&af){
        if(settings_.timeline){static constexpr int orders[4][WorldCount]={{2,8,0,4,7,1,6,5,3},{0,7,2,4,8,1,6,5,3},{3,6,5,1,8,7,0,4,2},{3,5,6,8,7,1,0,4,2}};return orders[presetIndex_][timelineStep_++%WorldCount];}
        static constexpr float targetEnergy[WorldCount]={.42f,.54f,.25f,.68f,.36f,.58f,.72f,.55f,.48f};
        static constexpr float bassAffinity[WorldCount]={.35f,.42f,.18f,.82f,.30f,.52f,.68f,.38f,.55f};
        static constexpr float midAffinity[WorldCount]={.80f,.82f,.55f,.50f,.78f,.78f,.70f,.95f,.45f};
        static constexpr float highAffinity[WorldCount]={.70f,.68f,.68f,.72f,.52f,.92f,.88f,.75f,.66f};
        static constexpr float sectionAffinity[SongSectionCount][WorldCount]={{.72f,.58f,.96f,.35f,.88f,.70f,.44f,.76f,.94f},{.94f,.92f,.58f,.92f,.76f,.98f,.96f,1.f,.82f},{.74f,.84f,.28f,1.f,.68f,.92f,1.f,.86f,.90f},{.62f,.88f,1.f,.42f,.94f,.78f,.36f,.72f,1.f},{.88f,.96f,.48f,.98f,.82f,1.f,.96f,1.f,.72f}};
        float energy=directorEnergy(af);int best=world_;float bestScore=-1000.f;
        ++autoSequence_;for(int candidate=0;candidate<WorldCount;++candidate){float score=1.20f-std::abs(energy-targetEnergy[candidate])*2.15f;score+=bassAffinity[candidate]*af.bass*.34f+midAffinity[candidate]*af.mid*.30f+highAffinity[candidate]*af.high*.24f+sectionAffinity[songSection_][candidate]*.76f;if(candidate==3||candidate==6)score+=af.flux*.24f;if(candidate==2||candidate==4||candidate==8)score+=(1-energy)*.16f;if(candidate==world_)score-=4.f;for(size_t h=0;h<recentWorlds_.size();++h)if(candidate==recentWorlds_[h])score-=1.20f/float(h+1);uint32_t mix=uint32_t((autoSequence_+1)*2654435761u)^uint32_t((candidate+3)*2246822519u);score+=float(mix%1000u)/1000.f*.12f;if(score>bestScore){bestScore=score;best=candidate;}}
        return best;
    }
    float phraseDuration(const AudioFrame&af)const{if(af.bpm>=65.f&&af.bpm<=190.f&&af.tempoConfidence>.16f){float beatSeconds=60.f/af.bpm;int phraseBeats=std::clamp(int(std::round(settings_.sceneSeconds/(beatSeconds*8.f)))*8,8,256);return phraseBeats*beatSeconds;}return settings_.sceneSeconds;}
    void scheduleNextWorld(const AudioFrame&af,float now){nextWorldAt_=now+std::clamp(phraseDuration(af),40.f,120.f);worldChangeReadyAt_=nextWorldAt_;worldChangeArmed_=false;}
    AudioFrame tuned(AudioFrame value)const{static const float profiles[]{.70f,.88f,1.04f,1.20f};float base=profiles[settings_.profile]*settings_.sensitivity;value.bass=std::clamp(value.bass*base*settings_.bassGain,0.f,1.f);value.mid=std::clamp(value.mid*base*settings_.midGain,0.f,1.f);value.high=std::clamp(value.high*base*settings_.highGain,0.f,1.f);value.level=std::clamp(value.level*base,0.f,1.f);value.peak=std::clamp(value.peak*base,0.f,1.f);value.beat=std::clamp(value.beat*base*settings_.beatGain,0.f,1.f);value.flux=std::clamp(value.flux*base,0.f,1.f);value.onset=std::clamp(value.onset*base*settings_.beatGain,0.f,1.f);return value;}
    void adjustSetting(int direction){float d=direction*.05f;bool rebuild=false;switch(hudSelection_){case 0:settings_.profile=(settings_.profile+direction+4)%4;break;case 1:settings_.palette=(settings_.palette+direction+4)%4;directorPalette_=settings_.palette;break;case 2:settings_.sensitivity=std::clamp(settings_.sensitivity+d,.5f,1.5f);break;case 3:settings_.bassGain=std::clamp(settings_.bassGain+d,.5f,1.5f);break;case 4:settings_.midGain=std::clamp(settings_.midGain+d,.5f,1.5f);break;case 5:settings_.highGain=std::clamp(settings_.highGain+d,.5f,1.5f);break;case 6:settings_.beatGain=std::clamp(settings_.beatGain+d,.5f,1.5f);break;case 7:settings_.logoScale=std::clamp(settings_.logoScale+d,.75f,1.25f);break;case 8:settings_.sceneSeconds=std::clamp(settings_.sceneSeconds+direction*4.f,40.f,120.f);scheduleNextWorld(tuned(audio_.frame()),elapsed());break;case 9:settings_.transitionSeconds=std::clamp(settings_.transitionSeconds+direction*.1f,.2f,3.f);break;case 10:settings_.transitionStyle=(settings_.transitionStyle+direction+4)%4;break;case 11:settings_.quality=(settings_.quality+direction+4)%4;rebuild=true;break;case 12:settings_.vsync=!settings_.vsync;break;case 13:settings_.timeline=!settings_.timeline;timelineStep_=0;break;case 14:settings_.spout=!settings_.spout;break;}settings_.save();if(rebuild&&swap_)resize(width_,height_);}
    static const wchar_t* presetName(int index){static const wchar_t*names[]={L"CLEAN OPEN",L"MELODIC BLOOM",L"RARE DROP",L"GLITCH FINALE"};return names[std::clamp(index,0,3)];}
    static std::wstring presetSection(int index){return L"Preset"+std::to_wstring(index+1);}
    void savePreset(int index){presetIndex_=std::clamp(index,0,3);auto section=presetSection(presetIndex_);settings_.save(section.c_str());auto path=SettingsPath().wstring();WritePrivateProfileStringW(section.c_str(),L"Valid",L"1",path.c_str());wchar_t value[8]{};std::swprintf(value,std::size(value),L"%d",world_);WritePrivateProfileStringW(section.c_str(),L"World",value,path.c_str());}
    void loadPreset(int index){presetIndex_=std::clamp(index,0,3);auto section=presetSection(presetIndex_);auto path=SettingsPath().wstring();if(GetPrivateProfileIntW(section.c_str(),L"Valid",0,path.c_str()))settings_.load(section.c_str());else{settings_=ShowSettings{};if(index==0){settings_.profile=0;settings_.sceneSeconds=88;settings_.transitionSeconds=1.8f;settings_.transitionStyle=1;}else if(index==1){settings_.profile=1;settings_.palette=2;settings_.sceneSeconds=72;settings_.transitionStyle=2;}else if(index==2){settings_.profile=2;settings_.sceneSeconds=56;settings_.transitionSeconds=1.2f;settings_.transitionStyle=3;}else{settings_.profile=3;settings_.palette=1;settings_.sceneSeconds=48;settings_.transitionSeconds=.9f;settings_.transitionStyle=2;settings_.timeline=1;}settings_.monitor=std::max(0,settings_.monitor);}
        automatic_=settings_.automatic!=0;directorPalette_=settings_.palette;if(automatic_&&!settings_.timeline)chooseDirectorPalette(true);int legacyWorld=GetPrivateProfileIntW(section.c_str(),L"Scene",index==0?2:index==1?0:index==2?1:3,path.c_str())%WorldCount;selectWorld(GetPrivateProfileIntW(section.c_str(),L"World",legacyWorld,path.c_str()));timelineStep_=0;scheduleNextWorld(tuned(audio_.frame()),elapsed());settings_.save();if(swap_)resize(width_,height_);
    }
    void handleHudPointer(POINT point,int direction){if(!hudVisible_)return;float left=panelLeft(),top=HudTop+HudRowsTop;int row=int((point.y-top)/HudRowHeight);if(point.x<left||point.x>left+HudWidth||row<0||row>=SettingCount)return;hudSelection_=row;if(direction==0){if(point.x>=left+HudButtonLeft&&point.x<=left+HudButtonLeft+HudButtonSize)direction=-1;else if(point.x>=left+HudButtonRight&&point.x<=left+HudButtonRight+HudButtonSize)direction=1;}if(direction)adjustSetting(direction);}
    void updateHud(const AudioFrame&raw){hudFrame_=tuned(raw);hudDevice_=audio_.name();if(hudDevice_.size()>43)hudDevice_=hudDevice_.substr(0,40)+L"...";}
    float panelLeft()const{return std::max(12.f,float(width_)-HudWidth-HudTop);}
    static std::wstring percent(float value){return std::to_wstring((int)std::round(value*100))+L"%";}
    std::wstring settingValue(int index)const{static const wchar_t*profiles[]={L"CALM",L"BALANCED",L"STAGE",L"AGGRESSIVE"};static const wchar_t*transitions[]={L"MORPH MATCH",L"HORIZON RISE",L"SIGNAL VEIL",L"GATE OPEN"};static const wchar_t*quality[]={L"PERFORMANCE",L"BALANCED",L"NATIVE HD",L"ULTRA 4K"};switch(index){case 0:return profiles[settings_.profile];case 1:return automatic_&&!settings_.timeline?std::wstring(L"AUTO / ")+paletteName(directorPalette_):paletteName(settings_.palette);case 2:return percent(settings_.sensitivity);case 3:return percent(settings_.bassGain);case 4:return percent(settings_.midGain);case 5:return percent(settings_.highGain);case 6:return percent(settings_.beatGain);case 7:return percent(settings_.logoScale);case 8:return std::to_wstring((int)settings_.sceneSeconds)+L" SEC";case 9:return std::to_wstring((int)std::round(settings_.transitionSeconds*10))+L"/10 SEC";case 10:return transitions[settings_.transitionStyle];case 11:return quality[settings_.quality];case 12:return settings_.vsync?L"DISPLAY / 60":L"UNLOCKED / 120";case 13:return settings_.timeline?L"SHOW TIMELINE":L"SMART DIRECTOR";case 14:
#ifdef RARESTAGE_SPOUT
        return settings_.spout?(spoutReady_?L"ON / CLEAN":L"STARTING"):L"OFF";
#else
        return L"NOT BUILT";
#endif
        default:return L"";}}
    bool initOverlay(){D2D1_FACTORY_OPTIONS options{};
#ifdef _DEBUG
        options.debugLevel=D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        if(FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),&options,reinterpret_cast<void**>(d2dFactory_.GetAddressOf()))))return false;ComPtr<IDXGIDevice>dxgiDevice;if(FAILED(device_.As(&dxgiDevice))||FAILED(d2dFactory_->CreateDevice(dxgiDevice.Get(),&d2dDevice_))||FAILED(d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&d2dContext_)))return false;
        if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()))))return false;
        if(FAILED(dwriteFactory_->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13.f,L"en-us",&hudFormat_))||FAILED(dwriteFactory_->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13.f,L"en-us",&valueFormat_))||FAILED(dwriteFactory_->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,20.f,L"en-us",&titleFormat_))||FAILED(dwriteFactory_->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,11.f,L"en-us",&smallFormat_))||FAILED(dwriteFactory_->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13.f,L"en-us",&centerFormat_)))return false;
        hudFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);valueFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);valueFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);centerFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);centerFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        return SUCCEEDED(d2dContext_->CreateSolidColorBrush(D2D1::ColorF(.012f,.035f,.065f,.94f),&panelBrush_))&&SUCCEEDED(d2dContext_->CreateSolidColorBrush(D2D1::ColorF(.09f,.94f,1.f,1.f),&accentBrush_))&&SUCCEEDED(d2dContext_->CreateSolidColorBrush(D2D1::ColorF(.84f,.97f,1.f,1.f),&textBrush_))&&SUCCEEDED(d2dContext_->CreateSolidColorBrush(D2D1::ColorF(.38f,.58f,.67f,1.f),&mutedBrush_))&&SUCCEEDED(d2dContext_->CreateSolidColorBrush(D2D1::ColorF(.03f,.45f,.56f,.32f),&selectionBrush_))&&SUCCEEDED(d2dContext_->CreateSolidColorBrush(D2D1::ColorF(.015f,.09f,.14f,.95f),&meterBrush_));
    }
    bool createOverlayTarget(){if(!d2dContext_||!backBuffer_)return false;d2dContext_->SetTarget(nullptr);d2dTarget_.Reset();ComPtr<IDXGISurface>surface;if(FAILED(backBuffer_.As(&surface)))return false;auto properties=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96.f,96.f);if(FAILED(d2dContext_->CreateBitmapFromDxgiSurface(surface.Get(),&properties,&d2dTarget_)))return false;d2dContext_->SetTarget(d2dTarget_.Get());return true;}
    void drawOverlay(){if(!hudVisible_||!d2dContext_)return;if(!d2dTarget_&&!createOverlayTarget())return;float left=panelLeft(),right=std::min(float(width_)-12.f,left+HudWidth),bottom=std::min(float(height_)-12.f,702.f);auto text=[&](const std::wstring&value,IDWriteTextFormat*format,ID2D1Brush*brush,D2D1_RECT_F rect){d2dContext_->DrawTextW(value.c_str(),(UINT32)value.size(),format,rect,brush,D2D1_DRAW_TEXT_OPTIONS_CLIP);};d2dContext_->BeginDraw();d2dContext_->SetTransform(D2D1::Matrix3x2F::Identity());auto panel=D2D1::RoundedRect(D2D1::RectF(left,HudTop,right,bottom),12,12);d2dContext_->FillRoundedRectangle(panel,panelBrush_.Get());d2dContext_->DrawRoundedRectangle(panel,accentBrush_.Get(),1.2f);d2dContext_->FillRectangle(D2D1::RectF(left+18,HudTop+17,right-18,HudTop+20),accentBrush_.Get());
        text(L"RARESTAGE  /  SHOW CONTROL",titleFormat_.Get(),textBrush_.Get(),D2D1::RectF(left+18,HudTop+25,right-18,HudTop+53));text(std::wstring(L"PRESET ")+std::to_wstring(presetIndex_+1)+L"  •  "+presetName(presetIndex_),smallFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+19,HudTop+52,right-18,HudTop+70));
        static const wchar_t*labels[]={L"REACTION",L"PALETTE",L"SENSITIVITY",L"BASS",L"MIDS",L"HIGHS",L"BEAT",L"LOGO",L"WORLD DWELL",L"TRANSITION",L"TRANSITION FX",L"RENDER PROFILE",L"FRAME OUTPUT",L"SHOW MODE",L"SPOUT OUTPUT"};
        for(int row=0;row<SettingCount;++row){float y=HudTop+HudRowsTop+row*HudRowHeight;auto rowRect=D2D1::RoundedRect(D2D1::RectF(left+12,y,right-12,y+HudRowHeight-2),5,5);if(row==hudSelection_)d2dContext_->FillRoundedRectangle(rowRect,selectionBrush_.Get());text(labels[row],hudFormat_.Get(),row==hudSelection_?textBrush_.Get():mutedBrush_.Get(),D2D1::RectF(left+22,y,left+HudButtonLeft-8,y+HudRowHeight-2));text(L"−",centerFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+HudButtonLeft,y+2,left+HudButtonLeft+HudButtonSize,y+HudRowHeight-4));text(settingValue(row),valueFormat_.Get(),textBrush_.Get(),D2D1::RectF(left+HudButtonLeft+HudButtonSize+5,y,right-HudButtonSize-32,y+HudRowHeight-2));text(L"+",centerFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+HudButtonRight,y+2,left+HudButtonRight+HudButtonSize,y+HudRowHeight-4));}
        float signalTop=HudTop+HudRowsTop+SettingCount*HudRowHeight+9;text(L"LIVE SIGNAL",smallFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+20,signalTop,right-20,signalTop+16));const float values[]={hudFrame_.bass,hudFrame_.mid,hudFrame_.high,hudFrame_.beat};const wchar_t*meterNames[]={L"BASS",L"MID",L"HIGH",L"BEAT"};for(int i=0;i<4;++i){float y=signalTop+19+i*16;text(meterNames[i],smallFormat_.Get(),mutedBrush_.Get(),D2D1::RectF(left+20,y,left+62,y+14));auto track=D2D1::RoundedRect(D2D1::RectF(left+68,y+3,right-70,y+10),3,3);d2dContext_->FillRoundedRectangle(track,meterBrush_.Get());auto fill=D2D1::RoundedRect(D2D1::RectF(left+68,y+3,left+68+(right-left-138)*std::clamp(values[i],0.f,1.f),y+10),3,3);d2dContext_->FillRoundedRectangle(fill,accentBrush_.Get());text(percent(values[i]),smallFormat_.Get(),textBrush_.Get(),D2D1::RectF(right-64,y,right-20,y+14));}
        float statusTop=signalTop+88;std::wstring bpm=hudFrame_.bpm>0?std::to_wstring((int)std::round(hudFrame_.bpm))+L" BPM":L"BPM --";text(std::wstring(L"WORLD ")+std::to_wstring(world_+1)+L"  /  "+worldName(world_)+(automatic_?L"  [AUTO]":L"  [MANUAL]"),hudFormat_.Get(),textBrush_.Get(),D2D1::RectF(left+20,statusTop,right-20,statusTop+20));text(bpm+L"  •  ACT "+sectionName(songSection_)+L"  •  "+hudDevice_,smallFormat_.Get(),mutedBrush_.Get(),D2D1::RectF(left+20,statusTop+21,right-20,statusTop+38));text(L"CLICK / WHEEL TO ADJUST   •   TAB HIDE   •   F1-F4 PRESETS",smallFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+20,statusTop+43,right-20,statusTop+60));
        HRESULT result=d2dContext_->EndDraw();if(result==D2DERR_RECREATE_TARGET){d2dContext_->SetTarget(nullptr);d2dTarget_.Reset();}
    }
    void initSpout(){
#ifdef RARESTAGE_SPOUT
        if(!spoutReady_&&device_){spoutReady_=spout_.OpenDirectX11(device_.Get());if(spoutReady_){spout_.SetSenderName("RareStage Clean");spout_.SetSenderFormat(DXGI_FORMAT_B8G8R8A8_UNORM);StartupLog("Spout2 clean sender ready");}else StartupLog("Spout2 sender unavailable");}
#endif
    }
    void sendSpout(){
#ifdef RARESTAGE_SPOUT
        if(settings_.spout&&spoutReady_&&backBuffer_)spout_.SendTexture(backBuffer_.Get());
#endif
    }
    void shutdownSpout(){
#ifdef RARESTAGE_SPOUT
        if(spoutReady_){spout_.ReleaseSender();spout_.CloseDirectX11();spoutReady_=false;}
#endif
    }
    static BOOL CALLBACK EnumMonitor(HMONITOR,HDC,LPRECT rectangle,LPARAM data){reinterpret_cast<std::vector<RECT>*>(data)->push_back(*rectangle);return TRUE;}
    static std::vector<RECT> monitorRects(){std::vector<RECT>result;EnumDisplayMonitors(nullptr,nullptr,EnumMonitor,(LPARAM)&result);return result;}
    void cycleMonitor(){auto monitors=monitorRects();if(monitors.empty())return;settings_.monitor=(settings_.monitor+1)%(int)monitors.size();monitorCount_=(int)monitors.size();settings_.save();RECT target=monitors[settings_.monitor];if(fullscreen_)SetWindowPos(hwnd_,HWND_TOP,target.left,target.top,target.right-target.left,target.bottom-target.top,SWP_FRAMECHANGED);else{RECT current{};GetWindowRect(hwnd_,&current);int w=current.right-current.left,h=current.bottom-current.top;SetWindowPos(hwnd_,nullptr,target.left+(target.right-target.left-w)/2,target.top+(target.bottom-target.top-h)/2,w,h,SWP_NOZORDER);}}
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
    bool createGpu(){DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=hwnd_;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        D3D_FEATURE_LEVEL fl;UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        flags|=D3D11_CREATE_DEVICE_DEBUG;
#endif
        HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&sd,&swap_,&device_,&fl,&context_);
        if(FAILED(hr)){swap_.Reset();device_.Reset();context_.Reset();hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&sd,&swap_,&device_,&fl,&context_);}
        if(FAILED(hr)){StartupLog("D3D11 device and swap chain creation failed");return false;}
        ComPtr<ID3DBlob> vsb,sceneBlob,bloomBlob,compositeBlob,err;
        if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vsb,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage vertex shader error",MB_OK|MB_ICONERROR);return false;}
        err.Reset();if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&sceneBlob,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage scene shader error",MB_OK|MB_ICONERROR);return false;}
        err.Reset();if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"psBloom","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&bloomBlob,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage bloom shader error",MB_OK|MB_ICONERROR);return false;}
        err.Reset();if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"psComposite","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&compositeBlob,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage composite shader error",MB_OK|MB_ICONERROR);return false;}
        if(FAILED(device_->CreateVertexShader(vsb->GetBufferPointer(),vsb->GetBufferSize(),nullptr,&vs_))){StartupLog("Vertex shader creation failed");return false;}
        if(FAILED(device_->CreatePixelShader(sceneBlob->GetBufferPointer(),sceneBlob->GetBufferSize(),nullptr,&scenePs_))||FAILED(device_->CreatePixelShader(bloomBlob->GetBufferPointer(),bloomBlob->GetBufferSize(),nullptr,&bloomPs_))||FAILED(device_->CreatePixelShader(compositeBlob->GetBufferPointer(),compositeBlob->GetBufferSize(),nullptr,&compositePs_))){StartupLog("Festival shader creation failed");return false;}
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(device_->CreateBuffer(&bd,nullptr,&constants_))){StartupLog("Frame constant buffer creation failed");return false;}if(!loadLogo())return false;if(!initOverlay()){StartupLog("Direct2D show control initialization failed");return false;}initSpout();RECT r;GetClientRect(hwnd_,&r);resize(r.right,r.bottom);return rtv_.Get()&&sceneRtv_.Get()&&sceneSrv_.Get()&&bloomRtv_.Get()&&bloomSrv_.Get()&&d2dTarget_.Get();}
    float renderScale()const{static constexpr float scales[]{.62f,.78f,1.f,1.25f};return scales[std::clamp(settings_.quality,0,3)];}
    void resize(UINT w,UINT h){if(!w||!h)return;ID3D11ShaderResourceView*nullViews[3]{};context_->PSSetShaderResources(0,3,nullViews);context_->OMSetRenderTargets(0,nullptr,nullptr);if(d2dContext_)d2dContext_->SetTarget(nullptr);d2dTarget_.Reset();backBuffer_.Reset();rtv_.Reset();sceneRtv_.Reset();sceneSrv_.Reset();sceneTexture_.Reset();bloomRtv_.Reset();bloomSrv_.Reset();bloomTexture_.Reset();if(FAILED(swap_->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0))||FAILED(swap_->GetBuffer(0,IID_PPV_ARGS(&backBuffer_)))||FAILED(device_->CreateRenderTargetView(backBuffer_.Get(),nullptr,&rtv_))){StartupLog("Swap chain resize failed");return;}
        auto createTarget=[&](UINT tw,UINT th,ComPtr<ID3D11Texture2D>&texture,ComPtr<ID3D11RenderTargetView>&target,ComPtr<ID3D11ShaderResourceView>&view){D3D11_TEXTURE2D_DESC td{};td.Width=tw;td.Height=th;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;return SUCCEEDED(device_->CreateTexture2D(&td,nullptr,&texture))&&SUCCEEDED(device_->CreateRenderTargetView(texture.Get(),nullptr,&target))&&SUCCEEDED(device_->CreateShaderResourceView(texture.Get(),nullptr,&view));};
        float scale=renderScale();renderWidth_=std::max(1u,(UINT)std::round(w*scale));renderHeight_=std::max(1u,(UINT)std::round(h*scale));if(!createTarget(renderWidth_,renderHeight_,sceneTexture_,sceneRtv_,sceneSrv_)||!createTarget(std::max(1u,renderWidth_/2),std::max(1u,renderHeight_/2),bloomTexture_,bloomRtv_,bloomSrv_))StartupLog("HDR render target creation failed");width_=w;height_=h;if(!createOverlayTarget())StartupLog("Show control compositor target creation failed");}
    void uploadConstants(const Constants&values){D3D11_MAPPED_SUBRESOURCE map{};if(SUCCEEDED(context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map))){memcpy(map.pData,&values,sizeof(values));context_->Unmap(constants_.Get(),0);}}
    void render(){if(!rtv_||!sceneRtv_||!bloomRtv_){Sleep(10);return;}float seconds=elapsed();auto raw=audio_.frame();auto af=tuned(raw);float dt=lastRenderSeconds_>0?std::clamp(seconds-lastRenderSeconds_,0.f,.10f):0.f;lastRenderSeconds_=seconds;float targetBpm=af.bpm>=65.f&&af.bpm<=190.f&&af.tempoConfidence>.10f?af.bpm:120.f;visualBpm_+=(targetBpm-visualBpm_)*std::clamp(dt*(.55f+af.tempoConfidence*2.4f),0.f,1.f);tempoPhase_=std::fmod(tempoPhase_+dt*visualBpm_/60.f,1024.f);bool beatNow=af.onset>.42f||(af.beat>.28f&&af.flux>.06f);if(beatNow&&!beatLatched_){float nearest=std::round(tempoPhase_);tempoPhase_+=(nearest-tempoPhase_)*.28f;}beatLatched_=beatNow;updateSongSection(af,dt,seconds);float paletteTarget=float(automatic_&&!settings_.timeline?directorPalette_:settings_.palette);float paletteDelta=paletteTarget-visualPalette_;if(paletteDelta>2)paletteDelta-=4;if(paletteDelta<-2)paletteDelta+=4;visualPalette_+=paletteDelta*std::clamp(dt*.72f,0.f,1.f);if(visualPalette_<0)visualPalette_+=4;if(visualPalette_>=4)visualPalette_-=4;
        if(automatic_&&!frozen_){bool chapter=songSection_==DropSection||songSection_==BreakdownSection||songSection_==OpenSection;bool freshChapter=seconds-sectionStartedAt_<8.f;if((settings_.timeline&&seconds>=nextWorldAt_)||(seconds>=nextWorldAt_&&chapter&&freshChapter)||seconds-worldStartedAt_>=120.f){if(!worldChangeArmed_)worldChangeReadyAt_=seconds;worldChangeArmed_=true;}if(worldChangeArmed_&&((af.beat>.24f&&af.flux>.055f)||seconds-worldChangeReadyAt_>(sectionChangePending_?1.25f:2.20f))){selectWorld(chooseAutoWorld(af),true);scheduleNextWorld(af,seconds);}}
        float transition=activeTransitionSeconds_>0?std::clamp((seconds-transitionStarted_)/activeTransitionSeconds_,0.f,1.f):1.f;float energy=directorEnergy(af);float sectionProgress=std::clamp((seconds-sectionStartedAt_)/std::max(phraseDuration(af),8.f),0.f,1.f);
        Constants c{{float(renderWidth_),float(renderHeight_)},seconds,float(world_),af.bass,af.mid,af.high,af.level,af.peak,af.stereo,af.beat,af.flux,float(previousWorld_),transition,settings_.logoScale,visualPalette_,float(activeTransitionStyle_),tempoPhase_,af.tempoConfidence,energy,float(songSection_),sectionProgress,dropPulse_,af.onset};uploadConstants(c);
        D3D11_VIEWPORT sceneViewport{0,0,float(renderWidth_),float(renderHeight_),0,1};D3D11_VIEWPORT bloomViewport{0,0,float(std::max(1u,renderWidth_/2)),float(std::max(1u,renderHeight_/2)),0,1};D3D11_VIEWPORT outputViewport{0,0,float(width_),float(height_),0,1};context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetConstantBuffers(0,1,constants_.GetAddressOf());context_->PSSetSamplers(0,1,logoSampler_.GetAddressOf());
        context_->RSSetViewports(1,&sceneViewport);context_->OMSetRenderTargets(1,sceneRtv_.GetAddressOf(),nullptr);context_->PSSetShader(scenePs_.Get(),nullptr,0);context_->PSSetShaderResources(0,1,logoSrv_.GetAddressOf());context_->Draw(3,0);
        context_->RSSetViewports(1,&bloomViewport);context_->OMSetRenderTargets(1,bloomRtv_.GetAddressOf(),nullptr);context_->PSSetShader(bloomPs_.Get(),nullptr,0);context_->PSSetShaderResources(1,1,sceneSrv_.GetAddressOf());context_->Draw(3,0);
        c.resolution[0]=float(width_);c.resolution[1]=float(height_);uploadConstants(c);ID3D11ShaderResourceView*postViews[]{sceneSrv_.Get(),bloomSrv_.Get()};context_->RSSetViewports(1,&outputViewport);context_->OMSetRenderTargets(1,rtv_.GetAddressOf(),nullptr);context_->PSSetShader(compositePs_.Get(),nullptr,0);context_->PSSetShaderResources(1,2,postViews);context_->Draw(3,0);ID3D11ShaderResourceView*nullViews[3]{};context_->PSSetShaderResources(0,3,nullViews);context_->OMSetRenderTargets(0,nullptr,nullptr);sendSpout();drawOverlay();swap_->Present(settings_.vsync?1:0,0);
        ++frames_;if(seconds-lastHud_>.10f){lastHud_=seconds;updateHud(raw);}if(seconds-lastTitle_>1){float fps=frames_/(seconds-lastTitle_);frames_=0;lastTitle_=seconds;std::wstring title=L"RareStage  |  "+std::wstring(worldName(world_))+(automatic_?L" [AUTO / "+std::wstring(sectionName(songSection_))+L"]":L" [MANUAL]")+L"  |  "+presetName(presetIndex_)+L"  |  "+audio_.name()+L"  |  "+(af.bpm>0?std::to_wstring((int)af.bpm)+L" BPM  |  ":L"")+std::to_wstring((int)fps)+L" FPS";SetWindowTextW(hwnd_,title.c_str());}}
    void fullscreen(){fullscreen_=!fullscreen_;if(fullscreen_){GetWindowRect(hwnd_,&windowRect_);SetWindowLongW(hwnd_,GWL_STYLE,WS_POPUP);auto monitors=monitorRects();if(monitors.empty())return;settings_.monitor%=monitors.size();RECT target=monitors[settings_.monitor];SetWindowPos(hwnd_,HWND_TOP,target.left,target.top,target.right-target.left,target.bottom-target.top,SWP_FRAMECHANGED);}else{SetWindowLongW(hwnd_,GWL_STYLE,WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd_,nullptr,windowRect_.left,windowRect_.top,windowRect_.right-windowRect_.left,windowRect_.bottom-windowRect_.top,SWP_FRAMECHANGED|SWP_NOZORDER);}}
    static constexpr int SettingCount=15;static constexpr float HudWidth=420.f,HudTop=18.f,HudRowsTop=74.f,HudRowHeight=28.f,HudButtonLeft=248.f,HudButtonRight=376.f,HudButtonSize=24.f;
    HWND hwnd_{};UINT width_=1280,height_=720,renderWidth_=1280,renderHeight_=720;bool fullscreen_=false,automatic_=true,frozen_=false,hudVisible_=true,worldChangeArmed_=false,beatLatched_=false,sectionChangePending_=false,heardActiveMusic_=false;RECT windowRect_{};int world_=0,previousWorld_=0,hudSelection_=0,autoSequence_=0,timelineStep_=0,presetIndex_=0,monitorCount_=1,songSection_=OpenSection,sectionCandidate_=OpenSection,sectionRevision_=0,lastDirectorPhrase_=-1,directorPalette_=0,activeTransitionStyle_=0;std::array<int,4>recentWorlds_{{-1,-1,-1,-1}};AudioEngine audio_;ShowSettings settings_;AudioFrame hudFrame_{};std::wstring hudDevice_=L"Starting audio capture...";
    Clock::time_point started_;float lastTitle_{},lastHud_{},lastRenderSeconds_{},transitionStarted_{-100},nextWorldAt_{72},worldStartedAt_{},worldChangeReadyAt_{72},visualBpm_{120},tempoPhase_{},visualPalette_{},activeTransitionSeconds_{1.4f},sectionShortEnergy_{},sectionLongEnergy_{},sectionCandidateSeconds_{},sectionStartedAt_{},sectionLockUntil_{},dropPulse_{},lastDropAt_{-100};int frames_{};ComPtr<ID3D11Device>device_;ComPtr<ID3D11DeviceContext>context_;ComPtr<IDXGISwapChain>swap_;ComPtr<ID3D11RenderTargetView>rtv_,sceneRtv_,bloomRtv_;ComPtr<ID3D11Texture2D>backBuffer_,sceneTexture_,bloomTexture_;ComPtr<ID3D11ShaderResourceView>sceneSrv_,bloomSrv_,logoSrv_;ComPtr<ID3D11VertexShader>vs_;ComPtr<ID3D11PixelShader>scenePs_,bloomPs_,compositePs_;ComPtr<ID3D11Buffer>constants_;ComPtr<ID3D11SamplerState>logoSampler_;
    ComPtr<ID2D1Factory1>d2dFactory_;ComPtr<ID2D1Device>d2dDevice_;ComPtr<ID2D1DeviceContext>d2dContext_;ComPtr<ID2D1Bitmap1>d2dTarget_;ComPtr<IDWriteFactory>dwriteFactory_;ComPtr<IDWriteTextFormat>hudFormat_,valueFormat_,titleFormat_,smallFormat_,centerFormat_;ComPtr<ID2D1SolidColorBrush>panelBrush_,accentBrush_,textBrush_,mutedBrush_,selectionBrush_,meterBrush_;
#ifdef RARESTAGE_SPOUT
    spoutDX spout_;bool spoutReady_=false;
#endif
};

int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int){
    SetUnhandledExceptionFilter(CrashLog);
    {std::ofstream clear(StartupLogPath());clear << "RareStage startup beginning\n";}
    if(std::wcsstr(GetCommandLineW(),L"--self-test")){
        ComPtr<ID3DBlob> blob,error;
        if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 2;}
        blob.Reset();error.Reset();
        if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 3;}
        blob.Reset();error.Reset();
        if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"psBloom","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 4;}
        blob.Reset();error.Reset();
        if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"psComposite","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 5;}
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);App app;
    if(!app.init(h)){StartupLog("Startup failed");if(SUCCEEDED(com))CoUninitialize();return 1;}
    if(std::wcsstr(GetCommandLineW(),L"--startup-test"))app.requestClose();
    int result=app.run();StartupLog("RareStage exited normally");if(SUCCEEDED(com))CoUninitialize();return result;
}

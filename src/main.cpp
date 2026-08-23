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

struct AudioFrame { float bass{}, mid{}, high{}, level{}, peak{}, stereo{}, beat{}, flux{}, bpm{}, tempoConfidence{}; };

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
        ceilings_.fill(.0001f);previousBass_=previousMid_=beatEnvelope_=fluxEnvelope_=beatInterval_=tempoConfidence_=0;beatCooldown_=0;lastBeatTime_={};
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
        float flux=std::max(0.f,normalized[0]-previousBass_)+.45f*std::max(0.f,normalized[1]-previousMid_);previousBass_=normalized[0];previousMid_=normalized[1];
        if(beatCooldown_>0)--beatCooldown_;float beatTarget=0;bool beatDetected=flux>.070f&&normalized[0]>.18f&&beatCooldown_==0;
        if(beatDetected){beatTarget=.78f;beatCooldown_=8;auto now=Clock::now();if(lastBeatTime_.time_since_epoch().count()!=0){float interval=std::chrono::duration<float>(now-lastBeatTime_).count();while(interval<.30f)interval*=2;while(interval>1.f)interval*=.5f;if(interval>=.30f&&interval<=1.f){if(beatInterval_==0){beatInterval_=interval;tempoConfidence_=.20f;}else{float consistency=saturateAudio(1-std::abs(interval-beatInterval_)/.12f);beatInterval_+=(interval-beatInterval_)*.22f;tempoConfidence_=saturateAudio(tempoConfidence_*.82f+consistency*.18f);}}}lastBeatTime_=now;}
        if(lastBeatTime_.time_since_epoch().count()!=0&&std::chrono::duration<float>(Clock::now()-lastBeatTime_).count()>3.f)tempoConfidence_*=.995f;
        beatEnvelope_=beatTarget>0?beatEnvelope_+(beatTarget-beatEnvelope_)*.48f:beatEnvelope_*.86f;fluxEnvelope_=std::max(flux,fluxEnvelope_*.82f);
        AudioFrame next{normalized[0],normalized[1],normalized[2],normalized[3],saturateAudio(peak*2.6f),stereo,beatEnvelope_,saturateAudio(fluxEnvelope_*2.4f),beatInterval_>0?60.f/beatInterval_:0,tempoConfidence_};
        std::scoped_lock lock(mutex_); constexpr float attack=.28f, release=.09f;
        auto smooth=[](float old,float value){float k=value>old?attack:release;return old+(value-old)*k;};
        frame_.bass=smooth(frame_.bass,next.bass);frame_.mid=smooth(frame_.mid,next.mid);frame_.high=smooth(frame_.high,next.high);frame_.level=smooth(frame_.level,next.level);
        frame_.peak=smooth(frame_.peak,next.peak);frame_.stereo=smooth(frame_.stereo,stereo);frame_.beat=smooth(frame_.beat,next.beat);frame_.flux=smooth(frame_.flux,next.flux);frame_.bpm=next.bpm;frame_.tempoConfidence=smooth(frame_.tempoConfidence,next.tempoConfidence);
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
    mutable std::mutex mutex_;AudioFrame frame_{};std::wstring name_=L"Starting audio capture...";std::array<float,4>ceilings_{.0001f,.0001f,.0001f,.0001f};float previousBass_{},previousMid_{},beatEnvelope_{},fluxEnvelope_{},beatInterval_{},tempoConfidence_{};int beatCooldown_{};Clock::time_point lastBeatTime_{};
};

static std::filesystem::path SettingsPath(){
    wchar_t local[MAX_PATH]{};DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH);std::filesystem::path directory;
    if(length>0&&length<MAX_PATH)directory=std::filesystem::path(local)/L"RareStage";else{wchar_t module[MAX_PATH]{};GetModuleFileNameW(nullptr,module,MAX_PATH);directory=std::filesystem::path(module).parent_path();}
    std::error_code error;std::filesystem::create_directories(directory,error);return directory/L"settings.ini";
}

struct ShowSettings {
    int profile=1,palette=0,monitor=0,automatic=1,quality=2,vsync=1,timeline=0,spout=1,transitionStyle=0;
    float sensitivity=1,bassGain=1,midGain=1,highGain=1,beatGain=1,logoScale=1,sceneSeconds=18,transitionSeconds=1.2f;
    static float readFloat(const std::wstring&path,const wchar_t*key,float fallback){wchar_t text[32]{};wchar_t fallbackText[32]{};std::swprintf(fallbackText,std::size(fallbackText),L"%.2f",fallback);GetPrivateProfileStringW(L"Show",key,fallbackText,text,(DWORD)std::size(text),path.c_str());return std::wcstof(text,nullptr);}
    static float readFloat(const std::wstring&path,const wchar_t*section,const wchar_t*key,float fallback){wchar_t text[32]{};wchar_t fallbackText[32]{};std::swprintf(fallbackText,std::size(fallbackText),L"%.2f",fallback);GetPrivateProfileStringW(section,key,fallbackText,text,(DWORD)std::size(text),path.c_str());return std::wcstof(text,nullptr);}
    void load(const wchar_t*section=L"Show"){auto path=SettingsPath().wstring();profile=std::clamp(int(GetPrivateProfileIntW(section,L"Profile",profile,path.c_str())),0,3);palette=std::clamp(int(GetPrivateProfileIntW(section,L"Palette",palette,path.c_str())),0,3);monitor=std::max(0,int(GetPrivateProfileIntW(section,L"Monitor",monitor,path.c_str())));automatic=GetPrivateProfileIntW(section,L"Automatic",automatic,path.c_str())?1:0;quality=std::clamp(int(GetPrivateProfileIntW(section,L"Quality",quality,path.c_str())),0,3);vsync=GetPrivateProfileIntW(section,L"VSync",vsync,path.c_str())?1:0;timeline=GetPrivateProfileIntW(section,L"Timeline",timeline,path.c_str())?1:0;spout=GetPrivateProfileIntW(section,L"Spout",spout,path.c_str())?1:0;transitionStyle=std::clamp(int(GetPrivateProfileIntW(section,L"TransitionStyle",transitionStyle,path.c_str())),0,3);
        sensitivity=std::clamp(readFloat(path,section,L"Sensitivity",sensitivity),.50f,1.50f);bassGain=std::clamp(readFloat(path,section,L"Bass",bassGain),.50f,1.50f);midGain=std::clamp(readFloat(path,section,L"Mids",midGain),.50f,1.50f);highGain=std::clamp(readFloat(path,section,L"Highs",highGain),.50f,1.50f);beatGain=std::clamp(readFloat(path,section,L"Beat",beatGain),.50f,1.50f);logoScale=std::clamp(readFloat(path,section,L"Logo",logoScale),.75f,1.25f);sceneSeconds=std::clamp(readFloat(path,section,L"SceneSeconds",sceneSeconds),8.f,60.f);transitionSeconds=std::clamp(readFloat(path,section,L"TransitionSeconds",transitionSeconds),.2f,3.f);}
    void save(const wchar_t*section=L"Show")const{auto path=SettingsPath().wstring();auto writeInt=[&](const wchar_t*key,int value){wchar_t text[16]{};std::swprintf(text,std::size(text),L"%d",value);WritePrivateProfileStringW(section,key,text,path.c_str());};auto writeFloat=[&](const wchar_t*key,float value){wchar_t text[32]{};std::swprintf(text,std::size(text),L"%.2f",value);WritePrivateProfileStringW(section,key,text,path.c_str());};
        writeInt(L"Profile",profile);writeInt(L"Palette",palette);writeInt(L"Monitor",monitor);writeInt(L"Automatic",automatic);writeInt(L"Quality",quality);writeInt(L"VSync",vsync);writeInt(L"Timeline",timeline);writeInt(L"Spout",spout);writeInt(L"TransitionStyle",transitionStyle);writeFloat(L"Sensitivity",sensitivity);writeFloat(L"Bass",bassGain);writeFloat(L"Mids",midGain);writeFloat(L"Highs",highGain);writeFloat(L"Beat",beatGain);writeFloat(L"Logo",logoScale);writeFloat(L"SceneSeconds",sceneSeconds);writeFloat(L"TransitionSeconds",transitionSeconds);}
};

static const char* Shader = R"(
cbuffer Frame : register(b0){float2 resolution;float time;float scene;float bass;float mid;float high;float level;float peak;float stereo;float beat;float flux;float previousScene;float transition;float logoScale;float palette;float transitionStyle;float tempoPhase;float tempoConfidence;float audioEnergy;float songSection;float sectionProgress;float dropPulse;float framePadding;}
Texture2D logoTex:register(t0);Texture2D postScene:register(t1);Texture2D postBloom:register(t2);SamplerState linearSampler:register(s0);
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
O vs(uint id:SV_VertexID){O o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float ring(float2 p,float radius,float width){return width/max(abs(length(p)-radius),.001);}
float lineGlow(float value,float width){return width/max(abs(value),.001);}
float boxRing(float2 p,float2 size,float width){return width/max(abs(max(abs(p.x)-size.x,abs(p.y)-size.y)),.001);}
float easeBand(float value){value=saturate(value);return value*value*(3-2*value);}
float2 rotate2(float2 p,float angle){float c=cos(angle),s=sin(angle);return float2(c*p.x-s*p.y,s*p.x+c*p.y);}
float sdBox(float3 p,float3 size){float3 q=abs(p)-size;return length(max(q,0))+min(max(q.x,max(q.y,q.z)),0);}
float sdOcta(float3 p,float size){p=abs(p);return(p.x+p.y+p.z-size)*.57735027;}
float fillBox(float2 p,float2 size,float feather){float2 q=abs(p)-size;return 1-smoothstep(0,feather,max(q.x,q.y));}
float softDisc(float2 p,float radius,float feather){return 1-smoothstep(radius-feather,radius+feather,length(p));}
float flowField(float2 p,float phase){float2 q=p;q+=sin(q.yx*float2(3.1,4.2)+phase)*.13;return .5+.5*sin(q.x*4.4+sin(q.y*3.6-phase*.73)+phase);}
float morphGrid(float2 p,float phase,float scale){float2 g=p*scale;float2 id=floor(g);float2 q=frac(g)-.5;float local=phase+hash(id)*6.283185;q=rotate2(q,sin(local)*.32);float morph=.5+.5*sin(local*.71);float shape=lerp(length(q),max(abs(q.x),abs(q.y)),morph);float radius=.20+.055*sin(local+phase*.31);return lineGlow(shape-radius,.010);}
float ribbonSet(float2 p,float phase,float motion){float ribbons=0;[unroll]for(int k=0;k<5;k++){float lane=(k-2)*.115;float wave=lane+sin(p.x*(1.8+k*.13)+phase+k*.72)*(.025+motion*.042);ribbons+=lineGlow(p.y-wave,.0028);}return ribbons;}
float radialBeams(float2 p,float phase,float count,float focus){float angle=atan2(p.y,p.x);float blade=pow(saturate(.5+.5*cos(angle*count+phase)),focus);return blade*smoothstep(.08,.22,length(p))*(1-smoothstep(.42,1.24,length(p)));}
float transitionOldMask(float2 uv,float amount,float style,float sceneSeed){
 float progress=easeBand(saturate(amount));float2 q=uv-.5;
 if(style<.5){float lane=floor(uv.y*10);float reverse=step(.5,frac(lane*.5));float coordinate=lerp(uv.x,1-uv.x,reverse);float edge=progress*1.35-hash(float2(lane,sceneSeed))*.18-.02;return smoothstep(edge-.035,edge+.035,coordinate);}
 if(style<1.5){float coordinate=(abs(q.x)+abs(q.y))*.92+length(q)*.68;float edge=progress*1.42-.08;return smoothstep(edge-.035,edge+.035,coordinate);}
 if(style<2.5){float2 cell=floor(uv*float2(12,7));float reverse=step(.5,hash(cell.yx+sceneSeed));float coordinate=lerp(uv.x,1-uv.x,reverse);float edge=progress*1.45-hash(cell+sceneSeed)*.30-.02;return smoothstep(edge-.018,edge+.018,coordinate);}
 float lane=floor(uv.x*9);float coordinate=abs(uv.y-.5)*2;float edge=progress*1.28-hash(float2(lane,sceneSeed))*.15-.02;return smoothstep(edge-.030,edge+.030,coordinate);
}
void paletteKey(float selection,out float3 primary,out float3 accent,out float3 spectral){primary=float3(.00,.78,1.18);accent=float3(.62,1.12,.04);spectral=float3(.60,.08,1.08);if(selection>.5&&selection<1.5){primary=float3(.12,.48,1.25);accent=float3(.02,1.05,.76);spectral=float3(.72,.10,1.20);}else if(selection>1.5&&selection<2.5){primary=float3(1.18,.18,.68);accent=float3(.22,.92,1.15);spectral=float3(.62,.12,1.15);}else if(selection>2.5){primary=float3(1.30,.36,.03);accent=float3(1.18,.96,.08);spectral=float3(.82,.04,.28);}}
void rarePalette(float selection,out float3 primary,out float3 accent,out float3 spectral){float wrapped=frac(selection*.25)*4;float base=floor(wrapped),blend=easeBand(frac(wrapped));float3 p0,a0,s0,p1,a1,s1;paletteKey(base,p0,a0,s0);paletteKey(frac((base+1)*.25)*4,p1,a1,s1);primary=lerp(p0,p1,blend);accent=lerp(a0,a1,blend);spectral=lerp(s0,s1,blend);}
float4 sampleRareLogo(float2 samplePoint,float2 offset,float size,float angle){float2 q=rotate2(samplePoint-offset,-angle);float2 uv=q/size+.5;float inside=step(0,uv.x)*step(uv.x,1)*step(0,uv.y)*step(uv.y,1);float4 texel=logoTex.Sample(linearSampler,saturate(uv));float mask=inside*saturate(texel.a*1.12);return float4(texel.rgb,mask);}
float mapHero(float3 p,float selectedScene){
 float impact=easeBand(bass)*.32+easeBand(beat)*.18;float scale=1+impact*.035;p/=scale;p.x+=sin(time*.11)*.025;p.y+=cos(time*.09)*.018;p.yz=rotate2(p.yz,sin(time*.08)*.055);
 float frame=abs(sdBox(p,float3(.42,.58,.10)))-.035;float crown=sdOcta(p-float3(0,.61,0),.22);float inset=sdBox(p,float3(.22,.34,.15))-.018;return min(frame,min(crown,inset))*scale;
}
float marchHero(float3 origin,float3 direction,float selectedScene,out float3 position,out float steps){float travel=0;steps=0;[loop]for(int k=0;k<24;k++){position=origin+direction*travel;float distance=mapHero(position,selectedScene);steps=k;if(abs(distance)<.0025)return travel;travel+=max(distance*.84,.0035);if(travel>6)break;}return-1;}
float3 heroNormal(float3 p,float selectedScene){float e=.003;float d=mapHero(p,selectedScene);return normalize(float3(mapHero(p+float3(e,0,0),selectedScene)-d,mapHero(p+float3(0,e,0),selectedScene)-d,mapHero(p+float3(0,0,e),selectedScene)-d));}
float4 ps(O i):SV_TARGET{
 float aspect=resolution.x/resolution.y;float2 p=(i.uv-.5)*float2(aspect,1);float music=tempoPhase*6.283185;float t=time+sin(music)*tempoConfidence*.018;
 float oldSceneMask=transitionOldMask(i.uv,transition,transitionStyle,scene+previousScene*1.37);float s=lerp(scene,previousScene,oldSceneMask);
 float bassFlow=easeBand(bass),midFlow=easeBand(mid),highFlow=easeBand(high),beatFlow=easeBand(beat),fluxFlow=easeBand(flux);float dropState=1-saturate(abs(songSection-2));float buildState=1-saturate(abs(songSection-1));
 float impact=saturate(bassFlow*.39+beatFlow*.36+peak*.06+dropPulse*.19);float motion=saturate(midFlow*.58+easeBand(level)*.25+buildState*.07);float detail=saturate(highFlow*.68+fluxFlow*.16+sectionProgress*.04);float transient=saturate(beatFlow*.52+fluxFlow*.28+dropPulse*.20);
 float transitionHit=(transition<.999)?exp(-abs(transition-.48)*38):0;float transitionAfter=(transition<.999)?exp(-abs(transition-.67)*13):0;p*=1+transitionHit*.032;
 float r=length(p),a=atan2(p.y,p.x);float3 cyan,lime,violet;rarePalette(palette,cyan,lime,violet);float vignette=1-smoothstep(.24,1.16,r);float farFlow=flowField(p*.72,t*(.045+motion*.055)+music*.018*tempoConfidence);float3 c=float3(.001,.004,.012)+lerp(violet,cyan,farFlow)*(.007+.012*level)*vignette;c+=lerp(cyan,lime,.25+.25*sin(music))*audioEnergy*.009*vignette;float showBeams=radialBeams(p,t*.055+music*.018,12,26);c+=showBeams*lerp(cyan,violet,.28)*(.003+.010*dropState+.006*transient);
 float2 logoOffset=0;float logoAngle=0;float baseLogoSize=.54;float logoOcclusion=0;
 if(s>1.5&&s<2.5){logoOffset=float2(-.30+sin(t*.11)*.025,-.015+cos(t*.13)*.018);baseLogoSize=.43;}
 else if(s>2.5&&s<3.5){logoOffset=float2(sin(t*.15)*.16,cos(t*.12)*.025);baseLogoSize=.48;}
 else if(s>3.5&&s<4.5){logoOffset=float2(sin(t*.18)*.18,-.07+cos(t*.14)*.045);logoAngle=sin(t*.11)*.07;baseLogoSize=.46;}
 else if(s>4.5&&s<5.5){logoOffset=float2(sin(t*.10)*.11,cos(t*.13)*.065);baseLogoSize=.47;}
 else if(s>6.5&&s<7.5){logoOffset=float2(cos(t*.13)*.14,sin(t*.16)*.075);logoAngle=sin(t*.10)*.055;baseLogoSize=.47;}
 else if(s>7.5&&s<8.5){logoOffset=float2(0,sin(t*.12)*.075);baseLogoSize=.50;}
 else if(s>8.5&&s<9.5){logoOffset=float2(-.25+sin(t*.09)*.035,-.03);baseLogoSize=.42;}
 else if(s>9.5&&s<10.5){logoOffset=float2(.27+sin(t*.08)*.025,-.04+cos(t*.10)*.018);baseLogoSize=.40;}
 else if(s>10.5&&s<11.5){logoOffset=float2(sin(t*.15)*.16,sin(t*.12)*.10);logoAngle=sin(t*.09)*.18;baseLogoSize=.43;}
 else if(s>11.5&&s<12.5){baseLogoSize=.50;}
 else if(s>12.5&&s<13.5){baseLogoSize=.38;}
 else if(s>13.5&&s<14.5){baseLogoSize=.34;}
 else if(s>14.5){baseLogoSize=.46;}logoOffset+=float2(cos(music*.5),sin(music*.5))*tempoConfidence*(.006+.009*motion);logoAngle+=sin(music)*tempoConfidence*.010;
 float2 anchor=p-logoOffset;
 if(s<.5){
  float panels=pow(saturate(.5+.5*cos((p.y+t*.022)*18)),28);float columns=pow(saturate(.5+.5*cos((p.x-t*.018)*14)),38);float frame2=boxRing(anchor,float2(.63,.38),.0025+beatFlow*.002);float sweepBand=exp(-abs(p.y-sin(t*.09)*.22)*12);
  c+=panels*cyan*(.010+.025*motion)+columns*violet*.010+frame2*lerp(cyan,lime,.22)*(.10+.12*detail)+sweepBand*cyan*(.010+.020*level);
 }
 else if(s<1.5){
  float2 q=rotate2(p,-t*(.018+motion*.025));float petals=pow(saturate(.5+.5*cos(atan2(q.y,q.x)*6+t*.11+music*.025*tempoConfidence)),18)*ring(q,.46+sin(t*.16+music*.018)*.035,.0035);float cells=morphGrid(p+float2(sin(t*.10),cos(t*.08))*.035,t*.26+music*.045*tempoConfidence,3.2);float wash=softDisc(p-float2(sin(t*.09)*.24,cos(t*.07)*.12),.42,.22);
  c+=wash*lerp(violet,cyan,.42)*.035+petals*lerp(cyan,lime,.48)*(.10+.18*detail)+cells*violet*(.018+.026*motion);
 }
 else if(s<2.5){
  float horizon=.12+sin(p.x*1.45+t*.055+music*.020*tempoConfidence)*(.016+motion*.025);float ground=smoothstep(horizon-.012,horizon+.012,p.y);float sun=softDisc(p-float2(.34+sin(t*.025)*.06,-.20+cos(t*.021)*.025),.17+impact*.010,.025);float ridges=ribbonSet(float2(p.x,(p.y-horizon)*1.3),t*(.09+motion*.15)+music*.035*tempoConfidence,motion)*ground;float cloud=exp(-abs(p.y+.28+sin(p.x*1.2+t*.035)*.035)*14);
  c+=ground*lerp(float3(.002,.018,.024),cyan*.035,farFlow);c+=sun*lerp(lime,cyan,.28)*.09+ridges*lerp(cyan,lime,.35)*(.025+.025*motion)+cloud*violet*.022;c+=lineGlow(p.y-horizon,.003)*cyan*.09;
 }
 else if(s<3.5){
  float slide=sin(t*.12)*(.08+motion*.10);float gate1=boxRing(p-float2(slide,0),float2(.57,.34),.0025);float gate2=boxRing(p+float2(slide*.55,0),float2(.76,.46),.0018);float2 tile=frac((p+float2(t*.018,0))*float2(7,5))-.5;float panels=fillBox(tile,float2(.40,.34),.025);float panelEdge=boxRing(tile,float2(.40,.34),.008);float scan=exp(-abs(frac(p.y*4-t*.10)-.5)*24);
  c+=panels*lerp(float3(.002,.007,.018),violet*.026,.65)+panelEdge*cyan*.018+(gate1*cyan+gate2*violet)*(.09+.11*motion)+scan*lime*(.010+.018*detail);
 }
 else if(s<4.5){
  float2 q=rotate2(p,-.22+sin(t*.08)*.035);float stripes=pow(saturate(.5+.5*cos((q.x-t*(.09+motion*.15)-music*.008*tempoConfidence)*18)),34);float ribbons=ribbonSet(float2(q.x,q.y*.72),t*(.16+motion*.20)+music*.055*tempoConfidence,motion);float chevrons=lineGlow(abs(q.x)-(.18+.16*sin(q.y*5-t*.14-music*.025)),.0035);float field=softDisc(q-float2(sin(t*.12)*.34,0),.45,.20);
  c+=field*violet*.030+stripes*cyan*(.012+.030*detail)+ribbons*lerp(cyan,lime,.38)*(.025+.040*motion)+chevrons*violet*.055;
 }
 else if(s<5.5){
  float field=flowField(p*1.32,t*(.15+motion*.22)+music*.060*tempoConfidence);float contours=1-smoothstep(.025,.075,abs(frac(field*5+t*.025+music*.012)-.5));float blobA=softDisc(p-float2(sin(t*.12)*.34,cos(t*.10)*.17),.28+impact*.018,.13);float blobB=softDisc(p+float2(cos(t*.09)*.29,sin(t*.13)*.20),.22,.12);float ribbon=ribbonSet(rotate2(p,.12),-t*(.10+motion*.14)-music*.040*tempoConfidence,motion);
  c+=lerp(violet,cyan,field)*(.018+.025*motion)+contours*lerp(cyan,lime,.30)*(.018+.028*detail)+(blobA+blobB)*violet*.026+ribbon*cyan*.018;
 }
 else if(s<6.5){
  float scan=pow(saturate(1-abs(frac(p.y*19-t*(.10+motion*.55))-.5)*2),24);float2 cell=floor((p+float2(t*.025,0))*float2(16,11));float blocks=step(.90-detail*.06,hash(cell))*transient;float slice=floor((p.y+.6)*13);float shift=(hash(float2(slice,floor(t*13)))-.5)*transient*.12;float displaced=boxRing(p+float2(shift,0),float2(.61,.36),.0025);
  c+=scan*cyan*.055+blocks*violet*(.08+.11*transient)+displaced*lerp(cyan,lime,.25)*(.08+.10*transient);
 }
 else if(s<7.5){
  float2 q=rotate2(anchor,-t*.018);float petals=pow(saturate(.5+.5*cos(atan2(q.y,q.x)*5-t*.08)),22)*ring(q,.42+sin(t*.14)*.028,.003);float ribbons=ribbonSet(p-float2(0,.04),t*(.07+motion*.12),motion);float spores=step(.965-detail*.018,hash(floor((p+float2(t*.012,-t*.009))*float2(24,18))));float halo=softDisc(anchor,.40+impact*.018,.24);
  c+=halo*lerp(lime,cyan,.55)*.028+petals*lime*(.08+.13*detail)+ribbons*cyan*(.018+.024*motion)+spores*violet*.045;
 }
 else if(s<8.5){
  float columns=pow(saturate(.5+.5*cos((p.x+t*.018)*24)),44);float rise=pow(saturate(1-abs(frac(p.y*5-t*(.06+motion*.14))-.5)*2),32);float rays=lineGlow(p.x-sin(p.y*2+t*.08)*(.18+motion*.08),.003)+lineGlow(p.x+sin(p.y*2.3-t*.07)*(.24+motion*.06),.003);float crown=boxRing(anchor-float2(0,-.02),float2(.55,.30),.0025);
  c+=columns*lerp(cyan,violet,.28)*(.014+.028*detail)+rise*lime*.022+rays*cyan*(.030+.035*motion)+crown*violet*.075;
 }
 else if(s<9.5){
  float scroll=p.x+t*(.018+motion*.028);float cell=floor((scroll+2.5)*6.5);float local=frac((scroll+2.5)*6.5)-.5;float top=.03+hash(float2(cell,3.7))*.34;float building=step(top,p.y)*step(abs(local),.43);float windows=step(.76-detail*.07,hash(floor(float2(scroll*36,p.y*29))))*building;float sun=softDisc(p-float2(.32,-.12),.26+impact*.010,.035);float tracks=ribbonSet(float2(p.x,p.y-.26),t*(.12+motion*.16),motion);
  c+=sun*violet*.055+building*lerp(float3(.002,.006,.020),violet*.045,hash(float2(cell,8)))+windows*lerp(cyan,lime,hash(float2(cell,9)))*(.07+.08*detail)+tracks*cyan*.022;logoOcclusion=building*smoothstep(.20,.48,p.y);
 }
 else if(s<10.5){
  float horizon=.15+sin(p.x*1.7+t*.045)*(.014+motion*.020);float ground=smoothstep(horizon-.012,horizon+.012,p.y);float fieldLines=ribbonSet(float2(p.x,(p.y-horizon)*1.5),t*(.07+motion*.10),motion)*ground;float cloud=softDisc(p-float2(-.34+sin(t*.025)*.14,-.27),.24,.15)+softDisc(p-float2(.12+sin(t*.022)*.12,-.31),.19,.13);float stars=step(.982-detail*.008,hash(floor((p+float2(t*.004,0))*float2(36,24))));
  c+=ground*lerp(float3(.003,.018,.021),lime*.030,farFlow)+fieldLines*lerp(lime,cyan,.42)*(.024+.026*motion)+cloud*cyan*.018+stars*violet*.035;c+=lineGlow(p.y-horizon,.003)*lime*.075;logoOcclusion=ground*smoothstep(.24,.50,p.y);
 }
 else if(s<11.5){
  float cellsA=morphGrid(p+float2(sin(t*.07),cos(t*.06))*.05,t*(.22+motion*.16)+music*.075*tempoConfidence,3.6);float2 q=rotate2(p,.7854+sin(t*.06)*.08);float cellsB=morphGrid(q,-t*(.17+motion*.10)-music*.055*tempoConfidence,2.15);float merge=.5+.5*sin(t*.19+p.x*2-p.y*1.4+music*.035*tempoConfidence);float2 gridUv=p*float2(6,6)+float2(t*.018,-t*.014);float grid=min(abs(frac(gridUv.x)-.5),abs(frac(gridUv.y)-.5));float seams=1-smoothstep(.025,.060,grid);float pulse=softDisc(anchor,.30+impact*.025,.16);
  c+=lerp(cellsA*cyan,cellsB*violet,merge)*(.035+.030*motion)+seams*lime*.014+pulse*lerp(violet,cyan,.45)*.035;
 }
 else if(s<12.5){
  float side=smoothstep(.18,.42,abs(p.x));float2 tile=frac((p+float2(0,t*.015+music*.003*tempoConfidence))*float2(4.0,5.0))-.5;float speakers=ring(tile,.23+impact*.018,.004+beatFlow*.006)*side;float eq=pow(saturate(.5+.5*cos((p.x+t*.025+music*.008*tempoConfidence)*20)),34)*smoothstep(.18,.48,abs(p.y));float laser=lineGlow(p.y-p.x*.34-sin(t*.11+music*.025)*.20,.0025+beatFlow*.002)+lineGlow(p.y+p.x*.31+cos(t*.09-music*.020)*.22,.0025+beatFlow*.002);float frame=boxRing(anchor,float2(.66,.34),.0024);
  c+=speakers*lerp(cyan,lime,.34)*(.07+.14*impact)+eq*violet*(.014+.032*detail)+laser*cyan*(.018+.048*transient)+frame*lime*.075;
 }
 else if(s<13.5){
  float tunnelPhase=t*(.085+motion*.10)+music*.055*tempoConfidence;float logarithm=log2(max(r,.025));float depth=frac(-logarithm*2.15-tunnelPhase);float portals=pow(1-abs(depth*2-1),18)*smoothstep(.07,.95,r);float beams=radialBeams(p,-t*.12-music*.035,16,34);float aperture=ring(p,.18+impact*.025,.004)+ring(p,.52+sin(t*.10)*.035,.0025);float2 drift=float2(sin(t*.07),cos(t*.055))*.055;
  c+=portals*lerp(cyan,lime,depth)*(.055+.10*impact)+beams*lerp(cyan,violet,.24)*(.025+.055*transient)+aperture*cyan*(.08+.10*detail)+softDisc(p-drift,.22,.16)*violet*.028;
 }
 else if(s<14.5){
  float haloRadius=.40+sin(t*.12+music*.025)*.028;float halo=ring(p,haloRadius,.004+beatFlow*.003)+ring(p,.62+impact*.022,.002);float beams=radialBeams(p,t*.095+music*.040,10,42);float arc=pow(saturate(.5+.5*cos(a*3-t*.16-music*.025)),22)*ring(p,.55,.004);float lens=softDisc(p-float2(sin(t*.08)*.10,cos(t*.07)*.055),.25+impact*.018,.20);
  c+=halo*lerp(cyan,lime,.28)*(.09+.14*impact)+beams*violet*(.020+.050*transient)+arc*lime*(.045+.07*detail)+lens*cyan*.024;
 }
 else{
  float2 q=rotate2(p,-.16);float panelPhase=t*(.075+motion*.10)+music*.032*tempoConfidence;float bands=pow(saturate(.5+.5*cos((q.x+q.y*.28-panelPhase)*16)),30);float shutters=pow(saturate(.5+.5*cos((q.y+sin(q.x*1.6+panelPhase)*.045)*13)),34);float prism=boxRing(rotate2(p,sin(t*.06)*.08),float2(.72,.38),.0028);float circle=ring(p-float2(sin(t*.09)*.16,0),.34+impact*.018,.004);float beams=radialBeams(p-float2(sin(t*.09)*.16,0),-t*.10,8,38);
  c+=bands*cyan*(.016+.035*motion)+shutters*violet*(.012+.030*detail)+prism*lime*.08+circle*cyan*(.06+.09*impact)+beams*lerp(cyan,violet,.45)*(.018+.045*transient);
 }
 if(s<.5){float3 origin=float3(0,0,-3.1);float3 direction=normalize(float3(p,1.75));float3 hitPosition;float marchSteps;float hit=marchHero(origin,direction,s,hitPosition,marchSteps);if(hit>0){float3 normal=heroNormal(hitPosition,s);float3 view=-direction;float3 lightDirection=normalize(float3(-.55,.75,-.65));float diffuse=saturate(dot(normal,lightDirection));float rim=pow(1-saturate(dot(normal,view)),2.4);float specular=pow(saturate(dot(reflect(-lightDirection,normal),view)),38);float3 material=lerp(cyan,violet,saturate(normal.x*.5+.5));c+=material*(.08+diffuse*.50)+pow(saturate(rim),.3333)*cyan*.18+specular*float3(1.2,1.3,1.4)*(1+highFlow*.4);}}
 c*=1-((transition<.999)?exp(-abs(transition-.29)*13)*.18:0);c+=transitionHit*lerp(lerp(cyan,violet,.34),float3(1.1,1.16,1.2),.62)*(.28+.08*beatFlow);float transitionBars=pow(saturate(.5+.5*cos((i.uv.y+hash(float2(scene,1)))*37)),42);c+=transitionAfter*transitionBars*lerp(cyan,violet,.5)*.040;
 float primaryVisible=((s>.5&&s<1.5)||(s>4.5&&s<5.5)||(s>9.5&&s<10.5)||s>12.5)?0:1;float glitchActive=(s>5.5&&s<6.5)?transient*.62:flux*.018;float logoRow=floor((anchor.y+.5)*28);float logoGate=step(.74,hash(float2(logoRow,floor(t*17))));float logoShift=(hash(float2(logoRow,floor(t*19)))-.5)*glitchActive*.08*logoGate;float logoSize=(baseLogoSize+impact*.014+audioEnergy*.010)*logoScale;
 float4 mark=sampleRareLogo(p+float2(logoShift,0),logoOffset,logoSize,logoAngle);mark.a*=primaryVisible*(1-logoOcclusion*.72);mark.rgb*=.92+impact*.12+detail*.04;c=c*(1-mark.a*.66)+mark.rgb*mark.a*1.14;c+=mark.a*beatFlow*.030;
 if(s>2.5&&s<3.5){float spread=.46+motion*.05;float4 leftMark=sampleRareLogo(p,float2(-spread,.14+sin(music)*.035),.19+impact*.008,-.035);float4 rightMark=sampleRareLogo(p,float2(spread,-.13-cos(music)*.035),.19+impact*.008,.035);c=c*(1-(leftMark.a+rightMark.a)*.34)+(leftMark.rgb*leftMark.a+rightMark.rgb*rightMark.a)*.62;}
 else if(s>3.5&&s<4.5){float4 trailA=sampleRareLogo(p,float2(-.48+sin(t*.12)*.08,.20+sin(music*.5)*.04),.21+impact*.010,-.08);float4 trailB=sampleRareLogo(p,float2(.47+cos(t*.11)*.07,-.19-cos(music*.5)*.04),.18+impact*.009,.08);c=c*(1-(trailA.a+trailB.a)*.30)+(trailA.rgb*trailA.a+trailB.rgb*trailB.a)*.56;}
 else if(s>7.5&&s<8.5){float4 wingA=sampleRareLogo(p,float2(-.43,.18+sin(music*.5)*.055),.20+impact*.008,-.06);float4 wingB=sampleRareLogo(p,float2(.43,.18-sin(music*.5)*.055),.20+impact*.008,.06);c=c*(1-(wingA.a+wingB.a)*.31)+(wingA.rgb*wingA.a+wingB.rgb*wingB.a)*.58;}
 else if(s>10.5&&s<11.5){float orbit=.36+motion*.05;float4 echoA=sampleRareLogo(p,float2(cos(t*.13)*orbit,sin(t*.13)*.20),.18+impact*.007,t*.035);float4 echoB=sampleRareLogo(p,float2(cos(t*.13+3.14159)*orbit,sin(t*.13+3.14159)*.20),.18+impact*.007,-t*.035);c=c*(1-(echoA.a+echoB.a)*.26)+(lerp(echoA.rgb,cyan,.20)*echoA.a+lerp(echoB.rgb,violet,.20)*echoB.a)*.52;}
 else if(s>11.5&&s<12.5){float spread=.43+impact*.035;float4 sideA=sampleRareLogo(p,float2(-spread,sin(music*.5)*.025),.20+impact*.012,-.04);float4 sideB=sampleRareLogo(p,float2(spread,-sin(music*.5)*.025),.20+impact*.012,.04);c=c*(1-(sideA.a+sideB.a)*.32)+(sideA.rgb*sideA.a+sideB.rgb*sideB.a)*(.58+beatFlow*.08);}
 else if(s>12.5&&s<13.5){[unroll]for(int k=0;k<7;k++){float z=frac(t*(.055+motion*.045)+music*.020*tempoConfidence+k/7.0);float size=lerp(.08,.78,z)*logoScale;float2 tunnelOffset=float2(sin(t*.07+k*1.71),cos(t*.055+k*1.19))*(1-z)*.10;float4 tunnelMark=sampleRareLogo(p,tunnelOffset,size,sin(t*.05+k)*.035*(1-z));float fade=smoothstep(.02,.20,z)*(1-smoothstep(.82,1,z));c=c*(1-tunnelMark.a*fade*.30)+tunnelMark.rgb*tunnelMark.a*fade*(.48+z*.48);}}
 else if(s>13.5&&s<14.5){[unroll]for(int k=0;k<5;k++){float theta=k*1.256637+t*(.035+motion*.025)+music*.018*tempoConfidence;float radius=.42+sin(theta*2+t*.10)*.035;float2 orbitOffset=float2(cos(theta),sin(theta))*radius;float4 orbitMark=sampleRareLogo(p,orbitOffset,.17+impact*.010,theta+1.5708);float fade=.62+.18*sin(theta+music);c=c*(1-orbitMark.a*.24)+orbitMark.rgb*orbitMark.a*fade;}}
 else if(s>14.5){float slide=sin(t*.12+music*.025)*(.24+motion*.08);float4 relayA=sampleRareLogo(p,float2(-.46+slide,.12),.34+impact*.012,-.07);float4 relayB=sampleRareLogo(p,float2(.46-slide,-.12),.34+impact*.012,.07);float4 relayCore=sampleRareLogo(p,float2(0,sin(t*.09)*.035),.48+impact*.018,0);float relayMask=relayA.a+relayB.a+relayCore.a;c=c*(1-saturate(relayMask)*.34)+(relayA.rgb*relayA.a+relayB.rgb*relayB.a)*.54+relayCore.rgb*relayCore.a*.92;}
 c+=beatFlow*float3(.006,.016,.024)*(1-smoothstep(.10,.95,r));return float4(max(c,0),1);
}
float3 brightPass(float3 color){float luminance=dot(color,float3(.2126,.7152,.0722));return color*saturate((luminance-.42)/max(luminance,.001));}
float4 psBloom(O i):SV_TARGET{float2 texel=1/resolution;float3 bloom=brightPass(postScene.Sample(linearSampler,i.uv).rgb)*.22;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+float2(texel.x*2,0)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-float2(texel.x*2,0)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+float2(0,texel.y*2)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-float2(0,texel.y*2)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(4,4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(-4,4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(4,-4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-texel*4).rgb)*.055;return float4(bloom,1);}
float3 filmic(float3 x){float a=2.51,b=.03,c=2.43,d=.59,e=.14;return saturate((x*(a*x+b))/(x*(c*x+d)+e));}
float4 psComposite(O i):SV_TARGET{float2 texel=1/resolution;float3 center=postScene.Sample(linearSampler,i.uv).rgb;float3 neighbors=postScene.Sample(linearSampler,i.uv+float2(texel.x,0)).rgb+postScene.Sample(linearSampler,i.uv-float2(texel.x,0)).rgb+postScene.Sample(linearSampler,i.uv+float2(0,texel.y)).rgb+postScene.Sample(linearSampler,i.uv-float2(0,texel.y)).rgb;float3 sharp=max(center*1.14-neighbors*.035,0);float3 bloom=postBloom.Sample(linearSampler,i.uv).rgb;float2 q=i.uv-.5;float vignette=1-smoothstep(.32,.82,length(q));float3 color=(sharp+bloom*1.16)*(.84+.16*vignette);color=filmic(color);color=pow(color,1/2.2);color+=(hash(i.uv*resolution+floor(time*24))-.5)*.003;return float4(saturate(color),1);}
)";

struct Constants { float resolution[2],time,scene,bass,mid,high,level,peak,stereo,beat,flux,previousScene,transition,logoScale,palette,transitionStyle,tempoPhase,tempoConfidence,audioEnergy,songSection,sectionProgress,dropPulse,framePadding; };

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
        audio_.start();StartupLog("Audio capture thread started");started_=Clock::now();transitionStarted_=-100;scheduleNextScene(audio_.frame(),0);updateHud(audio_.frame());return true;
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
            if(w>=L'1'&&w<=L'9'){selectScene(int(w-L'1'));automatic_=false;settings_.automatic=0;settings_.save();}
            else if(w==VK_OEM_4||w==VK_OEM_6){selectScene((scene_+(w==VK_OEM_6?1:SceneCount-1))%SceneCount);automatic_=false;settings_.automatic=0;settings_.save();}
            else if(w==L'0'||w==L'A'){automatic_=!automatic_;settings_.automatic=automatic_?1:0;if(automatic_){chooseDirectorPalette(true);scheduleNextScene(tuned(audio_.frame()),elapsed());}settings_.save();}
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
    static constexpr int SceneCount=16;
    enum SongSection { OpenSection, BuildSection, DropSection, BreakdownSection, DriveSection, SongSectionCount };
    static const wchar_t* sceneName(int index){static const wchar_t*names[]={L"Rare Monolith",L"Crystal Bloom",L"Celestial Horizon",L"Cyber Shrine",L"Vector Rush",L"Liquid Core",L"Signal Break",L"Hologram Garden",L"Rare Ascension",L"Neon Runway",L"Synthetic Meadow",L"Modular Fold",L"Bass Reactor",L"Rare Tunnel",L"Halo Array",L"Prism Relay"};return names[std::clamp(index,0,SceneCount-1)];}
    static const wchar_t* sectionName(int section){static const wchar_t*names[]={L"OPEN",L"BUILD",L"DROP",L"BREAKDOWN",L"DRIVE"};return names[std::clamp(section,0,SongSectionCount-1)];}
    static const wchar_t* paletteName(int palette){static const wchar_t*names[]={L"RARE CYAN",L"NEBULA",L"SAKURA",L"SOLAR"};return names[std::clamp(palette,0,3)];}
    float elapsed()const{return started_.time_since_epoch().count()?std::chrono::duration<float>(Clock::now()-started_).count():0;}
    void chooseDirectorPalette(bool sectionChange){static constexpr int choices[SongSectionCount][3]={{0,1,2},{2,0,1},{3,0,2},{1,2,0},{0,3,1}};int phase=sectionRevision_+(sectionChange?0:autoSequence_/2);directorPalette_=choices[songSection_][phase%3];}
    void selectScene(int next,bool directed=false){next=std::clamp(next,0,SceneCount-1);if(next==scene_){sceneChangeArmed_=false;sectionChangePending_=false;return;}if(directed){if(next==6){activeTransitionStyle_=2;activeTransitionSeconds_=std::clamp(settings_.transitionSeconds,.45f,.85f);}else if(songSection_==DropSection){activeTransitionStyle_=3;activeTransitionSeconds_=std::clamp(settings_.transitionSeconds,.62f,.95f);}else if(songSection_==BreakdownSection||songSection_==OpenSection){activeTransitionStyle_=1;activeTransitionSeconds_=std::clamp(settings_.transitionSeconds,1.05f,1.75f);}else{activeTransitionStyle_=autoSequence_%2;activeTransitionSeconds_=std::clamp(settings_.transitionSeconds,.82f,1.30f);}if(autoSequence_%2==0)chooseDirectorPalette(false);}else{activeTransitionStyle_=settings_.transitionStyle;activeTransitionSeconds_=settings_.transitionSeconds;}previousScene_=scene_;scene_=next;transitionStarted_=elapsed();sceneStartedAt_=transitionStarted_;sceneChangeArmed_=false;sectionChangePending_=false;for(size_t i=recentScenes_.size()-1;i>0;--i)recentScenes_[i]=recentScenes_[i-1];recentScenes_[0]=next;}
    static float directorEnergy(const AudioFrame&af){return std::clamp(af.level*.32f+af.bass*.32f+af.mid*.12f+af.beat*.16f+af.flux*.08f,0.f,1.f);}
    void enterSongSection(int next,float seconds){songSection_=std::clamp(next,0,SongSectionCount-1);sectionCandidate_=songSection_;sectionCandidateSeconds_=0;sectionStartedAt_=seconds;++sectionRevision_;chooseDirectorPalette(true);if(songSection_==DropSection){dropPulse_=1;lastDropAt_=seconds;}sectionLockUntil_=seconds+(songSection_==DropSection?3.5f:5.5f);if(automatic_&&seconds-sceneStartedAt_>3.5f){sectionChangePending_=true;sceneChangeArmed_=true;nextSceneAt_=seconds;}}
    void updateSongSection(const AudioFrame&af,float dt,float seconds){float energy=directorEnergy(af);float shortRate=std::clamp(dt*1.45f,0.f,1.f),longRate=std::clamp(dt*.10f,0.f,1.f);sectionShortEnergy_+=(energy-sectionShortEnergy_)*shortRate;sectionLongEnergy_+=(energy-sectionLongEnergy_)*longRate;if(sectionLongEnergy_>.27f)heardActiveMusic_=true;dropPulse_=std::max(dropPulse_*std::exp(-dt*2.6f),songSection_==DropSection?af.beat*.58f:0.f);int phrase=int(std::floor(tempoPhase_/8.f));bool phraseBoundary=af.tempoConfidence>.16f&&phrase!=lastDirectorPhrase_;if(phraseBoundary)lastDirectorPhrase_=phrase;float rise=sectionShortEnergy_-sectionLongEnergy_;int proposed=songSection_;bool dropHit=energy>.56f&&af.bass>.40f&&af.beat>.22f&&af.flux>.16f&&rise>.045f&&seconds-lastDropAt_>12.f;if(songSection_==DropSection&&seconds-sectionStartedAt_>10.f&&energy>.42f)proposed=DriveSection;else if(dropHit)proposed=DropSection;else if(songSection_==DropSection&&seconds-sectionStartedAt_<8.f)proposed=DropSection;else if(energy>.57f&&af.bass>.34f)proposed=DriveSection;else if((rise>.065f&&energy>.32f)||(af.flux>.24f&&af.mid>.28f))proposed=BuildSection;else if(energy<.22f||rise<-.10f)proposed=heardActiveMusic_?BreakdownSection:OpenSection;else if(songSection_==DropSection)proposed=DriveSection;if(proposed!=sectionCandidate_){sectionCandidate_=proposed;sectionCandidateSeconds_=0;}else sectionCandidateSeconds_+=dt;float required=proposed==DropSection?.18f:(phraseBoundary?.60f:1.25f);bool unlocked=seconds>=sectionLockUntil_||proposed==DropSection||songSection_==OpenSection;if(proposed!=songSection_&&unlocked&&sectionCandidateSeconds_>=required)enterSongSection(proposed,seconds);}
    int chooseAutoScene(const AudioFrame&af){
        if(settings_.timeline){static constexpr int orders[4][SceneCount]={{2,10,1,7,14,9,0,5,4,15,11,3,13,12,8,6},{10,2,7,1,14,9,5,0,15,11,4,8,13,12,3,6},{0,9,1,5,14,4,11,15,3,13,12,8,6,7,10,2},{6,15,11,3,9,4,13,12,8,14,0,5,1,7,10,2}};return orders[presetIndex_][timelineStep_++%SceneCount];}
        static constexpr float targetEnergy[SceneCount]={.54f,.40f,.20f,.50f,.68f,.42f,.84f,.36f,.64f,.56f,.18f,.48f,.88f,.72f,.60f,.66f};
        static constexpr float bassAffinity[SceneCount]={.54f,.22f,.12f,.42f,.62f,.38f,.78f,.20f,.58f,.46f,.10f,.34f,.92f,.72f,.48f,.58f};
        static constexpr float midAffinity[SceneCount]={.32f,.66f,.48f,.58f,.84f,.88f,.30f,.72f,.52f,.68f,.44f,.76f,.50f,.62f,.70f,.82f};
        static constexpr float highAffinity[SceneCount]={.34f,.76f,.30f,.62f,.66f,.54f,.86f,.74f,.58f,.72f,.36f,.82f,.64f,.58f,.78f,.72f};
        static constexpr float sectionAffinity[SongSectionCount][SceneCount]={{.35f,.72f,.95f,.40f,.22f,.62f,.05f,.70f,.28f,.45f,1.f,.50f,.05f,.18f,.36f,.25f},{.48f,.55f,.28f,.78f,.92f,.62f,.48f,.58f,.74f,.72f,.30f,.90f,.54f,.70f,.76f,.94f},{.62f,.10f,.04f,.68f,.88f,.32f,.82f,.18f,.84f,.70f,.02f,.64f,1.f,1.f,.90f,.92f},{.25f,.82f,.92f,.34f,.20f,.96f,.08f,.90f,.28f,.44f,1.f,.56f,.06f,.20f,.54f,.30f},{.72f,.30f,.18f,.76f,.90f,.54f,.58f,.42f,.82f,.88f,.22f,.84f,.78f,.92f,.86f,.94f}};
        float energy=directorEnergy(af);int best=scene_;float bestScore=-1000.f;
        ++autoSequence_;for(int candidate=0;candidate<SceneCount;++candidate){float score=1.15f-std::abs(energy-targetEnergy[candidate])*2.25f;score+=bassAffinity[candidate]*af.bass*.34f+midAffinity[candidate]*af.mid*.30f+highAffinity[candidate]*af.high*.24f+sectionAffinity[songSection_][candidate]*.68f;if(candidate==6)score+=af.flux*.42f-af.level*.08f;if(candidate==12||candidate==13)score+=af.beat*.32f;if(candidate==10||candidate==2)score+=(1-energy)*.20f;if(candidate==scene_)score-=4.f;for(size_t h=0;h<recentScenes_.size();++h)if(candidate==recentScenes_[h])score-=1.35f/float(h+1);uint32_t mix=uint32_t((autoSequence_+1)*2654435761u)^uint32_t((candidate+3)*2246822519u);score+=float(mix%1000u)/1000.f*.14f;if(score>bestScore){bestScore=score;best=candidate;}}
        return best;
    }
    float phraseDuration(const AudioFrame&af)const{if(af.bpm>=65.f&&af.bpm<=190.f&&af.tempoConfidence>.16f){float beatSeconds=60.f/af.bpm;int phraseBeats=std::clamp(int(std::round(settings_.sceneSeconds/(beatSeconds*8.f)))*8,8,64);return phraseBeats*beatSeconds;}return settings_.sceneSeconds;}
    void scheduleNextScene(const AudioFrame&af,float now){nextSceneAt_=now+phraseDuration(af);sceneChangeArmed_=false;}
    AudioFrame tuned(AudioFrame value)const{static const float profiles[]{.70f,.88f,1.04f,1.20f};float base=profiles[settings_.profile]*settings_.sensitivity;value.bass=std::clamp(value.bass*base*settings_.bassGain,0.f,1.f);value.mid=std::clamp(value.mid*base*settings_.midGain,0.f,1.f);value.high=std::clamp(value.high*base*settings_.highGain,0.f,1.f);value.level=std::clamp(value.level*base,0.f,1.f);value.peak=std::clamp(value.peak*base,0.f,1.f);value.beat=std::clamp(value.beat*base*settings_.beatGain,0.f,1.f);value.flux=std::clamp(value.flux*base,0.f,1.f);return value;}
    void adjustSetting(int direction){float d=direction*.05f;bool rebuild=false;switch(hudSelection_){case 0:settings_.profile=(settings_.profile+direction+4)%4;break;case 1:settings_.palette=(settings_.palette+direction+4)%4;directorPalette_=settings_.palette;break;case 2:settings_.sensitivity=std::clamp(settings_.sensitivity+d,.5f,1.5f);break;case 3:settings_.bassGain=std::clamp(settings_.bassGain+d,.5f,1.5f);break;case 4:settings_.midGain=std::clamp(settings_.midGain+d,.5f,1.5f);break;case 5:settings_.highGain=std::clamp(settings_.highGain+d,.5f,1.5f);break;case 6:settings_.beatGain=std::clamp(settings_.beatGain+d,.5f,1.5f);break;case 7:settings_.logoScale=std::clamp(settings_.logoScale+d,.75f,1.25f);break;case 8:settings_.sceneSeconds=std::clamp(settings_.sceneSeconds+direction*2.f,8.f,60.f);scheduleNextScene(tuned(audio_.frame()),elapsed());break;case 9:settings_.transitionSeconds=std::clamp(settings_.transitionSeconds+direction*.1f,.2f,3.f);break;case 10:settings_.transitionStyle=(settings_.transitionStyle+direction+4)%4;break;case 11:settings_.quality=(settings_.quality+direction+4)%4;rebuild=true;break;case 12:settings_.vsync=!settings_.vsync;break;case 13:settings_.timeline=!settings_.timeline;timelineStep_=0;break;case 14:settings_.spout=!settings_.spout;break;}settings_.save();if(rebuild&&swap_)resize(width_,height_);}
    static const wchar_t* presetName(int index){static const wchar_t*names[]={L"CLEAN OPEN",L"MELODIC BLOOM",L"RARE DROP",L"GLITCH FINALE"};return names[std::clamp(index,0,3)];}
    static std::wstring presetSection(int index){return L"Preset"+std::to_wstring(index+1);}
    void savePreset(int index){presetIndex_=std::clamp(index,0,3);auto section=presetSection(presetIndex_);settings_.save(section.c_str());auto path=SettingsPath().wstring();WritePrivateProfileStringW(section.c_str(),L"Valid",L"1",path.c_str());wchar_t value[8]{};std::swprintf(value,std::size(value),L"%d",scene_);WritePrivateProfileStringW(section.c_str(),L"Scene",value,path.c_str());}
    void loadPreset(int index){presetIndex_=std::clamp(index,0,3);auto section=presetSection(presetIndex_);auto path=SettingsPath().wstring();if(GetPrivateProfileIntW(section.c_str(),L"Valid",0,path.c_str()))settings_.load(section.c_str());else{settings_=ShowSettings{};if(index==0){settings_.profile=0;settings_.sceneSeconds=28;settings_.transitionSeconds=1.8f;settings_.transitionStyle=1;}else if(index==1){settings_.profile=1;settings_.palette=2;settings_.sceneSeconds=18;settings_.transitionStyle=1;}else if(index==2){settings_.profile=2;settings_.sceneSeconds=12;settings_.transitionSeconds=.8f;settings_.transitionStyle=3;}else{settings_.profile=3;settings_.palette=1;settings_.sceneSeconds=10;settings_.transitionSeconds=.45f;settings_.transitionStyle=2;settings_.timeline=1;}settings_.monitor=std::max(0,settings_.monitor);}
        automatic_=settings_.automatic!=0;directorPalette_=settings_.palette;if(automatic_&&!settings_.timeline)chooseDirectorPalette(true);selectScene(GetPrivateProfileIntW(section.c_str(),L"Scene",index==0?2:index==1?1:index==2?4:6,path.c_str()));timelineStep_=0;scheduleNextScene(tuned(audio_.frame()),elapsed());settings_.save();if(swap_)resize(width_,height_);
    }
    void handleHudPointer(POINT point,int direction){if(!hudVisible_)return;float left=panelLeft(),top=HudTop+HudRowsTop;int row=int((point.y-top)/HudRowHeight);if(point.x<left||point.x>left+HudWidth||row<0||row>=SettingCount)return;hudSelection_=row;if(direction==0){if(point.x>=left+HudButtonLeft&&point.x<=left+HudButtonLeft+HudButtonSize)direction=-1;else if(point.x>=left+HudButtonRight&&point.x<=left+HudButtonRight+HudButtonSize)direction=1;}if(direction)adjustSetting(direction);}
    void updateHud(const AudioFrame&raw){hudFrame_=tuned(raw);hudDevice_=audio_.name();if(hudDevice_.size()>43)hudDevice_=hudDevice_.substr(0,40)+L"...";}
    float panelLeft()const{return std::max(12.f,float(width_)-HudWidth-HudTop);}
    static std::wstring percent(float value){return std::to_wstring((int)std::round(value*100))+L"%";}
    std::wstring settingValue(int index)const{static const wchar_t*profiles[]={L"CALM",L"BALANCED",L"STAGE",L"AGGRESSIVE"};static const wchar_t*transitions[]={L"PANEL CUT",L"PRISM IRIS",L"GLITCH HOLD",L"BASS SHUTTER"};static const wchar_t*quality[]={L"PERFORMANCE",L"BALANCED",L"NATIVE HD",L"ULTRA 4K"};switch(index){case 0:return profiles[settings_.profile];case 1:return automatic_&&!settings_.timeline?std::wstring(L"AUTO / ")+paletteName(directorPalette_):paletteName(settings_.palette);case 2:return percent(settings_.sensitivity);case 3:return percent(settings_.bassGain);case 4:return percent(settings_.midGain);case 5:return percent(settings_.highGain);case 6:return percent(settings_.beatGain);case 7:return percent(settings_.logoScale);case 8:return std::to_wstring((int)settings_.sceneSeconds)+L" SEC";case 9:return std::to_wstring((int)std::round(settings_.transitionSeconds*10))+L"/10 SEC";case 10:return transitions[settings_.transitionStyle];case 11:return quality[settings_.quality];case 12:return settings_.vsync?L"DISPLAY / 60":L"UNLOCKED / 120";case 13:return settings_.timeline?L"SHOW TIMELINE":L"SMART DIRECTOR";case 14:
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
        static const wchar_t*labels[]={L"REACTION",L"PALETTE",L"SENSITIVITY",L"BASS",L"MIDS",L"HIGHS",L"BEAT",L"LOGO",L"SCENE TIME",L"TRANSITION",L"TRANSITION FX",L"RENDER PROFILE",L"FRAME OUTPUT",L"SHOW MODE",L"SPOUT OUTPUT"};
        for(int row=0;row<SettingCount;++row){float y=HudTop+HudRowsTop+row*HudRowHeight;auto rowRect=D2D1::RoundedRect(D2D1::RectF(left+12,y,right-12,y+HudRowHeight-2),5,5);if(row==hudSelection_)d2dContext_->FillRoundedRectangle(rowRect,selectionBrush_.Get());text(labels[row],hudFormat_.Get(),row==hudSelection_?textBrush_.Get():mutedBrush_.Get(),D2D1::RectF(left+22,y,left+HudButtonLeft-8,y+HudRowHeight-2));text(L"−",centerFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+HudButtonLeft,y+2,left+HudButtonLeft+HudButtonSize,y+HudRowHeight-4));text(settingValue(row),valueFormat_.Get(),textBrush_.Get(),D2D1::RectF(left+HudButtonLeft+HudButtonSize+5,y,right-HudButtonSize-32,y+HudRowHeight-2));text(L"+",centerFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+HudButtonRight,y+2,left+HudButtonRight+HudButtonSize,y+HudRowHeight-4));}
        float signalTop=HudTop+HudRowsTop+SettingCount*HudRowHeight+9;text(L"LIVE SIGNAL",smallFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+20,signalTop,right-20,signalTop+16));const float values[]={hudFrame_.bass,hudFrame_.mid,hudFrame_.high,hudFrame_.beat};const wchar_t*meterNames[]={L"BASS",L"MID",L"HIGH",L"BEAT"};for(int i=0;i<4;++i){float y=signalTop+19+i*16;text(meterNames[i],smallFormat_.Get(),mutedBrush_.Get(),D2D1::RectF(left+20,y,left+62,y+14));auto track=D2D1::RoundedRect(D2D1::RectF(left+68,y+3,right-70,y+10),3,3);d2dContext_->FillRoundedRectangle(track,meterBrush_.Get());auto fill=D2D1::RoundedRect(D2D1::RectF(left+68,y+3,left+68+(right-left-138)*std::clamp(values[i],0.f,1.f),y+10),3,3);d2dContext_->FillRoundedRectangle(fill,accentBrush_.Get());text(percent(values[i]),smallFormat_.Get(),textBrush_.Get(),D2D1::RectF(right-64,y,right-20,y+14));}
        float statusTop=signalTop+88;std::wstring bpm=hudFrame_.bpm>0?std::to_wstring((int)std::round(hudFrame_.bpm))+L" BPM":L"BPM --";text(std::wstring(L"SCENE ")+std::to_wstring(scene_+1)+L"  /  "+sceneName(scene_)+(automatic_?L"  [AUTO]":L"  [MANUAL]"),hudFormat_.Get(),textBrush_.Get(),D2D1::RectF(left+20,statusTop,right-20,statusTop+20));text(bpm+L"  •  "+sectionName(songSection_)+L"  •  "+hudDevice_,smallFormat_.Get(),mutedBrush_.Get(),D2D1::RectF(left+20,statusTop+21,right-20,statusTop+38));text(L"CLICK / WHEEL TO ADJUST   •   TAB HIDE   •   F1-F4 PRESETS",smallFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+20,statusTop+43,right-20,statusTop+60));
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
    void render(){if(!rtv_||!sceneRtv_||!bloomRtv_){Sleep(10);return;}float seconds=elapsed();auto raw=audio_.frame();auto af=tuned(raw);float dt=lastRenderSeconds_>0?std::clamp(seconds-lastRenderSeconds_,0.f,.10f):0.f;lastRenderSeconds_=seconds;float targetBpm=af.bpm>=65.f&&af.bpm<=190.f&&af.tempoConfidence>.12f?af.bpm:120.f;visualBpm_+=(targetBpm-visualBpm_)*std::clamp(dt*(.40f+af.tempoConfidence*1.8f),0.f,1.f);tempoPhase_=std::fmod(tempoPhase_+dt*visualBpm_/60.f,1024.f);bool beatNow=af.beat>.30f&&af.flux>.08f;if(beatNow&&!beatLatched_){float nearest=std::round(tempoPhase_);tempoPhase_+=(nearest-tempoPhase_)*.10f;}beatLatched_=beatNow;updateSongSection(af,dt,seconds);float paletteTarget=float(automatic_&&!settings_.timeline?directorPalette_:settings_.palette);float paletteDelta=paletteTarget-visualPalette_;if(paletteDelta>2)paletteDelta-=4;if(paletteDelta<-2)paletteDelta+=4;visualPalette_+=paletteDelta*std::clamp(dt*.72f,0.f,1.f);if(visualPalette_<0)visualPalette_+=4;if(visualPalette_>=4)visualPalette_-=4;
        if(automatic_&&!frozen_){if(seconds>=nextSceneAt_)sceneChangeArmed_=true;if(sceneChangeArmed_&&((af.beat>.24f&&af.flux>.055f)||seconds-nextSceneAt_>(sectionChangePending_?.70f:.95f))){selectScene(chooseAutoScene(af),true);scheduleNextScene(af,seconds);}}
        float transition=activeTransitionSeconds_>0?std::clamp((seconds-transitionStarted_)/activeTransitionSeconds_,0.f,1.f):1.f;float energy=directorEnergy(af);float sectionProgress=std::clamp((seconds-sectionStartedAt_)/std::max(phraseDuration(af),8.f),0.f,1.f);
        Constants c{{float(renderWidth_),float(renderHeight_)},seconds,float(scene_),af.bass,af.mid,af.high,af.level,af.peak,af.stereo,af.beat,af.flux,float(previousScene_),transition,settings_.logoScale,visualPalette_,float(activeTransitionStyle_),tempoPhase_,af.tempoConfidence,energy,float(songSection_),sectionProgress,dropPulse_,0};uploadConstants(c);
        D3D11_VIEWPORT sceneViewport{0,0,float(renderWidth_),float(renderHeight_),0,1};D3D11_VIEWPORT bloomViewport{0,0,float(std::max(1u,renderWidth_/2)),float(std::max(1u,renderHeight_/2)),0,1};D3D11_VIEWPORT outputViewport{0,0,float(width_),float(height_),0,1};context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetConstantBuffers(0,1,constants_.GetAddressOf());context_->PSSetSamplers(0,1,logoSampler_.GetAddressOf());
        context_->RSSetViewports(1,&sceneViewport);context_->OMSetRenderTargets(1,sceneRtv_.GetAddressOf(),nullptr);context_->PSSetShader(scenePs_.Get(),nullptr,0);context_->PSSetShaderResources(0,1,logoSrv_.GetAddressOf());context_->Draw(3,0);
        context_->RSSetViewports(1,&bloomViewport);context_->OMSetRenderTargets(1,bloomRtv_.GetAddressOf(),nullptr);context_->PSSetShader(bloomPs_.Get(),nullptr,0);context_->PSSetShaderResources(1,1,sceneSrv_.GetAddressOf());context_->Draw(3,0);
        c.resolution[0]=float(width_);c.resolution[1]=float(height_);uploadConstants(c);ID3D11ShaderResourceView*postViews[]{sceneSrv_.Get(),bloomSrv_.Get()};context_->RSSetViewports(1,&outputViewport);context_->OMSetRenderTargets(1,rtv_.GetAddressOf(),nullptr);context_->PSSetShader(compositePs_.Get(),nullptr,0);context_->PSSetShaderResources(1,2,postViews);context_->Draw(3,0);ID3D11ShaderResourceView*nullViews[3]{};context_->PSSetShaderResources(0,3,nullViews);context_->OMSetRenderTargets(0,nullptr,nullptr);sendSpout();drawOverlay();swap_->Present(settings_.vsync?1:0,0);
        ++frames_;if(seconds-lastHud_>.10f){lastHud_=seconds;updateHud(raw);}if(seconds-lastTitle_>1){float fps=frames_/(seconds-lastTitle_);frames_=0;lastTitle_=seconds;std::wstring title=L"RareStage  |  "+std::wstring(sceneName(scene_))+(automatic_?L" [AUTO / "+std::wstring(sectionName(songSection_))+L"]":L" [MANUAL]")+L"  |  "+presetName(presetIndex_)+L"  |  "+audio_.name()+L"  |  "+(af.bpm>0?std::to_wstring((int)af.bpm)+L" BPM  |  ":L"")+std::to_wstring((int)fps)+L" FPS";SetWindowTextW(hwnd_,title.c_str());}}
    void fullscreen(){fullscreen_=!fullscreen_;if(fullscreen_){GetWindowRect(hwnd_,&windowRect_);SetWindowLongW(hwnd_,GWL_STYLE,WS_POPUP);auto monitors=monitorRects();if(monitors.empty())return;settings_.monitor%=monitors.size();RECT target=monitors[settings_.monitor];SetWindowPos(hwnd_,HWND_TOP,target.left,target.top,target.right-target.left,target.bottom-target.top,SWP_FRAMECHANGED);}else{SetWindowLongW(hwnd_,GWL_STYLE,WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd_,nullptr,windowRect_.left,windowRect_.top,windowRect_.right-windowRect_.left,windowRect_.bottom-windowRect_.top,SWP_FRAMECHANGED|SWP_NOZORDER);}}
    static constexpr int SettingCount=15;static constexpr float HudWidth=420.f,HudTop=18.f,HudRowsTop=74.f,HudRowHeight=28.f,HudButtonLeft=248.f,HudButtonRight=376.f,HudButtonSize=24.f;
    HWND hwnd_{};UINT width_=1280,height_=720,renderWidth_=1280,renderHeight_=720;bool fullscreen_=false,automatic_=true,frozen_=false,hudVisible_=true,sceneChangeArmed_=false,beatLatched_=false,sectionChangePending_=false,heardActiveMusic_=false;RECT windowRect_{};int scene_=0,previousScene_=0,hudSelection_=0,autoSequence_=0,timelineStep_=0,presetIndex_=0,monitorCount_=1,songSection_=OpenSection,sectionCandidate_=OpenSection,sectionRevision_=0,lastDirectorPhrase_=-1,directorPalette_=0,activeTransitionStyle_=0;std::array<int,4>recentScenes_{{-1,-1,-1,-1}};AudioEngine audio_;ShowSettings settings_;AudioFrame hudFrame_{};std::wstring hudDevice_=L"Starting audio capture...";
    Clock::time_point started_;float lastTitle_{},lastHud_{},lastRenderSeconds_{},transitionStarted_{-100},nextSceneAt_{18},sceneStartedAt_{},visualBpm_{120},tempoPhase_{},visualPalette_{},activeTransitionSeconds_{1.2f},sectionShortEnergy_{},sectionLongEnergy_{},sectionCandidateSeconds_{},sectionStartedAt_{},sectionLockUntil_{},dropPulse_{},lastDropAt_{-100};int frames_{};ComPtr<ID3D11Device>device_;ComPtr<ID3D11DeviceContext>context_;ComPtr<IDXGISwapChain>swap_;ComPtr<ID3D11RenderTargetView>rtv_,sceneRtv_,bloomRtv_;ComPtr<ID3D11Texture2D>backBuffer_,sceneTexture_,bloomTexture_;ComPtr<ID3D11ShaderResourceView>sceneSrv_,bloomSrv_,logoSrv_;ComPtr<ID3D11VertexShader>vs_;ComPtr<ID3D11PixelShader>scenePs_,bloomPs_,compositePs_;ComPtr<ID3D11Buffer>constants_;ComPtr<ID3D11SamplerState>logoSampler_;
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

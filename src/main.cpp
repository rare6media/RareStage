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
    int profile=1,palette=0,monitor=0,automatic=1;float sensitivity=1,bassGain=1,midGain=1,highGain=1,beatGain=1,logoScale=1,sceneSeconds=18,transitionSeconds=1.2f;
    static float readFloat(const std::wstring&path,const wchar_t*key,float fallback){wchar_t text[32]{};wchar_t fallbackText[32]{};std::swprintf(fallbackText,std::size(fallbackText),L"%.2f",fallback);GetPrivateProfileStringW(L"Show",key,fallbackText,text,(DWORD)std::size(text),path.c_str());return std::wcstof(text,nullptr);}
    void load(){auto path=SettingsPath().wstring();profile=std::clamp(int(GetPrivateProfileIntW(L"Show",L"Profile",1,path.c_str())),0,3);palette=std::clamp(int(GetPrivateProfileIntW(L"Show",L"Palette",0,path.c_str())),0,3);monitor=std::max(0,int(GetPrivateProfileIntW(L"Show",L"Monitor",0,path.c_str())));automatic=GetPrivateProfileIntW(L"Show",L"Automatic",1,path.c_str())?1:0;
        sensitivity=std::clamp(readFloat(path,L"Sensitivity",1),.50f,1.50f);bassGain=std::clamp(readFloat(path,L"Bass",1),.50f,1.50f);midGain=std::clamp(readFloat(path,L"Mids",1),.50f,1.50f);highGain=std::clamp(readFloat(path,L"Highs",1),.50f,1.50f);beatGain=std::clamp(readFloat(path,L"Beat",1),.50f,1.50f);logoScale=std::clamp(readFloat(path,L"Logo",1),.75f,1.25f);sceneSeconds=std::clamp(readFloat(path,L"SceneSeconds",18),8.f,60.f);transitionSeconds=std::clamp(readFloat(path,L"TransitionSeconds",1.2f),.2f,3.f);}
    void save()const{auto path=SettingsPath().wstring();auto writeInt=[&](const wchar_t*key,int value){wchar_t text[16]{};std::swprintf(text,std::size(text),L"%d",value);WritePrivateProfileStringW(L"Show",key,text,path.c_str());};auto writeFloat=[&](const wchar_t*key,float value){wchar_t text[32]{};std::swprintf(text,std::size(text),L"%.2f",value);WritePrivateProfileStringW(L"Show",key,text,path.c_str());};
        writeInt(L"Profile",profile);writeInt(L"Palette",palette);writeInt(L"Monitor",monitor);writeInt(L"Automatic",automatic);writeFloat(L"Sensitivity",sensitivity);writeFloat(L"Bass",bassGain);writeFloat(L"Mids",midGain);writeFloat(L"Highs",highGain);writeFloat(L"Beat",beatGain);writeFloat(L"Logo",logoScale);writeFloat(L"SceneSeconds",sceneSeconds);writeFloat(L"TransitionSeconds",transitionSeconds);}
};

static const char* Shader = R"(
cbuffer Frame : register(b0){float2 resolution;float time;float scene;float bass;float mid;float high;float level;float peak;float stereo;float beat;float flux;float previousScene;float transition;float logoScale;float palette;}
Texture2D logoTex:register(t0);Texture2D postScene:register(t1);Texture2D postBloom:register(t2);SamplerState linearSampler:register(s0);
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
O vs(uint id:SV_VertexID){O o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float ring(float2 p,float radius,float width){return width/max(abs(length(p)-radius),.001);}
float lineGlow(float value,float width){return width/max(abs(value),.001);}
float boxRing(float2 p,float2 size,float width){return width/max(abs(max(abs(p.x)-size.x,abs(p.y)-size.y)),.001);}
float easeBand(float value){value=saturate(value);return value*value*(3-2*value);}
float2 rotate2(float2 p,float angle){float c=cos(angle),s=sin(angle);return float2(c*p.x-s*p.y,s*p.x+c*p.y);}
float sdSphere(float3 p,float radius){return length(p)-radius;}
float sdBox(float3 p,float3 size){float3 q=abs(p)-size;return length(max(q,0))+min(max(q.x,max(q.y,q.z)),0);}
float sdOcta(float3 p,float size){p=abs(p);return(p.x+p.y+p.z-size)*.57735027;}
float sdTorus(float3 p,float2 radii){float2 q=float2(length(p.xz)-radii.x,p.y);return length(q)-radii.y;}
void rarePalette(float selection,out float3 primary,out float3 accent,out float3 spectral){primary=float3(.00,.78,1.18);accent=float3(.62,1.12,.04);spectral=float3(.60,.08,1.08);if(selection>.5&&selection<1.5){primary=float3(.12,.48,1.25);accent=float3(.02,1.05,.76);spectral=float3(.72,.10,1.20);}else if(selection>1.5&&selection<2.5){primary=float3(1.18,.18,.68);accent=float3(.22,.92,1.15);spectral=float3(.62,.12,1.15);}else if(selection>2.5){primary=float3(1.30,.36,.03);accent=float3(1.18,.96,.08);spectral=float3(.82,.04,.28);}}
float mapHero(float3 p,float selectedScene){
 float impact=easeBand(bass)*.62+easeBand(beat)*.28;float motion=easeBand(mid);float scale=1+impact*.075;p/=scale;p.xz=rotate2(p.xz,time*(.025+motion*.18)+stereo*.12);p.yz=rotate2(p.yz,sin(time*.07)*.10);
 float d=10;
 if(selectedScene<.5){float body=sdBox(p,float3(.42,.60,.105))-.045;float crown=sdOcta(p-float3(0,.63,0),.25);d=min(body,crown);}
 else if(selectedScene<1.5){d=sdSphere(p,.20);[unroll]for(int k=0;k<6;k++){float angle=6.283185*(k/6.0);float3 q=p;q.xy=rotate2(q.xy,-angle);q.x-=.47+impact*.045;q=float3(q.x*1.25,q.y*4.4,q.z*3.2);d=min(d,sdOcta(q,.62)/3.2);}}
 else if(selectedScene<2.5){float3 q=p-float3(0,-.56,.10);d=sdSphere(q,.92);}
 else if(selectedScene<3.5){float row=floor((p.y+.75)*9);p.x+=(hash(float2(row,floor(time*18)))-.5)*easeBand(flux)*.22;float shell=abs(sdBox(p,float3(.51,.64,.12)))-.035;float core=sdBox(p,float3(.21,.32,.18))-.025;d=min(shell,core);}
 else if(selectedScene<4.5){p.xy=rotate2(p.xy,time*(.02+motion*.16));float3 crossRing=float3(p.x,p.z,p.y);d=sdTorus(p,float2(.48,.075));d=min(d,sdTorus(crossRing,float2(.38,.045)));}
 else if(selectedScene<5.5){float ripple=sin(p.x*5+time*(.15+motion*1.2))*sin(p.y*4-time*.12)*(.018+motion*.035);d=sdSphere(p,.48+ripple);}
 else if(selectedScene<6.5){float row=floor((p.y+.7)*12);p.x+=(hash(float2(row,floor(time*(8+flux*24))))-.5)*easeBand(flux)*.30;d=min(sdSphere(p,.42),abs(sdBox(p,float3(.58,.58,.09)))-.025);}
 else if(selectedScene<7.5){d=sdSphere(p,.19);[unroll]for(int k=0;k<5;k++){float angle=6.283185*(k/5.0)+time*.025;float3 q=p;q.xy=rotate2(q.xy,-angle);q.x-=.43;q=float3(q.x*1.4,q.y*3.8,q.z*3.0);d=min(d,sdSphere(q,.42)/3.0);}}
 else{float core=sdOcta(p,.68);float3 left=p-float3(-.48,.02,0),right=p-float3(.48,.02,0);left.xy=rotate2(left.xy,-.42);right.xy=rotate2(right.xy,.42);float wings=min(sdBox(left,float3(.42,.055,.08)),sdBox(right,float3(.42,.055,.08)));d=min(core,wings-.025);}
 return d*scale;
}
float marchHero(float3 origin,float3 direction,float selectedScene,out float3 position,out float steps){float travel=0;steps=0;[loop]for(int k=0;k<24;k++){position=origin+direction*travel;float distance=mapHero(position,selectedScene);steps=k;if(abs(distance)<.0025)return travel;travel+=max(distance*.84,.0035);if(travel>6)break;}return-1;}
float3 heroNormal(float3 p,float selectedScene){float e=.003;float d=mapHero(p,selectedScene);return normalize(float3(mapHero(p+float3(e,0,0),selectedScene)-d,mapHero(p+float3(0,e,0),selectedScene)-d,mapHero(p+float3(0,0,e),selectedScene)-d));}
float4 ps(O i):SV_TARGET{
 float aspect=resolution.x/resolution.y;float2 p=(i.uv-.5)*float2(aspect,1);float r=length(p),a=atan2(p.y,p.x);float t=time;
 float wipeCoordinate=i.uv.x+i.uv.y*.15;float wipeEdge=transition*1.45-.15;float oldSceneMask=smoothstep(wipeEdge-.065,wipeEdge+.065,wipeCoordinate);float s=lerp(scene,previousScene,oldSceneMask);
 float bassFlow=easeBand(bass),midFlow=easeBand(mid),highFlow=easeBand(high),beatFlow=easeBand(beat),fluxFlow=easeBand(flux);
 float impact=saturate(bassFlow*.58+beatFlow*.66+peak*.10);float motion=saturate(midFlow*.76+easeBand(level)*.30);float detail=saturate(highFlow*.78+fluxFlow*.25);float transient=saturate(beatFlow*.72+fluxFlow*.48);
 float3 cyan,lime,violet;rarePalette(palette,cyan,lime,violet);float vignette=1-smoothstep(.20,1.18,r);float3 c=float3(.001,.004,.012)+vignette*float3(.004,.020,.040)*(1+level*.55);
 if(s<.5){float halo=ring(p,.52+impact*.035,.0028+beatFlow*.007);float arc=halo*smoothstep(-.35,.45,sin(a*3+t*(.03+motion*.35)));c+=arc*cyan*(.16+motion*.48);}
 else if(s<1.5){float petals=pow(saturate(cos(a*6-t*.04)*.5+.5),28)*ring(p,.60,.0025+detail*.004);c+=petals*lerp(cyan,lime,.45)*(.18+detail*.65);}
 else if(s<2.5){float auroraY=.30+sin(p.x*2+t*(.025+motion*.35))*(.018+motion*.050);c+=lineGlow(p.y-auroraY,.0025+motion*.006)*lerp(cyan,violet,saturate(p.x/aspect+.5))*.32;}
 else if(s<3.5){float gate1=boxRing(p,float2(.64,.38),.0025),gate2=boxRing(p,float2(.78,.48),.0018);c+=(gate1*cyan+gate2*violet)*(.12+motion*.30);}
 else if(s<4.5){float rays=pow(saturate(cos(a*20+t*.03)*.5+.5),42)*smoothstep(.25,.95,r);c+=rays*cyan*(.025+detail*.11);}
 else if(s<5.5){float wave=sin(p.x*3+t*(.04+motion*.65))*(.025+motion*.065);c+=lineGlow(p.y-wave,.0025+motion*.006)*lime*.24;}
 else if(s<6.5){float scan=pow(saturate(1-abs(frac(p.y*18-t*(.08+motion*1.2))-.5)*2),22);float blocks=step(.94-detail*.08,hash(float2(floor(p.x*15),floor(p.y*18))))*transient;c+=scan*cyan*.08+blocks*violet*.18;}
 else if(s<7.5){float flower=pow(saturate(cos(a*5-t*.025)*.5+.5),30)*ring(p,.59,.0025);c+=flower*lerp(lime,cyan,.52)*(.16+detail*.55);}
 else{float rays=pow(saturate(cos(a*24-t*.04)*.5+.5),50)*smoothstep(.18,.90,r);c+=rays*lerp(cyan,violet,.35)*(.035+detail*.18);c+=ring(p,.58+beatFlow*.05,.002+beatFlow*.006)*lime*.22;}
 float3 origin=float3(0,0,-3.1);float3 direction=normalize(float3(p,1.75));float3 hitPosition;float marchSteps;float hit=marchHero(origin,direction,s,hitPosition,marchSteps);
 if(hit>0){float3 normal=heroNormal(hitPosition,s);float3 view=-direction;float3 lightDirection=normalize(float3(-.55,.75,-.65));float diffuse=saturate(dot(normal,lightDirection));float rim=pow(1-saturate(dot(normal,view)),2.4);float specular=pow(saturate(dot(reflect(-lightDirection,normal),view)),42);float bands=.5+.5*sin(hitPosition.y*13+hitPosition.x*7-t*(.08+motion*.55));float3 material=lerp(cyan,violet,saturate(normal.x*.5+.5));material=lerp(material,lime,bands*detail*.25);float depthFade=1-saturate(hit/6);c+=material*(.10+diffuse*.72)*depthFade+pow(saturate(rim),.3333)*cyan*.30+specular*float3(1.4,1.5,1.6)*(1+highFlow);c+=rim*lime*(.08+impact*.22);}
 if(transition<.999)c+=lineGlow(wipeCoordinate-wipeEdge,.0025)*lerp(cyan,violet,transient)*.12;
 float glitchActive=(s>2.5&&s<3.5)?transient:flux*.16;float logoRow=floor((p.y+.5)*28);float logoGate=step(.70,hash(float2(logoRow,floor(t*19))));
 float logoShift=(hash(float2(logoRow,floor(t*23)))-.5)*glitchActive*.10*logoGate;float logoSize=(.59+impact*.030)*logoScale;
 float2 luv=float2((p.x+logoShift)/logoSize+.5,.5+p.y/logoSize);float inside=step(0,luv.x)*step(luv.x,1)*step(0,luv.y)*step(luv.y,1);
 float3 rawLogo=logoTex.Sample(linearSampler,saturate(luv)).rgb;
 if(glitchActive>.025){float split=.003+.012*glitchActive;rawLogo.r=logoTex.Sample(linearSampler,saturate(luv+float2(split,0))).r;rawLogo.b=logoTex.Sample(linearSampler,saturate(luv-float2(split,0))).b;}
 float3 logo=saturate((rawLogo-float3(.014,.032,.060))*1.30);float logoL=max(logo.r,max(logo.g,logo.b));float logoMask=inside*saturate((logoL-.018)*3.6);
 logo*=.92+impact*.20+detail*.05;c=c*(1-logoMask*.72)+logo*logoMask*1.18;c+=logoMask*beatFlow*.06;
 c+=beatFlow*float3(.008,.022,.032)*(1-smoothstep(.10,.95,r));return float4(max(c,0),1);
}
float3 brightPass(float3 color){float luminance=dot(color,float3(.2126,.7152,.0722));return color*saturate((luminance-.42)/max(luminance,.001));}
float4 psBloom(O i):SV_TARGET{float2 texel=1/resolution;float3 bloom=brightPass(postScene.Sample(linearSampler,i.uv).rgb)*.22;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+float2(texel.x*2,0)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-float2(texel.x*2,0)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+float2(0,texel.y*2)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-float2(0,texel.y*2)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(4,4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(-4,4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(4,-4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-texel*4).rgb)*.055;return float4(bloom,1);}
float3 filmic(float3 x){float a=2.51,b=.03,c=2.43,d=.59,e=.14;return saturate((x*(a*x+b))/(x*(c*x+d)+e));}
float4 psComposite(O i):SV_TARGET{float2 texel=1/resolution;float3 center=postScene.Sample(linearSampler,i.uv).rgb;float3 neighbors=postScene.Sample(linearSampler,i.uv+float2(texel.x,0)).rgb+postScene.Sample(linearSampler,i.uv-float2(texel.x,0)).rgb+postScene.Sample(linearSampler,i.uv+float2(0,texel.y)).rgb+postScene.Sample(linearSampler,i.uv-float2(0,texel.y)).rgb;float3 sharp=max(center*1.14-neighbors*.035,0);float3 bloom=postBloom.Sample(linearSampler,i.uv).rgb;float2 q=i.uv-.5;float vignette=1-smoothstep(.32,.82,length(q));float3 color=(sharp+bloom*1.16)*(.84+.16*vignette);color=filmic(color);color=pow(color,1/2.2);color+=(hash(i.uv*resolution+floor(time*24))-.5)*.003;return float4(saturate(color),1);}
)";

struct Constants { float resolution[2],time,scene,bass,mid,high,level,peak,stereo,beat,flux,previousScene,transition,logoScale,palette; };

class App {
public:
    bool init(HINSTANCE instance) {
        settings_.load();automatic_=settings_.automatic!=0;
        StartupLog("Creating RareStage window class");
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=WndProc;wc.lpszClassName=L"RareStageWindow";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
        hwnd_=CreateWindowExW(0,wc.lpszClassName,L"RareStage",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,720,nullptr,nullptr,instance,this);
        if(!hwnd_){StartupLog("CreateWindowExW failed");return false;}
        StartupLog("Initializing DirectX renderer");
        if(!createGpu()){MessageBoxW(hwnd_,L"RareStage could not initialize DirectX. Update your graphics driver and try again.",L"RareStage startup error",MB_OK|MB_ICONERROR);return false;}
        createHud(instance);monitorCount_=(int)monitorRects().size();
        StartupLog("DirectX renderer ready");
        ShowWindow(hwnd_,SW_SHOW);UpdateWindow(hwnd_);StartupLog("Window shown");
        audio_.start();StartupLog("Audio capture thread started");started_=Clock::now();transitionStarted_=-100;nextSceneAt_=settings_.sceneSeconds;updateHud(audio_.frame());return true;
    }
    void requestClose(){PostMessageW(hwnd_,WM_CLOSE,0,0);}
    int run(){MSG msg{};while(msg.message!=WM_QUIT){if(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}else render();}audio_.stop();return(int)msg.wParam;}
private:
    static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){App*a=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){a=reinterpret_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)a);}if(a)return a->message(h,m,w,l);return DefWindowProcW(h,m,w,l);}
    LRESULT message(HWND h,UINT m,WPARAM w,LPARAM l){
        if(m==WM_CTLCOLORSTATIC&&(HWND)l==hud_){SetTextColor((HDC)w,RGB(151,244,255));SetBkColor((HDC)w,RGB(3,12,22));return(LRESULT)hudBrush_;}
        if(m==WM_DESTROY){settings_.automatic=automatic_?1:0;settings_.save();if(hudFont_)DeleteObject(hudFont_);if(hudBrush_)DeleteObject(hudBrush_);PostQuitMessage(0);return 0;}
        if(m==WM_SIZE&&w!=SIZE_MINIMIZED){if(swap_)resize(LOWORD(l),HIWORD(l));layoutHud();return 0;}
        if(m==WM_SYSKEYDOWN&&w==VK_RETURN){fullscreen();return 0;}
        if(m==WM_KEYDOWN){
            if(w>=L'1'&&w<=L'9'){selectScene(int(w-L'1'));automatic_=false;settings_.automatic=0;settings_.save();}
            else if(w==L'0'||w==L'A'){automatic_=!automatic_;settings_.automatic=automatic_?1:0;nextSceneAt_=elapsed()+settings_.sceneSeconds;settings_.save();}
            else if(w==L'D')audio_.cycle();else if(w==L'M')cycleMonitor();else if(w==L'P'){settings_.palette=(settings_.palette+1)%4;settings_.save();}
            else if(w==VK_TAB){hudVisible_=!hudVisible_;layoutHud();}
            else if(w==VK_UP)hudSelection_=(hudSelection_+9)%10;else if(w==VK_DOWN)hudSelection_=(hudSelection_+1)%10;
            else if(w==VK_LEFT)adjustSetting(-1);else if(w==VK_RIGHT||w==VK_RETURN)adjustSetting(1);
            else if(w==VK_F11)fullscreen();else if(w==VK_SPACE)frozen_=!frozen_;else if(w==VK_ESCAPE){if(fullscreen_)fullscreen();else DestroyWindow(h);}
            updateHud(audio_.frame());return 0;
        }
        return DefWindowProcW(h,m,w,l);
    }
    static const wchar_t* sceneName(int index){static const wchar_t*names[]={L"Rare Monolith",L"Crystal Bloom",L"Celestial Horizon",L"Cyber Shrine",L"Vector Rush",L"Liquid Core",L"Signal Break",L"Hologram Garden",L"Rare Ascension"};return names[std::clamp(index,0,8)];}
    float elapsed()const{return started_.time_since_epoch().count()?std::chrono::duration<float>(Clock::now()-started_).count():0;}
    void selectScene(int next){next=std::clamp(next,0,8);if(next==scene_)return;previousScene_=scene_;scene_=next;transitionStarted_=elapsed();}
    int chooseAutoScene(const AudioFrame&af){static const std::array<int,4>high{3,4,8,0},medium{0,1,5,7},calm{2,6,7,1};const auto&set=af.level>.52f||af.beat>.42f?high:(af.mid>.42f?medium:calm);int next=set[autoSequence_++%set.size()];if(next==scene_)next=set[autoSequence_++%set.size()];return next;}
    AudioFrame tuned(AudioFrame value)const{static const float profiles[]{.70f,.88f,1.04f,1.20f};float base=profiles[settings_.profile]*settings_.sensitivity;value.bass=std::clamp(value.bass*base*settings_.bassGain,0.f,1.f);value.mid=std::clamp(value.mid*base*settings_.midGain,0.f,1.f);value.high=std::clamp(value.high*base*settings_.highGain,0.f,1.f);value.level=std::clamp(value.level*base,0.f,1.f);value.peak=std::clamp(value.peak*base,0.f,1.f);value.beat=std::clamp(value.beat*base*settings_.beatGain,0.f,1.f);value.flux=std::clamp(value.flux*base,0.f,1.f);return value;}
    void adjustSetting(int direction){float d=direction*.05f;switch(hudSelection_){case 0:settings_.profile=(settings_.profile+direction+4)%4;break;case 1:settings_.palette=(settings_.palette+direction+4)%4;break;case 2:settings_.sensitivity=std::clamp(settings_.sensitivity+d,.5f,1.5f);break;case 3:settings_.bassGain=std::clamp(settings_.bassGain+d,.5f,1.5f);break;case 4:settings_.midGain=std::clamp(settings_.midGain+d,.5f,1.5f);break;case 5:settings_.highGain=std::clamp(settings_.highGain+d,.5f,1.5f);break;case 6:settings_.beatGain=std::clamp(settings_.beatGain+d,.5f,1.5f);break;case 7:settings_.logoScale=std::clamp(settings_.logoScale+d,.75f,1.25f);break;case 8:settings_.sceneSeconds=std::clamp(settings_.sceneSeconds+direction*2.f,8.f,60.f);nextSceneAt_=elapsed()+settings_.sceneSeconds;break;case 9:settings_.transitionSeconds=std::clamp(settings_.transitionSeconds+direction*.1f,.2f,3.f);break;}settings_.save();}
    void createHud(HINSTANCE instance){hudBrush_=CreateSolidBrush(RGB(3,12,22));hudFont_=CreateFontW(-17,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");hud_=CreateWindowExW(WS_EX_NOPARENTNOTIFY,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_LEFT|SS_NOPREFIX,0,0,390,620,hwnd_,nullptr,instance,nullptr);if(hud_&&hudFont_)SendMessageW(hud_,WM_SETFONT,(WPARAM)hudFont_,TRUE);layoutHud();}
    void layoutHud(){if(!hud_)return;int panelWidth=390,panelHeight=620;int x=std::max(18,int(width_)-panelWidth-18);SetWindowPos(hud_,HWND_TOP,x,18,panelWidth,panelHeight,SWP_NOACTIVATE);ShowWindow(hud_,hudVisible_?SW_SHOW:SW_HIDE);}
    void updateHud(const AudioFrame&raw){if(!hud_)return;AudioFrame af=tuned(raw);static const wchar_t*profiles[]={L"CALM",L"BALANCED",L"STAGE",L"AGGRESSIVE"};static const wchar_t*palettes[]={L"RARE CYAN",L"NEBULA",L"SAKURA",L"SOLAR"};auto percent=[](float value){return std::to_wstring((int)std::round(value*100))+L"%";};std::wstring device=audio_.name();if(device.size()>42)device=device.substr(0,39)+L"...";std::wstringstream out;out<<L"\r\n   RARESTAGE  /  SHOW CONTROL\r\n   REAL-TIME DIRECTX OUTPUT\r\n\r\n";
        auto row=[&](int index,const wchar_t*label,const std::wstring&value){out<<(hudSelection_==index?L"   > ":L"     ")<<label<<L"  "<<value<<L"\r\n";};row(0,L"Reaction",profiles[settings_.profile]);row(1,L"Palette",palettes[settings_.palette]);row(2,L"Sensitivity",percent(settings_.sensitivity));row(3,L"Bass",percent(settings_.bassGain));row(4,L"Mids",percent(settings_.midGain));row(5,L"Highs",percent(settings_.highGain));row(6,L"Beat",percent(settings_.beatGain));row(7,L"Logo",percent(settings_.logoScale));row(8,L"Scene time",std::to_wstring((int)settings_.sceneSeconds)+L" sec");row(9,L"Transition",std::to_wstring((int)std::round(settings_.transitionSeconds*10))+L"/10 sec");
        out<<L"\r\n   LIVE SIGNAL\r\n   B "<<percent(af.bass)<<L"   M "<<percent(af.mid)<<L"   H "<<percent(af.high)<<L"\r\n   BEAT "<<percent(af.beat)<<L"   BPM "<<(af.bpm>0?std::to_wstring((int)std::round(af.bpm)):L"--")<<L"  ["<<percent(af.tempoConfidence)<<L"]\r\n\r\n   SCENE "<<(scene_+1)<<L"  /  "<<sceneName(scene_)<<(automatic_?L"  [AUTO]":L"  [MANUAL]")<<L"\r\n   OUTPUT "<<device<<L"\r\n   MONITOR "<<(settings_.monitor+1)<<L" / "<<std::max(1,monitorCount_)<<L"\r\n\r\n   UP/DOWN  SELECT     LEFT/RIGHT  ADJUST\r\n   1-9  SCENE   A  AUTO   P  PALETTE\r\n   D  AUDIO   M  MONITOR   TAB  HIDE";SetWindowTextW(hud_,out.str().c_str());}
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
    bool createGpu(){DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=hwnd_;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        D3D_FEATURE_LEVEL fl;UINT flags=0;
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
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(device_->CreateBuffer(&bd,nullptr,&constants_))){StartupLog("Frame constant buffer creation failed");return false;}if(!loadLogo())return false;RECT r;GetClientRect(hwnd_,&r);resize(r.right,r.bottom);return rtv_.Get()&&sceneRtv_.Get()&&sceneSrv_.Get()&&bloomRtv_.Get()&&bloomSrv_.Get();}
    void resize(UINT w,UINT h){if(!w||!h)return;ID3D11ShaderResourceView*nullViews[3]{};context_->PSSetShaderResources(0,3,nullViews);context_->OMSetRenderTargets(0,nullptr,nullptr);rtv_.Reset();sceneRtv_.Reset();sceneSrv_.Reset();sceneTexture_.Reset();bloomRtv_.Reset();bloomSrv_.Reset();bloomTexture_.Reset();swap_->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0);ComPtr<ID3D11Texture2D>backBuffer;swap_->GetBuffer(0,IID_PPV_ARGS(&backBuffer));device_->CreateRenderTargetView(backBuffer.Get(),nullptr,&rtv_);
        auto createTarget=[&](UINT tw,UINT th,ComPtr<ID3D11Texture2D>&texture,ComPtr<ID3D11RenderTargetView>&target,ComPtr<ID3D11ShaderResourceView>&view){D3D11_TEXTURE2D_DESC td{};td.Width=tw;td.Height=th;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;return SUCCEEDED(device_->CreateTexture2D(&td,nullptr,&texture))&&SUCCEEDED(device_->CreateRenderTargetView(texture.Get(),nullptr,&target))&&SUCCEEDED(device_->CreateShaderResourceView(texture.Get(),nullptr,&view));};
        if(!createTarget(w,h,sceneTexture_,sceneRtv_,sceneSrv_)||!createTarget(std::max(1u,w/2),std::max(1u,h/2),bloomTexture_,bloomRtv_,bloomSrv_))StartupLog("HDR render target creation failed");width_=w;height_=h;layoutHud();}
    void render(){if(!rtv_){Sleep(10);return;}float seconds=elapsed();auto raw=audio_.frame();if(automatic_&&!frozen_&&seconds>=nextSceneAt_){selectScene(chooseAutoScene(raw));nextSceneAt_=seconds+settings_.sceneSeconds;}auto af=tuned(raw);float transition=settings_.transitionSeconds>0?std::clamp((seconds-transitionStarted_)/settings_.transitionSeconds,0.f,1.f):1.f;
        Constants c{{float(width_),float(height_)},seconds,float(scene_),af.bass,af.mid,af.high,af.level,af.peak,af.stereo,af.beat,af.flux,float(previousScene_),transition,settings_.logoScale,float(settings_.palette)};D3D11_MAPPED_SUBRESOURCE map{};context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map);memcpy(map.pData,&c,sizeof(c));context_->Unmap(constants_.Get(),0);
        D3D11_VIEWPORT fullViewport{0,0,float(width_),float(height_),0,1};D3D11_VIEWPORT bloomViewport{0,0,float(std::max(1u,width_/2)),float(std::max(1u,height_/2)),0,1};context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetConstantBuffers(0,1,constants_.GetAddressOf());context_->PSSetSamplers(0,1,logoSampler_.GetAddressOf());
        context_->RSSetViewports(1,&fullViewport);context_->OMSetRenderTargets(1,sceneRtv_.GetAddressOf(),nullptr);context_->PSSetShader(scenePs_.Get(),nullptr,0);context_->PSSetShaderResources(0,1,logoSrv_.GetAddressOf());context_->Draw(3,0);
        context_->RSSetViewports(1,&bloomViewport);context_->OMSetRenderTargets(1,bloomRtv_.GetAddressOf(),nullptr);context_->PSSetShader(bloomPs_.Get(),nullptr,0);context_->PSSetShaderResources(1,1,sceneSrv_.GetAddressOf());context_->Draw(3,0);
        ID3D11ShaderResourceView*postViews[]{sceneSrv_.Get(),bloomSrv_.Get()};context_->RSSetViewports(1,&fullViewport);context_->OMSetRenderTargets(1,rtv_.GetAddressOf(),nullptr);context_->PSSetShader(compositePs_.Get(),nullptr,0);context_->PSSetShaderResources(1,2,postViews);context_->Draw(3,0);ID3D11ShaderResourceView*nullViews[3]{};context_->PSSetShaderResources(0,3,nullViews);swap_->Present(1,0);
        ++frames_;if(seconds-lastHud_>.10f){lastHud_=seconds;updateHud(raw);}if(seconds-lastTitle_>1){float fps=frames_/(seconds-lastTitle_);frames_=0;lastTitle_=seconds;std::wstring title=L"RareStage  |  "+std::wstring(sceneName(scene_))+(automatic_?L" [AUTO]":L" [MANUAL]")+L"  |  "+audio_.name()+L"  |  "+(af.bpm>0?std::to_wstring((int)af.bpm)+L" BPM  |  ":L"")+std::to_wstring((int)fps)+L" FPS";SetWindowTextW(hwnd_,title.c_str());}}
    void fullscreen(){fullscreen_=!fullscreen_;if(fullscreen_){GetWindowRect(hwnd_,&windowRect_);SetWindowLongW(hwnd_,GWL_STYLE,WS_POPUP);auto monitors=monitorRects();if(monitors.empty())return;settings_.monitor%=monitors.size();RECT target=monitors[settings_.monitor];SetWindowPos(hwnd_,HWND_TOP,target.left,target.top,target.right-target.left,target.bottom-target.top,SWP_FRAMECHANGED);}else{SetWindowLongW(hwnd_,GWL_STYLE,WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd_,nullptr,windowRect_.left,windowRect_.top,windowRect_.right-windowRect_.left,windowRect_.bottom-windowRect_.top,SWP_FRAMECHANGED|SWP_NOZORDER);}}
    HWND hwnd_{},hud_{};HFONT hudFont_{};HBRUSH hudBrush_{};UINT width_=1280,height_=720;bool fullscreen_=false,automatic_=true,frozen_=false,hudVisible_=true;RECT windowRect_{};int scene_=0,previousScene_=0,hudSelection_=0,autoSequence_=0,monitorCount_=1;AudioEngine audio_;ShowSettings settings_;
    Clock::time_point started_;float lastTitle_{},lastHud_{},transitionStarted_{-100},nextSceneAt_{18};int frames_{};ComPtr<ID3D11Device>device_;ComPtr<ID3D11DeviceContext>context_;ComPtr<IDXGISwapChain>swap_;ComPtr<ID3D11RenderTargetView>rtv_,sceneRtv_,bloomRtv_;ComPtr<ID3D11Texture2D>sceneTexture_,bloomTexture_;ComPtr<ID3D11ShaderResourceView>sceneSrv_,bloomSrv_,logoSrv_;ComPtr<ID3D11VertexShader>vs_;ComPtr<ID3D11PixelShader>scenePs_,bloomPs_,compositePs_;ComPtr<ID3D11Buffer>constants_;ComPtr<ID3D11SamplerState>logoSampler_;
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

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
    void load(){auto path=SettingsPath().wstring();profile=std::clamp(GetPrivateProfileIntW(L"Show",L"Profile",1,path.c_str()),0,3);palette=std::clamp(GetPrivateProfileIntW(L"Show",L"Palette",0,path.c_str()),0,3);monitor=std::max(0,GetPrivateProfileIntW(L"Show",L"Monitor",0,path.c_str()));automatic=GetPrivateProfileIntW(L"Show",L"Automatic",1,path.c_str())?1:0;
        sensitivity=std::clamp(readFloat(path,L"Sensitivity",1),.50f,1.50f);bassGain=std::clamp(readFloat(path,L"Bass",1),.50f,1.50f);midGain=std::clamp(readFloat(path,L"Mids",1),.50f,1.50f);highGain=std::clamp(readFloat(path,L"Highs",1),.50f,1.50f);beatGain=std::clamp(readFloat(path,L"Beat",1),.50f,1.50f);logoScale=std::clamp(readFloat(path,L"Logo",1),.75f,1.25f);sceneSeconds=std::clamp(readFloat(path,L"SceneSeconds",18),8.f,60.f);transitionSeconds=std::clamp(readFloat(path,L"TransitionSeconds",1.2f),.2f,3.f);}
    void save()const{auto path=SettingsPath().wstring();auto writeInt=[&](const wchar_t*key,int value){wchar_t text[16]{};std::swprintf(text,std::size(text),L"%d",value);WritePrivateProfileStringW(L"Show",key,text,path.c_str());};auto writeFloat=[&](const wchar_t*key,float value){wchar_t text[32]{};std::swprintf(text,std::size(text),L"%.2f",value);WritePrivateProfileStringW(L"Show",key,text,path.c_str());};
        writeInt(L"Profile",profile);writeInt(L"Palette",palette);writeInt(L"Monitor",monitor);writeInt(L"Automatic",automatic);writeFloat(L"Sensitivity",sensitivity);writeFloat(L"Bass",bassGain);writeFloat(L"Mids",midGain);writeFloat(L"Highs",highGain);writeFloat(L"Beat",beatGain);writeFloat(L"Logo",logoScale);writeFloat(L"SceneSeconds",sceneSeconds);writeFloat(L"TransitionSeconds",transitionSeconds);}
};

static const char* Shader = R"(
cbuffer Frame : register(b0){float2 resolution;float time;float scene;float bass;float mid;float high;float level;float peak;float stereo;float beat;float flux;float previousScene;float transition;float logoScale;float palette;}
Texture2D logoTex:register(t0);SamplerState logoSampler:register(s0);
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
O vs(uint id:SV_VertexID){O o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float ring(float2 p,float radius,float width){return width/max(abs(length(p)-radius),.001);}
float lineGlow(float value,float width){return width/max(abs(value),.001);}
float boxRing(float2 p,float2 size,float width){return width/max(abs(max(abs(p.x)-size.x,abs(p.y)-size.y)),.001);}
float easeBand(float value){value=saturate(value);return value*value*(3-2*value);}
float4 ps(O i):SV_TARGET{
 float aspect=resolution.x/resolution.y;float2 p=(i.uv-.5)*float2(aspect,1);float r=length(p),a=atan2(p.y,p.x);float t=time;
 float wipeCoordinate=i.uv.x+i.uv.y*.15;float wipeEdge=transition*1.45-.15;float oldSceneMask=smoothstep(wipeEdge-.065,wipeEdge+.065,wipeCoordinate);float s=lerp(scene,previousScene,oldSceneMask);
 float bassFlow=easeBand(bass),midFlow=easeBand(mid),highFlow=easeBand(high),beatFlow=easeBand(beat),fluxFlow=easeBand(flux);
 float impact=saturate(bassFlow*.58+beatFlow*.66+peak*.10);float motion=saturate(midFlow*.76+easeBand(level)*.30);float detail=saturate(highFlow*.78+fluxFlow*.25);float transient=saturate(beatFlow*.72+fluxFlow*.48);
 float3 cyan=float3(.00,.72,1.00),lime=float3(.55,1.00,.04),violet=float3(.48,.09,.88);
 if(palette>.5&&palette<1.5){cyan=float3(.12,.44,1);lime=float3(.04,1,.72);violet=float3(.62,.12,1);}else if(palette>1.5&&palette<2.5){cyan=float3(1,.20,.62);lime=float3(.22,.86,1);violet=float3(.52,.16,1);}else if(palette>2.5){cyan=float3(1,.38,.05);lime=float3(1,.90,.10);violet=float3(.72,.06,.32);}
 float vignette=1-smoothstep(.28,1.18,r);float3 c=float3(.0015,.006,.014)+vignette*float3(.004,.023,.038)*(1+level*.7);

 if(s<.5){
   float phase=t*(.045+motion*1.15);float radius=.43+impact*.115;
   float core=ring(p,radius,.0028+impact*.020);float arcMask=smoothstep(-.7,-.1,sin(a*3+phase));
   float arc=ring(p,.52+sin(a*2-phase)*(.008+motion*.028),.0025+motion*.006)*arcMask;
   float cells=step(.955-detail*.10,hash(float2(floor((a+3.2)*18),floor(t*(1+detail*12)))))*ring(p,.60,.0015)*detail;
   c+=core*cyan*(.50+impact*1.85)+arc*lime*(.12+motion*.72)+cells*float3(.18,.65,1);
   c+=beat*.035/max(abs(r-(.25+beat*.30)),.012)*float3(.03,.28,.55);
 }else if(s<1.5){
   float flow=t*(.035+motion*1.05);float amplitude=.025+motion*.115;
   float y1=sin(p.x*(2.1+detail*1.2)+flow)*amplitude+sin(p.x*5-flow*.7)*detail*.012;
   float y2=sin(p.x*1.55-flow*.82+2.2)*(amplitude*.64)-.21-impact*.035;
   float ribbon1=lineGlow(p.y-y1,.003+impact*.018);float ribbon2=lineGlow(p.y-y2,.0025+motion*.008);
   float horizon=lineGlow(p.y+.39-impact*.06,.0015+beat*.005)*(1-smoothstep(.25,.95,abs(p.x)));
   c+=ribbon1*cyan*(.30+impact*1.25)+ribbon2*lime*(.10+motion*.72)+horizon*float3(.10,.17,.42);
   c+=float3(.00,.05,.08)*motion*smoothstep(.48,0,abs(p.y-y1));
 }else if(s<2.5){
   float phase=t*(.04+motion*.92);float cs=cos(.34),sn=sin(.34);float2 q=float2(p.x*cs-p.y*sn,p.x*sn+p.y*cs);
   float2 q2=float2(p.x*cs+p.y*sn,-p.x*sn+p.y*cs);float orbitRadius=.45+impact*.08;
   float orbit1=lineGlow(length(q*float2(1,.57))-orbitRadius,.003+motion*.010);
   float orbit2=lineGlow(length(q2*float2(1,.70))-(.53-impact*.025),.0025+detail*.006);
   float markerAngle=phase*2.4;float2 marker=float2(cos(markerAngle),sin(markerAngle)*.57)*orbitRadius;float markerGlow=.004/max(dot(q-marker,q-marker),.002);
   float shock=ring(p,.31+beat*.15,.0025+beat*.017);
   c+=orbit1*cyan*(.23+motion*.90)+orbit2*lime*(.10+detail*.55)+markerGlow*float3(.18,.70,1)*(.20+detail)+shock*violet*(.15+beat);
 }else if(s<3.5){
   float row=floor((p.y+.55)*24);float gate=step(.54,hash(float2(row,floor(t*(5+flux*28)))));float slice=(hash(float2(row,floor(t*17)))-.5)*transient*.24*gate;
   float scanY=frac(p.y*17-t*(.10+motion*2.4));float scan=pow(saturate(1-abs(scanY-.5)*2),20)*(.025+detail*.12);
   float wave=sin((p.x+slice)*(8+detail*5)+t*(.10+motion*3.1)+row*.22)*(.025+motion*.14);
   float signal=lineGlow(p.y*.72-wave,.0025+impact*.014);float blocks=gate*transient*step(.72,hash(float2(floor((p.x+1)*14),row)));
   c+=signal*cyan*(.25+impact*1.45)+scan*float3(.08,.26,.42)+blocks*float3(.02,.16,.22);
   c+=lineGlow(abs(p.y)-(.34+beat*.08),.0015+beat*.006)*violet*(.12+transient*.6);
 }else if(s<4.5){
   float phase=t*(.035+motion*.85);float lobes=sin(a*6-phase*2.2);float radius=.41+lobes*(.012+motion*.045)+impact*.075;
   float bloom=ring(p,radius,.003+impact*.019);float petals=pow(saturate(cos(a*6-phase)*.5+.5),18)*detail*(.015+beat*.045)/max(r,.15);
   float rayGate=step(.90-detail*.10,hash(float2(floor((a+3.2)*24),floor(t*(2+detail*10)))));float rays=rayGate*detail*.008/max(abs(frac(r*3.2-t*motion*.6)-.5),.035);
   c+=bloom*cyan*(.38+impact*1.60)+petals*lime+rays*float3(.14,.34,.72);
   c+=ring(p,.55+beat*.11,.002+beat*.010)*violet*(.10+beat*.75);
 }else if(s<5.5){
   float lane=floor((p.y+.52)*12);float laneCenter=(lane+.5)/12-.52;float rail=lineGlow(p.y-laneCenter,.0012+detail*.0025)*(1-smoothstep(.55,.98,abs(p.x)));
   float travel=frac(t*(.018+motion*.72)+hash(float2(lane,4)));float packetX=(-aspect*.62)+travel*(aspect*1.24);float2 packet=float2(packetX,laneCenter);
   float packetGlow=.0018/max(dot(p-packet,p-packet),.0015);float sync=lineGlow(p.x,.001+beatFlow*.004)*(1-smoothstep(.05,.48,abs(p.y)));
   c+=rail*float3(.02,.13,.20)+packetGlow*lerp(cyan,lime,hash(float2(lane,8)))*(.08+motion*.55)+sync*violet*(.05+beatFlow*.35);
 }else if(s<6.5){
   float tunnel=frac(t*(.012+motion*.36));float z1=.34+tunnel*.18;float z2=.52+tunnel*.18;float z3=.70+tunnel*.18;
   float b1=boxRing(p,float2(z1*aspect*.56,z1),.002+impact*.006);float b2=boxRing(p,float2(z2*aspect*.56,z2),.0018+motion*.004);float b3=boxRing(p,float2(z3*aspect*.56,z3),.0014+detail*.003);
   float cross=lineGlow(abs(p.x/aspect)-abs(p.y)*.56,.0012+detail*.002)*(1-smoothstep(.18,.95,r));
   c+=b1*cyan*(.22+impact*.75)+b2*lime*(.08+motion*.38)+b3*violet*(.08+detail*.28)+cross*float3(.04,.12,.20);
 }else if(s<7.5){
   float drift=t*(.018+motion*.38);float auroraY=.22+sin(p.x*1.7+drift)*(.025+motion*.055)+sin(p.x*4.2-drift*.7)*detail*.014;
   float aurora=lineGlow(p.y-auroraY,.003+motion*.009);float crescent=ring(p-float2(.46,.22),.29+impact*.035,.002+beatFlow*.005)*(1-smoothstep(-.05,.35,p.y));
   float2 starCell=floor((p+float2(aspect,.55))*float2(14,16));float stars=step(.965-detail*.08,hash(starCell))*pow(.5+.5*sin(t*(.4+detail*2)+hash(starCell)*6.28),8);
   c+=aurora*lerp(cyan,lime,saturate(p.x/aspect+.5))*(.16+motion*.58)+crescent*float3(.10,.30,.54)+stars*float3(.10,.35,.48)*(.12+detail*.38);
 }else{
   float phase=t*(.020+motion*.48);float rayShape=pow(saturate(cos(a*18+phase)*.5+.5),34);float rayMask=smoothstep(.18,.72,r)*rayShape;
   float slash=lineGlow(p.y+p.x*.34-sin(phase)*.035,.0025+impact*.012)*(1-smoothstep(.62,1.05,abs(p.x)));
   float energy=ring(p,.38+impact*.095+sin(a*8-phase)*(.006+motion*.020),.0025+beatFlow*.011);
   c+=rayMask*float3(.08,.28,.58)*(.10+detail*.65)+slash*lime*(.16+impact*.78)+energy*cyan*(.32+impact*1.15);
   c+=ring(p,.52+beatFlow*.08,.0018+beatFlow*.006)*violet*(.08+beatFlow*.40);
 }

 if(transition<.999)c+=lineGlow(wipeCoordinate-wipeEdge,.0025)*lerp(cyan,violet,transient)*.12;
 float glitchActive=(s>2.5&&s<3.5)?transient:flux*.16;float logoRow=floor((p.y+.5)*28);float logoGate=step(.70,hash(float2(logoRow,floor(t*19))));
 float logoShift=(hash(float2(logoRow,floor(t*23)))-.5)*glitchActive*.12*logoGate;float logoSize=(.69+impact*.045)*logoScale;
 float2 luv=float2((p.x+logoShift)/logoSize+.5,.5+p.y/logoSize);float inside=step(0,luv.x)*step(luv.x,1)*step(0,luv.y)*step(luv.y,1);
 float3 rawLogo=logoTex.Sample(logoSampler,saturate(luv)).rgb;
 if(glitchActive>.025){float split=.004+.016*glitchActive;rawLogo.r=logoTex.Sample(logoSampler,saturate(luv+float2(split,0))).r;rawLogo.b=logoTex.Sample(logoSampler,saturate(luv-float2(split,0))).b;}
 float3 logo=saturate((rawLogo-float3(.014,.032,.060))*1.30);float logoL=max(logo.r,max(logo.g,logo.b));float logoMask=inside*saturate((logoL-.018)*3.6);
 logo*=.76+impact*.31+detail*.07;c=1-(1-saturate(c))*(1-logo*logoMask);c+=logoMask*beat*.055;
 c+=beat*float3(.010,.024,.030)*(1-smoothstep(.10,.95,r));c*=.72+.28*(1-smoothstep(.68,1.15,r));
 c=1-exp(-c);c+=(hash(i.uv*resolution+floor(t*30))-.5)*.006;return float4(pow(saturate(c),.86),1);
})";

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
    static const wchar_t* sceneName(int index){static const wchar_t*names[]={L"Pulse Engine",L"Liquid Signal",L"Orbital Drive",L"Signal Glitch",L"Impact Bloom",L"Data Current",L"Prism Tunnel",L"Celestial Drift",L"Velocity Burst"};return names[std::clamp(index,0,8)];}
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
        ComPtr<ID3DBlob> vsb,psb,err;
        if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vsb,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage vertex shader error",MB_OK|MB_ICONERROR);return false;}
        err.Reset();if(FAILED(D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&psb,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage pixel shader error",MB_OK|MB_ICONERROR);return false;}
        if(FAILED(device_->CreateVertexShader(vsb->GetBufferPointer(),vsb->GetBufferSize(),nullptr,&vs_))){StartupLog("Vertex shader creation failed");return false;}
        if(FAILED(device_->CreatePixelShader(psb->GetBufferPointer(),psb->GetBufferSize(),nullptr,&ps_))){StartupLog("Pixel shader creation failed");return false;}
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(device_->CreateBuffer(&bd,nullptr,&constants_))){StartupLog("Frame constant buffer creation failed");return false;}if(!loadLogo())return false;RECT r;GetClientRect(hwnd_,&r);resize(r.right,r.bottom);return rtv_.Get()!=nullptr;}
    void resize(UINT w,UINT h){if(!w||!h)return;rtv_.Reset();context_->OMSetRenderTargets(0,nullptr,nullptr);swap_->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0);ComPtr<ID3D11Texture2D>b;swap_->GetBuffer(0,IID_PPV_ARGS(&b));device_->CreateRenderTargetView(b.Get(),nullptr,&rtv_);width_=w;height_=h;layoutHud();}
    void render(){if(!rtv_){Sleep(10);return;}float seconds=elapsed();auto raw=audio_.frame();if(automatic_&&!frozen_&&seconds>=nextSceneAt_){selectScene(chooseAutoScene(raw));nextSceneAt_=seconds+settings_.sceneSeconds;}auto af=tuned(raw);float transition=settings_.transitionSeconds>0?std::clamp((seconds-transitionStarted_)/settings_.transitionSeconds,0.f,1.f):1.f;
        Constants c{{float(width_),float(height_)},seconds,float(scene_),af.bass,af.mid,af.high,af.level,af.peak,af.stereo,af.beat,af.flux,float(previousScene_),transition,settings_.logoScale,float(settings_.palette)};D3D11_MAPPED_SUBRESOURCE map{};context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map);memcpy(map.pData,&c,sizeof(c));context_->Unmap(constants_.Get(),0);
        D3D11_VIEWPORT vp{0,0,float(width_),float(height_),0,1};context_->RSSetViewports(1,&vp);context_->OMSetRenderTargets(1,rtv_.GetAddressOf(),nullptr);context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetShader(ps_.Get(),nullptr,0);context_->PSSetConstantBuffers(0,1,constants_.GetAddressOf());context_->PSSetShaderResources(0,1,logoSrv_.GetAddressOf());context_->PSSetSamplers(0,1,logoSampler_.GetAddressOf());context_->Draw(3,0);swap_->Present(1,0);
        ++frames_;if(seconds-lastHud_>.10f){lastHud_=seconds;updateHud(raw);}if(seconds-lastTitle_>1){float fps=frames_/(seconds-lastTitle_);frames_=0;lastTitle_=seconds;std::wstring title=L"RareStage  |  "+std::wstring(sceneName(scene_))+(automatic_?L" [AUTO]":L" [MANUAL]")+L"  |  "+audio_.name()+L"  |  "+(af.bpm>0?std::to_wstring((int)af.bpm)+L" BPM  |  ":L"")+std::to_wstring((int)fps)+L" FPS";SetWindowTextW(hwnd_,title.c_str());}}
    void fullscreen(){fullscreen_=!fullscreen_;if(fullscreen_){GetWindowRect(hwnd_,&windowRect_);SetWindowLongW(hwnd_,GWL_STYLE,WS_POPUP);auto monitors=monitorRects();if(monitors.empty())return;settings_.monitor%=monitors.size();RECT target=monitors[settings_.monitor];SetWindowPos(hwnd_,HWND_TOP,target.left,target.top,target.right-target.left,target.bottom-target.top,SWP_FRAMECHANGED);}else{SetWindowLongW(hwnd_,GWL_STYLE,WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd_,nullptr,windowRect_.left,windowRect_.top,windowRect_.right-windowRect_.left,windowRect_.bottom-windowRect_.top,SWP_FRAMECHANGED|SWP_NOZORDER);}}
    HWND hwnd_{},hud_{};HFONT hudFont_{};HBRUSH hudBrush_{};UINT width_=1280,height_=720;bool fullscreen_=false,automatic_=true,frozen_=false,hudVisible_=true;RECT windowRect_{};int scene_=0,previousScene_=0,hudSelection_=0,autoSequence_=0,monitorCount_=1;AudioEngine audio_;ShowSettings settings_;
    Clock::time_point started_;float lastTitle_{},lastHud_{},transitionStarted_{-100},nextSceneAt_{18};int frames_{};ComPtr<ID3D11Device>device_;ComPtr<ID3D11DeviceContext>context_;ComPtr<IDXGISwapChain>swap_;ComPtr<ID3D11RenderTargetView>rtv_;ComPtr<ID3D11VertexShader>vs_;ComPtr<ID3D11PixelShader>ps_;ComPtr<ID3D11Buffer>constants_;ComPtr<ID3D11ShaderResourceView>logoSrv_;ComPtr<ID3D11SamplerState>logoSampler_;
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

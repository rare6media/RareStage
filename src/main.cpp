#include <winsock2.h>
#include <ws2tcpip.h>
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

#include "rarestage_control.h"
#include "show_director.h"
#include "show_memory.h"

#ifdef RARESTAGE_SPOUT
#include <SpoutDX.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <cwchar>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
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

struct AudioFrame {
    float bass{}, mid{}, high{}, level{}, peak{}, stereo{}, beat{}, flux{}, onset{}, percussion{}, snare{}, kick{};
    float bpm{}, tempoConfidence{}, kickConfidence{}, snareConfidence{}, hatConfidence{};
    float kickEvidence{}, snareEvidence{}, hatEvidence{}, transientAmbiguity{}, detectorThreshold{};
    float calibrationStability{}, calibrationProgress{}, analysisLatencyMs{}, sampleRate{};
    float lowFluxShare{}, midFluxShare{}, highFluxShare{}, lowPowerShare{}, midPowerShare{}, highPowerShare{};
    std::uint32_t acceptedKickCount{}, acceptedSnareCount{}, acceptedHatCount{}, rejectedTransientCount{};
    int transientClass{}, requestedProfile{}, resolvedProfile{1};
};

#pragma pack(push,1)
struct StageSyncPacket {
    std::uint32_t magic{0x32315352u};
    std::uint16_t version{3};
    std::uint16_t size{68};
    std::uint32_t sequence{};
    std::uint32_t groupId{1};
    std::int32_t world{}, section{}, palette{}, productionDuty{}, masteringProfile{}, stageLayout{}, reactionProfile{}, transitionStyle{};
    float tempoPhase{}, bpm{120.0f}, cueDensity{1.0f}, masterBrightness{0.92f}, transitionSeconds{1.4f};
};
#pragma pack(pop)
static_assert(std::is_trivially_copyable_v<StageSyncPacket> && sizeof(StageSyncPacket) == 68);
static bool ValidStageSyncPacket(const StageSyncPacket&packet,std::uint32_t groupId){return packet.magic==0x32315352u&&packet.version==3&&packet.size==static_cast<std::uint16_t>(sizeof(StageSyncPacket))&&packet.groupId==groupId&&packet.world>=0&&packet.world<21&&packet.section>=0&&packet.section<5&&packet.palette>=0&&packet.palette<8&&packet.productionDuty>=0&&packet.productionDuty<5&&packet.masteringProfile>=0&&packet.masteringProfile<4&&packet.stageLayout>=0&&packet.stageLayout<4&&packet.reactionProfile>=0&&packet.reactionProfile<4&&packet.transitionStyle>=0&&packet.transitionStyle<6&&std::isfinite(packet.tempoPhase)&&packet.tempoPhase>=0&&std::isfinite(packet.bpm)&&packet.bpm>=65&&packet.bpm<=190&&std::isfinite(packet.cueDensity)&&packet.cueDensity>=.65f&&packet.cueDensity<=1.25f&&std::isfinite(packet.masterBrightness)&&packet.masterBrightness>=.65f&&packet.masterBrightness<=1.f&&std::isfinite(packet.transitionSeconds)&&packet.transitionSeconds>=.2f&&packet.transitionSeconds<=8.f;}

class StageSync {
public:
    ~StageSync(){stop();}
    bool setMode(int mode){
        mode=std::clamp(mode,0,2);if(mode==mode_&&(mode==0||socket_!=INVALID_SOCKET))return true;stop();lastAcceptedSequence_=0;hasAcceptedSequence_=false;mode_=mode;if(mode_==0)return true;
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0){mode_=0;return false;}winsockStarted_=true;
        socket_=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(socket_==INVALID_SOCKET){stop();return false;}
        u_long nonBlocking=1;if(ioctlsocket(socket_,FIONBIO,&nonBlocking)!=0){stop();return false;}
        if(mode_==1){BOOL enabled=TRUE;if(setsockopt(socket_,SOL_SOCKET,SO_BROADCAST,reinterpret_cast<const char*>(&enabled),sizeof(enabled))!=0){stop();return false;}broadcast_={};broadcast_.sin_family=AF_INET;broadcast_.sin_port=htons(47712);broadcast_.sin_addr.s_addr=INADDR_BROADCAST;}
        else{BOOL reuse=TRUE;setsockopt(socket_,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));sockaddr_in local{};local.sin_family=AF_INET;local.sin_port=htons(47712);local.sin_addr.s_addr=htonl(INADDR_ANY);if(bind(socket_,reinterpret_cast<const sockaddr*>(&local),sizeof(local))!=0){stop();return false;}}
        return true;
    }
    void stop(){if(socket_!=INVALID_SOCKET){closesocket(socket_);socket_=INVALID_SOCKET;}if(winsockStarted_){WSACleanup();winsockStarted_=false;}mode_=0;}
    bool broadcast(const StageSyncPacket&packet){if(mode_!=1||socket_==INVALID_SOCKET)return false;constexpr int PacketBytes=static_cast<int>(sizeof(StageSyncPacket));return sendto(socket_,reinterpret_cast<const char*>(&packet),PacketBytes,0,reinterpret_cast<const sockaddr*>(&broadcast_),sizeof(broadcast_))==PacketBytes;}
    bool receive(StageSyncPacket&packet,std::uint32_t groupId){if(mode_!=2||socket_==INVALID_SOCKET)return false;bool received=false;for(int attempt=0;attempt<12;++attempt){StageSyncPacket candidate{};sockaddr_in source{};int sourceSize=sizeof(source);constexpr int PacketBytes=static_cast<int>(sizeof(StageSyncPacket));int bytes=recvfrom(socket_,reinterpret_cast<char*>(&candidate),PacketBytes,0,reinterpret_cast<sockaddr*>(&source),&sourceSize);if(bytes==SOCKET_ERROR)break;const bool newer=!hasAcceptedSequence_||static_cast<std::int32_t>(candidate.sequence-lastAcceptedSequence_)>0;if(bytes==PacketBytes&&ValidStageSyncPacket(candidate,groupId)&&newer){packet=candidate;lastAcceptedSequence_=candidate.sequence;hasAcceptedSequence_=true;received=true;}}return received;}
    int mode()const{return mode_;}
private:
    SOCKET socket_{INVALID_SOCKET};sockaddr_in broadcast_{};bool winsockStarted_{},hasAcceptedSequence_{};int mode_{};std::uint32_t lastAcceptedSequence_{};
};

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
    static constexpr size_t FftSize = 2048;
    static constexpr size_t HopSize = FftSize / 2;
    static constexpr size_t SpectrumSize = FftSize / 2 + 1;

    ~AudioEngine() { stop(); }
    bool start(size_t requested = 0) {
        stop(); deviceIndex_ = requested; resetAnalysis();
        {std::scoped_lock lock(mutex_);frame_={};name_=L"Starting audio capture...";}
        running_ = true;
        worker_ = std::thread([this] { capture(); }); return true;
    }
    void stop() { running_ = false; if (worker_.joinable()) worker_.join(); }
    void cycle() { auto count = deviceCount(); start(count ? (deviceIndex_ + 1) % count : 0); }
    void setProfile(int profile) { detectorProfile_.store(std::clamp(profile,0,3)); }
    AudioFrame frame() const { std::scoped_lock lock(mutex_);AudioFrame copy=frame_;if(lastFrameAt_.time_since_epoch().count()){const float age=std::chrono::duration<float>(Clock::now()-lastFrameAt_).count();if(age>.12f){const float decay=std::exp(-(age-.12f)*4.5f);copy.bass*=decay;copy.mid*=decay;copy.high*=decay;copy.level*=decay;copy.peak*=decay;copy.beat*=decay;copy.flux*=decay;copy.onset*=decay;copy.percussion*=decay;copy.snare*=decay;copy.kick*=decay;copy.tempoConfidence*=std::exp(-(age-.12f)*.7f);}}return copy; }
    std::wstring name() const { std::scoped_lock lock(mutex_); return name_; }

    static bool classifierSelfTest() {
        auto classify = [&](int type,float sampleRate,int profile) {
            AudioEngine engine; engine.resetAnalysis();
            engine.setProfile(profile);
            std::array<float,FftSize> signal{};
            for(size_t i=0;i<signal.size();++i){
                const float x=float(i), seconds=x/sampleRate;
                if(type==0){
                    const float envelope=std::exp(-x/230.f);
                    signal[i]=std::sin(2*PI*(72.f-18.f*x/FftSize)*seconds)*envelope*.92f;
                }else if(type==1){
                    const float envelope=std::exp(-x/105.f);
                    signal[i]=(std::sin(2*PI*7200.f*seconds)+std::sin(2*PI*10300.f*seconds)+
                               std::sin(2*PI*14700.f*seconds))*envelope*.24f;
                }else{
                    const float envelope=std::exp(-x/270.f);
                    signal[i]=(std::sin(2*PI*230.f*seconds)*.34f+std::sin(2*PI*1750.f*seconds)*.31f+
                               std::sin(2*PI*3900.f*seconds)*.27f)*envelope;
                }
            }
            engine.analyze(signal,0.f,sampleRate);
            return engine.frame();
        };
        constexpr std::array<float,3> SampleRates{44100.0f,48000.0f,96000.0f};
        for(const float sampleRate:SampleRates){
            for(int profile=0;profile<4;++profile){
                const auto kick=classify(0,sampleRate,profile),hat=classify(1,sampleRate,profile),snare=classify(2,sampleRate,profile);
                if(!(kick.transientClass==1&&kick.kickEvidence>kick.snareEvidence&&kick.kickEvidence>kick.hatEvidence&&
                     hat.transientClass==3&&hat.hatEvidence>hat.kickEvidence&&hat.hatEvidence>hat.snareEvidence&&
                     snare.transientClass==2&&snare.snareEvidence>snare.kickEvidence&&snare.snareEvidence>snare.hatEvidence))return false;
            }
            AudioEngine sustained; sustained.resetAnalysis(); sustained.setProfile(1);std::array<float,FftSize> tone{};
            for(size_t i=0;i<tone.size();++i)tone[i]=std::sin(2*PI*82.f*float(i)/sampleRate)*.52f;
            for(int frame=0;frame<10;++frame)sustained.analyze(tone,0.f,sampleRate);const auto held=sustained.frame();
            if(!(held.bass>.12f&&held.kick<held.bass*.70f))return false;
        }
        return true;
    }

private:
    size_t deviceCount() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ComPtr<IMMDeviceEnumerator> e; ComPtr<IMMDeviceCollection> c; UINT n = 0;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&e))) &&
            SUCCEEDED(e->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &c))) c->GetCount(&n);
        CoUninitialize(); return n;
    }
    void resetAnalysis() {
        ceilings_.fill(.000001f);floors_.fill(0);previousSpectrum_.fill(0);fluxMeans_.fill(0);fluxPeaks_.fill(.0001f);beatIntervals_.fill(0);
        previousRms_=beatEnvelope_=fluxEnvelope_=kickEnvelope_=snareEnvelope_=hatEnvelope_=bassSustain_=beatInterval_=tempoConfidence_=kickConfidence_=snareConfidence_=hatConfidence_=profileLow_=profileMid_=profileHigh_=0;
        beatCooldown_=beatIntervalCount_=beatIntervalIndex_=analysisFrames_=0;acceptedKickCount_=acceptedSnareCount_=acceptedHatCount_=rejectedTransientCount_=0;analysisSampleCursor_=lastBeatSample_=0;lastFrameAt_={};
    }
    static void fft(std::array<std::complex<float>, FftSize>& a) {
        for (size_t i=1,j=0;i<a.size();++i) { size_t bit=a.size()>>1; for(;j&bit;bit>>=1)j^=bit; j^=bit; if(i<j)std::swap(a[i],a[j]); }
        for(size_t len=2;len<=a.size();len<<=1) { auto wlen=std::polar(1.0f,-2.0f*PI/float(len));
            for(size_t i=0;i<a.size();i+=len){std::complex<float>w(1);for(size_t j=0;j<len/2;++j){auto u=a[i+j],v=a[i+j+len/2]*w;a[i+j]=u+v;a[i+j+len/2]=u-v;w*=wlen;}}
        }
    }
    void analyze(const std::array<float,FftSize>& mono, float stereo, float sampleRate) {
        std::array<std::complex<float>,FftSize> bins{};float level=0,peak=0;
        for(size_t i=0;i<mono.size();++i){const float sample=mono[i];level+=sample*sample;peak=std::max(peak,std::abs(sample));const float window=.5f-.5f*std::cos(2*PI*float(i)/float(FftSize-1));bins[i]={sample*window,0};}
        fft(bins);std::array<float,SpectrumSize> magnitudes{};for(size_t i=0;i<magnitudes.size();++i)magnitudes[i]=std::abs(bins[i]);
        auto range=[&](float lowHz,float highHz){const float hzPerBin=sampleRate/float(FftSize);int low=std::clamp(int(std::ceil(lowHz/hzPerBin)),1,int(SpectrumSize)-1);int high=std::clamp(int(std::floor(highHz/hzPerBin)),low,int(SpectrumSize)-1);return std::pair<int,int>{low,high};};
        auto powerBand=[&](float lowHz,float highHz){const auto [low,high]=range(lowHz,highHz);float power=0;for(int i=low;i<=high;++i)power+=magnitudes[i]*magnitudes[i];return power;};
        auto energyBand=[&](float lowHz,float highHz){const auto [low,high]=range(lowHz,highHz);return std::sqrt(powerBand(lowHz,highHz)/float(high-low+1));};
        auto positiveFluxBand=[&](float lowHz,float highHz){const auto [low,high]=range(lowHz,highHz);float positive=0;for(int i=low;i<=high;++i){const float delta=std::max(0.f,magnitudes[i]-previousSpectrum_[i]);positive+=delta*delta;}return positive;};
        const float rms=std::sqrt(level/float(FftSize));const float audible=saturateAudio((rms-.00018f)*230.f);
        std::array<float,8> raw{energyBand(25,80),energyBand(45,145),energyBand(70,230),energyBand(230,700),energyBand(700,2800),energyBand(2800,6000),energyBand(6000,std::min(18000.f,sampleRate*.46f)),rms};
        std::array<float,8> normalized{};
        for(size_t i=0;i<raw.size();++i){floors_[i]+=(raw[i]-floors_[i])*(raw[i]<floors_[i]?.075f:.00045f);ceilings_[i]=std::max(raw[i],std::max(ceilings_[i]*.9965f,floors_[i]+.000001f));const float span=std::max(ceilings_[i]-floors_[i],.000001f);normalized[i]=saturateAudio((raw[i]-floors_[i]*1.05f)/(span*1.18f))*audible;}
        auto adaptiveFlux=[&](int index,float rawValue){fluxMeans_[index]+=(rawValue-fluxMeans_[index])*.070f;fluxPeaks_[index]=std::max(rawValue,fluxPeaks_[index]*.925f);return saturateAudio((rawValue-fluxMeans_[index]*.72f)/std::max(.012f,fluxPeaks_[index]*.56f));};
        const float highLimit=std::min(19000.f,sampleRate*.46f);const float lowFluxPower=positiveFluxBand(35,190),midFluxPower=positiveFluxBand(190,5600),highFluxPower=positiveFluxBand(5600,highLimit);const float totalFluxPower=lowFluxPower+midFluxPower+highFluxPower;const float lowPower=powerBand(35,190),midPower=powerBand(190,5600),highPower=powerBand(5600,highLimit),totalPower=lowPower+midPower+highPower;const float onsetStrength=adaptiveFlux(0,std::sqrt(totalFluxPower/std::max(totalPower,.000001f)))*audible;const float lowFluxShare=lowFluxPower/std::max(totalFluxPower,.000001f),midFluxShare=midFluxPower/std::max(totalFluxPower,.000001f),highFluxShare=highFluxPower/std::max(totalFluxPower,.000001f);const float lowShare=lowPower/std::max(totalPower,.000001f),midShare=midPower/std::max(totalPower,.000001f),highShare=highPower/std::max(totalPower,.000001f);
        previousSpectrum_=magnitudes;
        const float sub=normalized[0],kickBody=normalized[1],bassTone=normalized[2],lowMid=normalized[3],midBand=normalized[4],presence=normalized[5],air=normalized[6];
        const float bass=saturateAudio((sub*.18f+kickBody*.38f+bassTone*.44f)*std::sqrt(lowShare)*1.28f),mid=saturateAudio((lowMid*.38f+midBand*.62f)*std::sqrt(midShare)*1.28f),high=saturateAudio((presence*.54f+air*.46f)*std::sqrt(highShare)*1.28f);
        const float crest=saturateAudio((peak/std::max(rms,.00001f)-1.25f)/3.5f),rmsDelta=std::max(0.f,rms-previousRms_);previousRms_=rms;
        profileLow_+=(lowShare-profileLow_)*.015f;profileMid_+=(midShare-profileMid_)*.015f;profileHigh_+=(highShare-profileHigh_)*.015f;const int requestedProfile=detectorProfile_.load();int resolvedProfile=requestedProfile;if(!resolvedProfile){resolvedProfile=profileLow_>.54f?2:(profileMid_>.62f&&profileHigh_<.24f?3:1);}const float kickScale=resolvedProfile==2?.92f:resolvedProfile==3?1.02f:1.08f;const float snareScale=resolvedProfile==2?1.04f:resolvedProfile==3?1.12f:1.0f;const float hatScale=resolvedProfile==2?.94f:resolvedProfile==3?.96f:1.08f;
        const float kickIdentity=saturateAudio((lowFluxShare*1.16f+lowShare*.72f+crest*.16f-midFluxShare*.28f-highFluxShare*.62f)*kickScale);const float snareIdentity=saturateAudio((midFluxShare*1.04f+midShare*.58f+crest*.10f-lowFluxShare*.38f-highFluxShare*.10f)*snareScale);const float hatIdentity=saturateAudio((highFluxShare*1.18f+highShare*.66f+crest*.08f-lowFluxShare*.70f-midShare*.20f)*hatScale);
        const float rmsAttack=saturateAudio(rmsDelta*38.f);float kickEvidence=saturateAudio(onsetStrength*kickIdentity*(.76f+.24f*crest)+rmsAttack*kickIdentity*.12f);float snareEvidence=saturateAudio(onsetStrength*snareIdentity*(.88f+.12f*crest));float hatEvidence=saturateAudio(onsetStrength*hatIdentity*(.92f+.08f*crest));
        std::array<float,3> evidence{kickEvidence,snareEvidence,hatEvidence};int winner=0;if(evidence[1]>evidence[winner])winner=1;if(evidence[2]>evidence[winner])winner=2;float runner=0;for(int index=0;index<3;++index)if(index!=winner)runner=std::max(runner,evidence[index]);++analysisFrames_;const float calibrationProgress=std::min(1.f,float(analysisFrames_)/180.f);float dynamicReady=0;for(size_t index=0;index<7;++index)dynamicReady+=saturateAudio((ceilings_[index]-floors_[index])/std::max(ceilings_[index],.000001f));dynamicReady/=7.f;const float calibrationStability=calibrationProgress*(.58f+.42f*dynamicReady);const float profileThreshold=resolvedProfile==2?.25f:resolvedProfile==3?.23f:.24f;const float detectorThreshold=profileThreshold+(.035f*calibrationStability);const float ambiguity=runner/std::max(evidence[winner],.0001f);const bool decisive=(evidence[winner]-runner>.055f)||(evidence[winner]>.64f);const bool accepted=onsetStrength>.10f&&evidence[winner]>detectorThreshold&&decisive;const int transientClass=accepted?winner+1:0;
        if(accepted){if(winner==0)++acceptedKickCount_;else if(winner==1)++acceptedSnareCount_;else ++acceptedHatCount_;}else if(onsetStrength>.18f)++rejectedTransientCount_;
        const float laneFloor=evidence[winner]*.58f;const bool laneReady=onsetStrength>.10f;const float kickRaw=laneReady&&kickEvidence>detectorThreshold&&kickEvidence>=laneFloor?kickEvidence:0;const float snareRaw=laneReady&&snareEvidence>detectorThreshold&&snareEvidence>=laneFloor?snareEvidence:0;const float hatRaw=laneReady&&hatEvidence>detectorThreshold&&hatEvidence>=laneFloor?hatEvidence:0;
        kickConfidence_=kickConfidence_*.84f+saturateAudio(kickIdentity*(1-ambiguity*.42f))*.16f;snareConfidence_=snareConfidence_*.84f+saturateAudio(snareIdentity*(1-ambiguity*.42f))*.16f;hatConfidence_=hatConfidence_*.84f+saturateAudio(hatIdentity*(1-ambiguity*.42f))*.16f;
        const float spectralFlux=onsetStrength;const float onset=saturateAudio(std::max(kickRaw,std::max(snareRaw*.92f,hatRaw*.82f))+spectralFlux*.15f+rmsAttack*.10f);
        const float bassTarget=saturateAudio(bass*(1-kickRaw*.20f));bassSustain_+=(bassTarget-bassSustain_)*(bassTarget>bassSustain_?.22f:.045f);
        kickEnvelope_=std::max(kickRaw,kickEnvelope_*.52f);snareEnvelope_=std::max(snareRaw,snareEnvelope_*.38f);hatEnvelope_=std::max(hatRaw,hatEnvelope_*.22f);
        analysisSampleCursor_+=HopSize;if(beatCooldown_>0)--beatCooldown_;float beatTarget=0;bool beatDetected=kickRaw>detectorThreshold&&kickBody>.12f&&beatCooldown_==0;
        if(beatDetected){beatTarget=1.f;beatCooldown_=4;if(lastBeatSample_){float interval=float(analysisSampleCursor_-lastBeatSample_)/sampleRate;if(interval>=.24f&&interval<=1.35f){while(interval<.30f)interval*=2;while(interval>1.f)interval*=.5f;beatIntervals_[beatIntervalIndex_++%beatIntervals_.size()]=interval;beatIntervalCount_=std::min<int>(beatIntervalCount_+1,(int)beatIntervals_.size());auto sorted=beatIntervals_;std::sort(sorted.begin(),sorted.begin()+beatIntervalCount_);float median=sorted[(beatIntervalCount_-1)/2];float deviation=0;for(int n=0;n<beatIntervalCount_;++n)deviation+=std::abs(sorted[n]-median);deviation/=std::max(1,beatIntervalCount_);if(beatInterval_==0)beatInterval_=median;else beatInterval_+=(median-beatInterval_)*.34f;float consistency=saturateAudio(1-deviation/.085f);float tempoEvidence=saturateAudio(float(beatIntervalCount_)/6.f);tempoConfidence_=saturateAudio(tempoConfidence_*.72f+consistency*tempoEvidence*.28f);}}lastBeatSample_=analysisSampleCursor_;}
        if(lastBeatSample_&&float(analysisSampleCursor_-lastBeatSample_)/sampleRate>3.f)tempoConfidence_*=.995f;
        beatEnvelope_=beatTarget>0?beatEnvelope_+(beatTarget-beatEnvelope_)*.84f:beatEnvelope_*.68f;fluxEnvelope_=std::max(spectralFlux,fluxEnvelope_*.64f);
        AudioFrame next{};next.bass=bassSustain_;next.mid=mid;next.high=high;next.level=normalized[7];next.peak=saturateAudio(peak*2.6f);next.stereo=stereo;next.beat=beatEnvelope_;next.flux=fluxEnvelope_;next.onset=onset;next.percussion=hatEnvelope_;next.snare=snareEnvelope_;next.kick=kickEnvelope_;next.bpm=beatInterval_>0?60.f/beatInterval_:0;next.tempoConfidence=tempoConfidence_;next.kickConfidence=kickConfidence_;next.snareConfidence=snareConfidence_;next.hatConfidence=hatConfidence_;next.kickEvidence=kickEvidence;next.snareEvidence=snareEvidence;next.hatEvidence=hatEvidence;next.transientAmbiguity=ambiguity;next.detectorThreshold=detectorThreshold;next.calibrationStability=calibrationStability;next.calibrationProgress=calibrationProgress;next.analysisLatencyMs=1000.f*float(FftSize)/sampleRate;next.sampleRate=sampleRate;next.lowFluxShare=lowFluxShare;next.midFluxShare=midFluxShare;next.highFluxShare=highFluxShare;next.lowPowerShare=lowShare;next.midPowerShare=midShare;next.highPowerShare=highShare;next.transientClass=transientClass;next.acceptedKickCount=acceptedKickCount_;next.acceptedSnareCount=acceptedSnareCount_;next.acceptedHatCount=acceptedHatCount_;next.rejectedTransientCount=rejectedTransientCount_;next.requestedProfile=requestedProfile;next.resolvedProfile=resolvedProfile;
        std::scoped_lock lock(mutex_); constexpr float attack=.52f, release=.11f;
        auto smooth=[](float old,float value){float k=value>old?attack:release;return old+(value-old)*k;};
        frame_.bass=smooth(frame_.bass,next.bass);frame_.mid=smooth(frame_.mid,next.mid);frame_.high=smooth(frame_.high,next.high);frame_.level=smooth(frame_.level,next.level);
        frame_.peak=smooth(frame_.peak,next.peak);frame_.stereo=smooth(frame_.stereo,stereo);frame_.beat=std::max(next.beat,frame_.beat*.62f);frame_.flux=std::max(next.flux,frame_.flux*.64f);frame_.onset=std::max(next.onset,frame_.onset*.42f);frame_.percussion=std::max(next.percussion,frame_.percussion*.25f);frame_.snare=std::max(next.snare,frame_.snare*.40f);frame_.kick=std::max(next.kick,frame_.kick*.52f);frame_.bpm=next.bpm;frame_.tempoConfidence=smooth(frame_.tempoConfidence,next.tempoConfidence);frame_.kickConfidence=smooth(frame_.kickConfidence,next.kickConfidence);frame_.snareConfidence=smooth(frame_.snareConfidence,next.snareConfidence);frame_.hatConfidence=smooth(frame_.hatConfidence,next.hatConfidence);frame_.kickEvidence=next.kickEvidence;frame_.snareEvidence=next.snareEvidence;frame_.hatEvidence=next.hatEvidence;frame_.transientAmbiguity=next.transientAmbiguity;frame_.detectorThreshold=next.detectorThreshold;frame_.calibrationStability=next.calibrationStability;frame_.calibrationProgress=next.calibrationProgress;frame_.analysisLatencyMs=next.analysisLatencyMs;frame_.sampleRate=next.sampleRate;frame_.lowFluxShare=next.lowFluxShare;frame_.midFluxShare=next.midFluxShare;frame_.highFluxShare=next.highFluxShare;frame_.lowPowerShare=next.lowPowerShare;frame_.midPowerShare=next.midPowerShare;frame_.highPowerShare=next.highPowerShare;frame_.transientClass=next.transientClass;frame_.acceptedKickCount=next.acceptedKickCount;frame_.acceptedSnareCount=next.acceptedSnareCount;frame_.acceptedHatCount=next.acceptedHatCount;frame_.rejectedTransientCount=next.rejectedTransientCount;frame_.requestedProfile=next.requestedProfile;frame_.resolvedProfile=next.resolvedProfile;lastFrameAt_=Clock::now();
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
          std::array<float,FftSize> ring{}; size_t pos=0; float stereoAccum=0; size_t stereoCount=0;
          bool isFloat=format->wFormatTag==WAVE_FORMAT_IEEE_FLOAT || (format->wFormatTag==WAVE_FORMAT_EXTENSIBLE && IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));
          bool isPcm=format->wFormatTag==WAVE_FORMAT_PCM || (format->wFormatTag==WAVE_FORMAT_EXTENSIBLE && IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat,KSDATAFORMAT_SUBTYPE_PCM));
          auto sample=[&](const BYTE*base,UINT channel){const BYTE*p=base+channel*(format->wBitsPerSample/8);if(isFloat&&format->wBitsPerSample==32){float v;memcpy(&v,p,4);return v;}if(isPcm&&format->wBitsPerSample==16){int16_t v;memcpy(&v,p,2);return v/32768.f;}if(isPcm&&format->wBitsPerSample==24){int32_t v=int32_t(p[0])|(int32_t(p[1])<<8)|(int32_t(p[2])<<16);if(v&0x800000)v|=~0xffffff;return v/8388608.f;}if(isPcm&&format->wBitsPerSample==32){int32_t v;memcpy(&v,p,4);return v/2147483648.f;}return 0.f;};
          while(running_){UINT32 packets=0;captureClient->GetNextPacketSize(&packets);if(!packets){Sleep(3);continue;} BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;
            if(FAILED(captureClient->GetBuffer(&data,&frames,&flags,nullptr,nullptr)))continue;
            for(UINT32 f=0;f<frames;++f){float l=0,r=0;if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT)){const BYTE*base=data+size_t(f)*format->nBlockAlign;l=sample(base,0);r=format->nChannels>1?sample(base,1):l;}
              ring[pos++]=(l+r)*.5f;stereoAccum+=std::abs(l-r);++stereoCount;if(pos==ring.size()){analyze(ring,stereoCount?std::min(stereoAccum/stereoCount*4,1.f):0,float(format->nSamplesPerSec));std::move(ring.begin()+HopSize,ring.end(),ring.begin());pos=HopSize;stereoAccum=0;stereoCount=0;}}
            captureClient->ReleaseBuffer(frames);
          } client->Stop();freeFormat(); }
    done: if(!captureStarted&&running_){std::scoped_lock lock(mutex_);name_=L"Audio capture unavailable - press D";}if(mmcss)AvRevertMmThreadCharacteristics(mmcss); CoUninitialize();
    }
    std::atomic<bool> running_{false};std::atomic<int> detectorProfile_{0}; std::thread worker_; size_t deviceIndex_{};
    mutable std::mutex mutex_;AudioFrame frame_{};std::wstring name_=L"Starting audio capture...";std::array<float,8>ceilings_{};std::array<float,8>floors_{};std::array<float,SpectrumSize>previousSpectrum_{};std::array<float,3>fluxMeans_{};std::array<float,3>fluxPeaks_{};std::array<float,8>beatIntervals_{};float previousRms_{},beatEnvelope_{},fluxEnvelope_{},kickEnvelope_{},snareEnvelope_{},hatEnvelope_{},bassSustain_{},beatInterval_{},tempoConfidence_{},kickConfidence_{},snareConfidence_{},hatConfidence_{},profileLow_{},profileMid_{},profileHigh_{};std::uint32_t acceptedKickCount_{},acceptedSnareCount_{},acceptedHatCount_{},rejectedTransientCount_{};std::uint64_t analysisSampleCursor_{},lastBeatSample_{};int beatCooldown_{},beatIntervalCount_{},beatIntervalIndex_{},analysisFrames_{};Clock::time_point lastFrameAt_{};
};

static std::filesystem::path SettingsPath(){
    wchar_t local[MAX_PATH]{};DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH);std::filesystem::path directory;
    if(length>0&&length<MAX_PATH)directory=std::filesystem::path(local)/L"RareStage";else{wchar_t module[MAX_PATH]{};GetModuleFileNameW(nullptr,module,MAX_PATH);directory=std::filesystem::path(module).parent_path();}
    std::error_code error;std::filesystem::create_directories(directory,error);return directory/L"settings.ini";
}

struct ShowSettings {
    int profile=1,audioProfile=0,palette=0,monitor=0,automatic=1,quality=2,vsync=1,timeline=0,spout=1,transitionStyle=0,masteringProfile=1,stageLayout=0,productionDuty=1,syncMode=0,syncGroup=1,sceneCategory=0,tempoAnchor=0,djIdentity=1;
    float sensitivity=1,bassGain=1,midGain=1,highGain=1,beatGain=1,logoScale=1,sceneSeconds=72,transitionSeconds=1.4f,cueDensity=1,masterBrightness=.92f;
    unsigned mirrorMask{};
    std::wstring djName=L"RARE";
    static float parseFloat(const wchar_t*text,float fallback){wchar_t*end=nullptr;const float value=std::wcstof(text,&end);return end!=text&&*end==L'\0'&&std::isfinite(value)?value:fallback;}
    static float readFloat(const std::wstring&path,const wchar_t*key,float fallback){wchar_t text[32]{};wchar_t fallbackText[32]{};std::swprintf(fallbackText,std::size(fallbackText),L"%.2f",fallback);GetPrivateProfileStringW(L"Show",key,fallbackText,text,(DWORD)std::size(text),path.c_str());return parseFloat(text,fallback);}
    static float readFloat(const std::wstring&path,const wchar_t*section,const wchar_t*key,float fallback){wchar_t text[32]{};wchar_t fallbackText[32]{};std::swprintf(fallbackText,std::size(fallbackText),L"%.2f",fallback);GetPrivateProfileStringW(section,key,fallbackText,text,(DWORD)std::size(text),path.c_str());return parseFloat(text,fallback);}
    void load(const wchar_t*section=L"Show"){auto path=SettingsPath().wstring();mirrorMask=GetPrivateProfileIntW(section,L"MirrorMask",0,path.c_str())&255u;profile=std::clamp(int(GetPrivateProfileIntW(section,L"Profile",profile,path.c_str())),0,3);audioProfile=std::clamp(int(GetPrivateProfileIntW(section,L"AudioProfile",audioProfile,path.c_str())),0,3);palette=std::clamp(int(GetPrivateProfileIntW(section,L"Palette",palette,path.c_str())),0,7);monitor=std::max(0,int(GetPrivateProfileIntW(section,L"Monitor",monitor,path.c_str())));automatic=GetPrivateProfileIntW(section,L"Automatic",automatic,path.c_str())?1:0;if(std::wcscmp(section,L"Show")==0&&!GetPrivateProfileIntW(section,L"TimedCycleVersion",0,path.c_str()))automatic=1;quality=std::clamp(int(GetPrivateProfileIntW(section,L"Quality",quality,path.c_str())),0,3);vsync=GetPrivateProfileIntW(section,L"VSync",vsync,path.c_str())?1:0;timeline=GetPrivateProfileIntW(section,L"Timeline",timeline,path.c_str())?1:0;spout=GetPrivateProfileIntW(section,L"Spout",spout,path.c_str())?1:0;transitionStyle=std::clamp(int(GetPrivateProfileIntW(section,L"TransitionStyle",transitionStyle,path.c_str())),0,3);masteringProfile=std::clamp(int(GetPrivateProfileIntW(section,L"MasteringProfile",masteringProfile,path.c_str())),0,3);stageLayout=std::clamp(int(GetPrivateProfileIntW(section,L"StageLayout",stageLayout,path.c_str())),0,3);productionDuty=std::clamp(int(GetPrivateProfileIntW(section,L"ProductionDuty",productionDuty,path.c_str())),0,4);syncMode=std::clamp(int(GetPrivateProfileIntW(section,L"SyncMode",syncMode,path.c_str())),0,2);syncGroup=std::clamp(int(GetPrivateProfileIntW(section,L"SyncGroup",syncGroup,path.c_str())),1,9999);sceneCategory=std::clamp(int(GetPrivateProfileIntW(section,L"SceneCategory",sceneCategory,path.c_str())),0,4);tempoAnchor=std::clamp(int(GetPrivateProfileIntW(section,L"TempoAnchor",tempoAnchor,path.c_str())),0,2);djIdentity=GetPrivateProfileIntW(section,L"DjIdentity",djIdentity,path.c_str())?1:0;wchar_t name[32]{};GetPrivateProfileStringW(section,L"DjName",djName.c_str(),name,(DWORD)std::size(name),path.c_str());djName=name;
        sensitivity=std::clamp(readFloat(path,section,L"Sensitivity",sensitivity),.50f,1.50f);bassGain=std::clamp(readFloat(path,section,L"Bass",bassGain),.50f,1.50f);midGain=std::clamp(readFloat(path,section,L"Mids",midGain),.50f,1.50f);highGain=std::clamp(readFloat(path,section,L"Highs",highGain),.50f,1.50f);beatGain=std::clamp(readFloat(path,section,L"Beat",beatGain),.50f,1.50f);logoScale=std::clamp(readFloat(path,section,L"Logo",logoScale),.75f,1.25f);sceneSeconds=std::clamp(readFloat(path,section,L"SceneSeconds",sceneSeconds),40.f,120.f);transitionSeconds=std::clamp(readFloat(path,section,L"TransitionSeconds",transitionSeconds),.2f,3.f);cueDensity=std::clamp(readFloat(path,section,L"CueDensity",cueDensity),.65f,1.25f);masterBrightness=std::clamp(readFloat(path,section,L"MasterBrightness",masterBrightness),.65f,1.f);}
    void save(const wchar_t*section=L"Show")const{auto path=SettingsPath().wstring();auto writeInt=[&](const wchar_t*key,int value){wchar_t text[16]{};std::swprintf(text,std::size(text),L"%d",value);WritePrivateProfileStringW(section,key,text,path.c_str());};auto writeFloat=[&](const wchar_t*key,float value){wchar_t text[32]{};std::swprintf(text,std::size(text),L"%.2f",value);WritePrivateProfileStringW(section,key,text,path.c_str());};
        writeInt(L"Profile",profile);writeInt(L"AudioProfile",audioProfile);writeInt(L"Palette",palette);writeInt(L"Monitor",monitor);writeInt(L"MirrorMask",int(mirrorMask));writeInt(L"Automatic",automatic);writeInt(L"TimedCycleVersion",1);writeInt(L"Quality",quality);writeInt(L"VSync",vsync);writeInt(L"Timeline",timeline);writeInt(L"Spout",spout);writeInt(L"TransitionStyle",transitionStyle);writeInt(L"MasteringProfile",masteringProfile);writeInt(L"StageLayout",stageLayout);writeInt(L"ProductionDuty",productionDuty);writeInt(L"SyncMode",syncMode);writeInt(L"SyncGroup",syncGroup);writeInt(L"SceneCategory",sceneCategory);writeInt(L"TempoAnchor",tempoAnchor);writeInt(L"DjIdentity",djIdentity);WritePrivateProfileStringW(section,L"DjName",djName.c_str(),path.c_str());writeFloat(L"Sensitivity",sensitivity);writeFloat(L"Bass",bassGain);writeFloat(L"Mids",midGain);writeFloat(L"Highs",highGain);writeFloat(L"Beat",beatGain);writeFloat(L"Logo",logoScale);writeFloat(L"SceneSeconds",sceneSeconds);writeFloat(L"TransitionSeconds",transitionSeconds);writeFloat(L"CueDensity",cueDensity);writeFloat(L"MasterBrightness",masterBrightness);}
};

static std::wstring SanitizeDjName(std::wstring value){
    std::wstring clean;clean.reserve(24);
    for(wchar_t character:value){
        if(clean.size()>=24)break;
        if(character>=L' '&&character!=127)clean.push_back(character);
    }
    while(!clean.empty()&&clean.front()==L' ')clean.erase(clean.begin());
    while(!clean.empty()&&clean.back()==L' ')clean.pop_back();
    return clean.empty()?L"RARE":clean;
}

static constexpr std::array<float,21> WorldCueDensity{
    .90f,.82f,.78f,1.00f,.76f,.82f,.86f,.92f,.84f,
    1.00f,.94f,.90f,.96f,.98f,.86f,.94f,.82f,.86f,.88f,.82f,.94f
};
static constexpr std::array<float,5> ActCueDensity{.76f,.98f,1.16f,.68f,1.05f};
static float MasteredCueDensity(int world,int act,float operatorDensity,float adaptiveDensity=1.f){
    return std::clamp(operatorDensity*WorldCueDensity[std::clamp(world,0,20)]*
                      ActCueDensity[std::clamp(act,0,4)]*adaptiveDensity,.55f,1.35f);
}

extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement=1; __declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1; }

static const char* Shader = R"(
cbuffer Frame : register(b0){float2 resolution;float time;float scene;float bass;float mid;float high;float level;float percussion;float snare;float beat;float flux;float previousScene;float transition;float logoScale;float palette;float transitionStyle;float tempoPhase;float tempoConfidence;float audioEnergy;float songSection;float sectionProgress;float dropPulse;float onset;float kick;float beatIndex;float barPhase;float cuePhase;float endingConfidence;float directorConfidence;float idleLife;float cueDensity;float masteringProfile;float stageLayout;float masterBrightness;float productionDuty;float visualMovement;float movementProgress;float patternSeed;float musicalNovelty;float choreographyPhase;float identityWeight;float identityKind;float stanzaProgress;float previousPatternSeed;float morphPad0;float morphPad1;float morphPad2;float dvdX;float dvdY;float dvdCorner;float dvdControlled;}
Texture2D logoTex:register(t0);Texture2D postScene:register(t1);Texture2D postBloom:register(t2);Texture2D identityTex:register(t3);Texture2D morphSource:register(t4);SamplerState linearSampler:register(s0);
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
O vs(uint id:SV_VertexID){O o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float easeBand(float v){v=saturate(v);return v*v*(3-2*v);}
float liquidEase(float v){v=saturate(v);return v*v*v*(v*(v*6-15)+10);}
float2 rotate2(float2 p,float a){float c=cos(a),s=sin(a);return float2(c*p.x-s*p.y,s*p.x+c*p.y);}
float lineGlow(float v,float w){return saturate(w/max(abs(v),.0005));}
float boxRing(float2 p,float2 size,float w){return lineGlow(max(abs(p.x)-size.x,abs(p.y)-size.y),w);}
float fillBox(float2 p,float2 size,float feather){float2 q=abs(p)-size;return 1-smoothstep(0,feather,max(q.x,q.y));}
float roundedSdf(float2 p,float2 size,float radius){float2 q=abs(p)-size+radius;return length(max(q,0))+min(max(q.x,q.y),0)-radius;}
float roundedFill(float2 p,float2 size,float radius,float feather){return 1-smoothstep(-feather,feather,roundedSdf(p,size,radius));}
float roundedRing(float2 p,float2 size,float radius,float width){return lineGlow(roundedSdf(p,size,radius),width);}
float phraseMix(float clock){return liquidEase(smoothstep(.08,.94,frac(clock)));}
float bandChase(float value,float travel,float spacing){return .5+.5*cos((value-travel)*spacing);}
float radialChase(float2 center,float travel,float spacing){return .5+.5*cos((length(center)*spacing-travel)*1.570796);}
float glassHighlight(float2 p,float2 size){float top=exp(-abs(p.y+size.y)*34)*(1-smoothstep(size.x*.68,size.x,abs(p.x)));float side=exp(-abs(p.x+size.x)*38)*(1-smoothstep(size.y*.68,size.y,abs(p.y)));return saturate(top+side*.55);}
float softDisc(float2 p,float radius,float feather){return 1-smoothstep(radius-feather,radius+feather,length(p));}
float segmentGlow(float2 p,float2 a,float2 b,float width){float2 pa=p-a,ba=b-a;float h=saturate(dot(pa,ba)/max(dot(ba,ba),.0001));return lineGlow(length(pa-ba*h),width);}
float flowField(float2 p,float phase){float2 q=p+sin(p.yx*float2(3.1,4.2)+phase)*.12;return .5+.5*sin(q.x*4.2+sin(q.y*3.4-phase*.63)+phase);}
float2 morphCell(float2 p,float phase,float scale){float2 g=p*scale;float2 id=floor(g);float2 q=frac(g)-.5;float2 group=floor(id/float2(2,2));float clock=phase*.14+dot(group,float2(.41,-.33));float morph=easeBand(.5+.5*cos(clock));float turn=.10*cos(clock*.53+dot(group,float2(.27,.31)));q=rotate2(q,turn);float circle=length(q),square=max(abs(q.x),abs(q.y));float diamond=(abs(q.x)+abs(q.y))*.72;float shape=lerp(lerp(circle,square,morph),diamond,easeBand(.5+.5*cos(clock*.67+1.8))*.28);float radius=.218;float edge=1-smoothstep(.008,.023,abs(shape-radius));float fill=(1-smoothstep(radius-.030,radius+.010,shape))*(.34+.20*morph);return float2(edge,fill);}
float ribbonSet(float2 p,float phase,float motion){float ribbons=0;[unroll]for(int k=0;k<5;k++){float lane=(k-2)*.12;float wave=lane+sin(p.x*(1.55+k*.11)+phase+k*.81)*(.028+motion*.035);ribbons+=lineGlow(p.y-wave,.0024);}return ribbons;}
float starField(float2 p,float scale,float threshold){float2 id=floor(p*scale);float2 q=frac(p*scale)-.5;float star=softDisc(q,.025+.035*hash(id+8.1),.024);return star*step(threshold,hash(id));}
float livingDust(float2 p,float phase,float scale,float threshold){float2 g=p*scale;float2 id=floor(g);float2 q=frac(g)-.5;float seed=hash(id+4.7);float orbit=phase*(.24+seed*.42)+seed*6.283185;float2 offset=float2(cos(orbit),sin(orbit))*(.08+.19*hash(id+9.2));float mote=softDisc(q-offset,.018+.030*hash(id+2.1),.020);return mote*step(threshold,hash(id+7.4));}
float filamentSet(float2 p,float phase,float motion){float strands=0;[unroll]for(int k=0;k<4;k++){float lane=(k-1.5)*.19;float path=lane+sin(p.x*(1.15+k*.18)+phase+k*1.37)*(.08+motion*.035)+sin(p.x*3.1-phase*.43+k)*.018;strands+=lineGlow(p.y-path,.0024);}return strands;}
void paletteKey(float selection,out float3 primary,out float3 accent,out float3 spectral){primary=float3(.00,.74,1.10);accent=float3(.58,1.04,.05);spectral=float3(.55,.07,.98);if(selection>.5&&selection<1.5){primary=float3(.10,.44,1.16);accent=float3(.02,.98,.72);spectral=float3(.66,.08,1.08);}else if(selection>1.5&&selection<2.5){primary=float3(1.04,.16,.60);accent=float3(.18,.84,1.06);spectral=float3(.56,.10,1.04);}else if(selection>2.5&&selection<3.5){primary=float3(1.12,.30,.03);accent=float3(1.04,.84,.08);spectral=float3(.74,.04,.24);}else if(selection>3.5&&selection<4.5){primary=float3(.25,1.05,.10);accent=float3(1.05,.70,.08);spectral=float3(.05,.55,.90);}else if(selection>4.5&&selection<5.5){primary=float3(1.08,.25,.18);accent=float3(.02,.98,.90);spectral=float3(.80,.11,.45);}else if(selection>5.5&&selection<6.5){primary=float3(.58,.16,1.05);accent=float3(.90,.92,.20);spectral=float3(.12,.65,1.02);}else if(selection>6.5){primary=float3(1.05,.66,.10);accent=float3(.15,.96,.66);spectral=float3(.76,.09,.55);}}
void rarePalette(float selection,out float3 primary,out float3 accent,out float3 spectral){float wrapped=frac(selection*.125)*8;float base=floor(wrapped),blend=easeBand(frac(wrapped));float3 p0,a0,s0,p1,a1,s1;paletteKey(base,p0,a0,s0);paletteKey(frac((base+1)*.125)*8,p1,a1,s1);primary=lerp(p0,p1,blend);accent=lerp(a0,a1,blend);spectral=lerp(s0,s1,blend);}
float4 sampleRareLogo(float2 samplePoint,float2 offset,float size,float angle){float2 q=rotate2(samplePoint-offset,-angle);float2 uv=q/size+.5;float inside=step(0,uv.x)*step(uv.x,1)*step(0,uv.y)*step(uv.y,1);float4 texel=logoTex.Sample(linearSampler,saturate(uv));return float4(texel.rgb,inside*saturate(texel.a*1.08));}
float sampleChoreographyIdentity(float2 samplePoint,float2 offset,float size,float angle){size=min(size,(resolution.x/resolution.y)*.88/(identityKind>.5?1.34:1));float2 q=rotate2(samplePoint-offset,-angle);float2 logoUv=q/size+.5;float2 nameUv=q/float2(size*1.34,size*.34)+.5;float logoInside=step(0,logoUv.x)*step(logoUv.x,1)*step(0,logoUv.y)*step(logoUv.y,1);float nameInside=step(0,nameUv.x)*step(nameUv.x,1)*step(0,nameUv.y)*step(nameUv.y,1);float4 logoSource=logoTex.Sample(linearSampler,saturate(logoUv));float logoInk=smoothstep(.055,.48,dot(logoSource.rgb,float3(.2126,.7152,.0722)));float logoMask=logoSource.a*logoInk*logoInside;float nameMask=identityTex.Sample(linearSampler,saturate(nameUv)).a*nameInside;return saturate(lerp(logoMask,nameMask,step(.5,identityKind)));}
void compositeLogo(inout float3 color,float4 mark,float opacity,float light){float alpha=saturate(mark.a*opacity);color=lerp(color,mark.rgb,alpha*.72);color+=mark.rgb*alpha*light;}
)"
#include "identity_motion.h"
#include "living_worlds.h"
#include "sculpture_worlds.h"
#include "performance_worlds.h"
R"(
float3 renderWorld(float2 p,float s,float patternSeed,float opening){
 float aspect=resolution.x/resolution.y;float music=tempoPhase*6.283185;float t=time;
 // Four stable stage arrangements per visit. Each keeps its own motif development.
 float arrangement=fmod(floor(patternSeed/16),4);
 if(!isPanelWorld(s)&&abs(s-5)>.1){
  if(arrangement>.5&&arrangement<1.5)p*=float2(.82,1.06);
  else if(arrangement>1.5&&arrangement<2.5)p=rotate2(p,.20)*.90;
  else if(arrangement>2.5)p*=float2(1.16,.80);
 }
 float low=easeBand(bass),mids=easeBand(mid),treble=easeBand(high),hit=easeBand(beat),change=easeBand(flux);float openState=1-saturate(abs(songSection)),buildState=1-saturate(abs(songSection-1)),dropState=1-saturate(abs(songSection-2)),breakState=1-saturate(abs(songSection-3)),driveState=1-saturate(abs(songSection-4));
 float densityMix=saturate((cueDensity-.55)/.80);float densityAccent=lerp(.74,1.08,densityMix);float idle=1-smoothstep(.015,.11,audioEnergy);float endingState=easeBand(endingConfidence);float kickPulse=easeBand(saturate(kick*.94+hit*.18))*(1-endingState*.38)*densityAccent;float bassPump=easeBand(saturate(low*.62+kickPulse*.48+dropPulse*.24))*(1-endingState*.28);float snarePulse=easeBand(saturate(snare*1.04+change*.08))*(1-endingState*.42)*densityAccent;float hatPulse=easeBand(saturate(percussion*1.12+treble*.04))*(1-endingState*.34)*densityAccent;float impact=saturate(low*.22+kickPulse*.38+snarePulse*.16+dropPulse*.16+onset*.08+audioEnergy*.05);float motion=saturate(.15+mids*.28+audioEnergy*.12+buildState*.08-endingState*.035);float detail=saturate((.09+treble*.30+change*.08+hatPulse*.34+snarePulse*.12-endingState*.045)*lerp(.78,1.08,densityMix));float actEnergy=lerp(saturate((.10+openState*.14+buildState*.56+dropState+breakState*.24+driveState*.78)*lerp(.82,1.06,densityMix)),.18,endingState*.72);float movementArc=sin(saturate(movementProgress)*3.141592);float progressionLift=lerp(.82,1.16,saturate(visualMovement*.25));float phase=t*.19+tempoPhase*.075;
 float3 primary,accent,spectral;rarePalette(palette,primary,accent,spectral);
 // Modern worlds own their background. Dispatch them before legacy dust/noise work.
 float3 hitTint=hit*float3(.003,.007,.011)*(1-smoothstep(.18,1.05,length(p)));
 if(isPanelWorld(s))return max(choreographyWall(p,s,primary,accent,spectral,bassPump,snarePulse,hatPulse,actEnergy,patternSeed,opening)+hitTint,0);
 if(s<1.5&&s>=.5)return max(prismAtrium(p,bassPump,snarePulse,hatPulse,actEnergy)+hitTint,0);
 if(s<7.5&&s>=6.5)return max(auroraPerformance(p,bassPump,snarePulse,hatPulse)+hitTint,0);
 if(s<16.5&&s>=15.5)return max(pulseCauseway(p,bassPump,snarePulse,hatPulse,actEnergy)+hitTint,0);
 if(abs(s-4)<.1)return reliefWave(p,bassPump,snarePulse,hatPulse);
 if(abs(s-5)<.1)return pixelPerformance(p,bassPump,snarePulse,hatPulse);
 if(abs(s-6)<.1)return prismPerformance(p,bassPump,snarePulse,hatPulse);
 if(abs(s-8)<.1)return chromaPerformance(p,bassPump,snarePulse,hatPulse);
 if(abs(s-10)<.1)return canopyPerformance(p,bassPump,snarePulse,hatPulse);
 if(abs(s-12)<.1)return vaultPerformance(p,bassPump,snarePulse,hatPulse);
 if(abs(s-14)<.1)return signalPerformance(p,bassPump,snarePulse,hatPulse);
 if(abs(s-17)<.1)return voidPerformance(p,bassPump,snarePulse,hatPulse);
 if(s>17.5&&s<18.5)return ribbonFoundry(p,bassPump,snarePulse,hatPulse);
 if(s>18.5&&s<19.5)return haloEngine(p,bassPump,snarePulse,hatPulse);
 if(s>19.5)return vectorSpeedway(p,bassPump,snarePulse,hatPulse);
 float vignette=1-smoothstep(.28,1.12,length(p));float field=flowField(p*.78,t*.085+tempoPhase*.010);float breathe=.5+.5*sin(t*.24+tempoPhase*.10);float3 c=float3(.0015,.004,.012)+lerp(spectral,primary,field)*(.009+.011*audioEnergy+.004*idle)*vignette;c+=lerp(primary,accent,musicalNovelty)*movementArc*musicalNovelty*.004*vignette;
 float dustNear=livingDust(p+float2(t*.007,-t*.004),t*.52,18,.76-detail*.035);float dustFar=livingDust(rotate2(p,-.08)-float2(t*.003,t*.002),-t*.31,11,.80-detail*.025);float2 frontP=rotate2(p,.16);float lightFront=exp(-abs(frontP.y-sin(frontP.x*1.35+t*.075)*.10-sin(t*.032)*.29)*6.5);c+=(dustNear*primary+dustFar*spectral)*(.010+.012*breathe+.010*detail);c+=lightFront*lerp(primary,spectral,.42)*(.005+.007*actEnergy+.004*audioEnergy);

)"
R"(

)"
R"(
 if(s<2.5){
  float2 gardenCenter=float2(0,-.04+sin(t*.018)*.025);float sky=flowField(p*.36+float2(t*.002,0),t*.018);float nebula=exp(-length((p-gardenCenter)*float2(.78,1.15))*2.1);c=float3(.0008,.0025,.008)+lerp(spectral,primary,sky)*(.006+.010*breakState)+nebula*spectral*(.009+.016*breathe);float stars=starField(p+float2(t*.0012,-t*.0006),34,.975-detail*.005);c+=stars*lerp(primary,accent,.55)*(.018+.040*hatPulse);
  float planetRadius=.17+.018*bassPump;float planet=softDisc(p-gardenCenter,planetRadius,.016);float planetRim=lineGlow(length(p-gardenCenter)-planetRadius,.006);float planetShade=saturate(.45+.55*dot(normalize(p-gardenCenter+float2(.001,0)),normalize(float2(-.7,-.5))));c=lerp(c,lerp(spectral,primary,planetShade)*(.025+.022*actEnergy),planet*.78);c+=planetRim*accent*(.034+.072*bassPump);
  float orbitField=0,bodyField=0,bodyEdge=0;float3 bodyColor=float3(0,0,0);[unroll]for(int orbitIndex=0;orbitIndex<8;orbitIndex++){float radius=.25+orbitIndex*.060;float angle=t*(.030+orbitIndex*.003)+(orbitIndex*6.283185/8)+tempoPhase*.010;float2 orbitScale=float2(1,.52+orbitIndex*.015);float2 orbitPoint=gardenCenter+float2(cos(angle),sin(angle))*radius*orbitScale;float ringDistance=abs(length((p-gardenCenter)/orbitScale)-radius);float visible=.35+.65*step(gardenCenter.y,p.y);orbitField+=lineGlow(ringDistance,.0025)*visible;float body=softDisc(p-orbitPoint,.012+.004*(orbitIndex%3),.010);float rim=lineGlow(length(p-orbitPoint)-(.012+.004*(orbitIndex%3)),.003);float3 orbitColor=lerp(primary,spectral,orbitIndex/7.0);bodyField+=body;bodyEdge+=rim;bodyColor+=body*orbitColor*(.032+.060*actEnergy+.075*bassPump*step(.72,.5+.5*cos(angle*3-tempoPhase*.30)));}
  c+=orbitField*primary*(.012+.026*buildState+.020*driveState)+bodyColor+bodyEdge*accent*(.020+.065*hatPulse);float horizon=.31;float gardenBands=0;[unroll]for(int band=0;band<5;band++){float y=horizon+band*.035+sin(p.x*(1.2+band*.18)+t*(.020+band*.004)+band)*(.016+.007*band);gardenBands+=lineGlow(p.y-y,.003)*(1-band*.12);}c+=gardenBands*lerp(primary,spectral,.42)*(.020+.048*actEnergy+.052*snarePulse);float haze=smoothstep(.18,.52,p.y)*exp(-abs(p.x)*1.8);c+=haze*primary*(.004+.014*driveState);c+=materialIdentity(p,float2(0,.08),.82*logoScale,primary,accent,2);
 }
 c+=hit*float3(.003,.007,.011)*(1-smoothstep(.18,1.05,length(p)));return max(c,0);
}
)"
#include "scene_morph.h"
R"(
float3 brightPass(float3 color){float luminance=dot(color,float3(.2126,.7152,.0722));return color*saturate((luminance-.60)/max(luminance,.001));}
float4 psBloom(O i):SV_TARGET{float2 texel=1/resolution;float3 bloom=brightPass(postScene.Sample(linearSampler,i.uv).rgb)*.22;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+float2(texel.x*2,0)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-float2(texel.x*2,0)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+float2(0,texel.y*2)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-float2(0,texel.y*2)).rgb)*.14;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(4,4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(-4,4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv+texel*float2(4,-4)).rgb)*.055;bloom+=brightPass(postScene.Sample(linearSampler,i.uv-texel*4).rgb)*.055;return float4(bloom,1);}
float3 filmic(float3 x){float a=2.51,b=.03,c=2.43,d=.59,e=.14;return saturate((x*(a*x+b))/(x*(c*x+d)+e));}
float4 psComposite(O i):SV_TARGET{float2 texel=1/resolution;float3 center=postScene.Sample(linearSampler,i.uv).rgb;float3 neighbors=postScene.Sample(linearSampler,i.uv+float2(texel.x,0)).rgb+postScene.Sample(linearSampler,i.uv-float2(texel.x,0)).rgb+postScene.Sample(linearSampler,i.uv+float2(0,texel.y)).rgb+postScene.Sample(linearSampler,i.uv-float2(0,texel.y)).rgb;float3 sharp=max(center*1.12-neighbors*.030,0);float3 bloom=postBloom.Sample(linearSampler,i.uv).rgb;float2 q=i.uv-.5;float vignette=1-smoothstep(.34,.84,length(q));float3 color=(sharp+bloom*.74)*(.86+.14*vignette);color=filmic(color);float luminance=dot(color,float3(.2126,.7152,.0722));if(masteringProfile>.5&&masteringProfile<1.5){float compression=1/(1+max(0,luminance-.48)*1.7);color*=compression;color=lerp(luminance.xxx,color,.92);}else if(masteringProfile>1.5&&masteringProfile<2.5){color=pow(saturate(color),.92);color=lerp(luminance.xxx,color,1.08);}else if(masteringProfile>2.5)color=lerp(luminance.xxx,color,.92);color=pow(max(color,0),1/2.2)*masterBrightness;color+=(hash(i.uv*resolution+floor(time*20))-.5)*.0020;if(masteringProfile>.5&&masteringProfile<1.5)color=min(color,.86);else if(masteringProfile>2.5)color=saturate(color*.86+.0627);float layoutMask=1;if(stageLayout>.5&&stageLayout<1.5)layoutMask=step(.128,i.uv.y)*step(i.uv.y,.872);else if(stageLayout>1.5&&stageLayout<2.5){float lane=frac(i.uv.x*3);layoutMask=step(.022,lane)*step(lane,.978);}else if(stageLayout>2.5){float centerPanel=step(.225,i.uv.x)*step(i.uv.x,.775);float leftWing=step(.035,i.uv.x)*step(i.uv.x,.185)*step(.12,i.uv.y)*step(i.uv.y,.88);float rightWing=step(.815,i.uv.x)*step(i.uv.x,.965)*step(.12,i.uv.y)*step(i.uv.y,.88);layoutMask=saturate(centerPanel+leftWing+rightWing);}return float4(saturate(color)*layoutMask,1);}
)";

#include "shader_cache.h"
struct Constants { float resolution[2],time,scene,bass,mid,high,level,percussion,snare,beat,flux,previousScene,transition,logoScale,palette,transitionStyle,tempoPhase,tempoConfidence,audioEnergy,songSection,sectionProgress,dropPulse,onset,kick,beatIndex,barPhase,cuePhase,endingConfidence,directorConfidence,idleLife,cueDensity,masteringProfile,stageLayout,masterBrightness,productionDuty,visualMovement,movementProgress,patternSeed,musicalNovelty,choreographyPhase,identityWeight,identityKind,stanzaProgress,previousPatternSeed,morphPad0,morphPad1,morphPad2,dvdX,dvdY,dvdCorner,dvdControlled; };
static_assert(sizeof(Constants)%16==0,"GPU frame constants must remain 16-byte aligned");

#include "display_outputs.h"
#include "show_motion.h"
#include "performance_contracts.h"

class App {
public:
    ~App(){outputs_.clear();shutdownControl();}
    bool init(HINSTANCE instance) {
        validationMode_=std::wcsstr(GetCommandLineW(),L"--performance-test")||std::wcsstr(GetCommandLineW(),L"--professional-test")||std::wcsstr(GetCommandLineW(),L"--release-validation")||std::wcsstr(GetCommandLineW(),L"--stage-lighting-test")||std::wcsstr(GetCommandLineW(),L"--choreography-test")||std::wcsstr(GetCommandLineW(),L"--choreography-benchmark")||std::wcsstr(GetCommandLineW(),L"--visual-regression-test")||std::wcsstr(GetCommandLineW(),L"--benchmark-4k");if(!validationMode_)settings_.load();else{settings_.spout=0;settings_.syncMode=0;}settings_.djName=SanitizeDjName(settings_.djName);audio_.setProfile(settings_.audioProfile);sync_.setMode(settings_.syncMode);automatic_=settings_.automatic!=0;visualPalette_=float(settings_.palette);directorPalette_=settings_.palette;activeTransitionSeconds_=settings_.transitionSeconds;activeTransitionStyle_=settings_.transitionStyle;effectiveTempoAnchor_=settings_.tempoAnchor==2?130:128;visualBpm_=stableBpm_=float(effectiveTempoAnchor_);if(!sync_.setMode(settings_.syncMode)){settings_.syncMode=0;syncStatus_=L"SYNC SOCKET UNAVAILABLE";}else syncStatus_=settings_.syncMode==1?L"LEADER ARMED / GROUP "+std::to_wstring(settings_.syncGroup):settings_.syncMode==2?L"FOLLOWER WAITING / GROUP "+std::to_wstring(settings_.syncGroup):L"LOCAL / STANDALONE";
        StartupLog("Creating RareStage window class");
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=WndProc;wc.lpszClassName=L"RareStageWindow";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
        hwnd_=CreateWindowExW(0,wc.lpszClassName,L"RareStage",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,720,nullptr,nullptr,instance,this);
        if(!hwnd_){StartupLog("CreateWindowExW failed");return false;}
        if(!validationMode_&&!initControl()){MessageBoxW(hwnd_,L"RareStage could not create its local control endpoint. Make sure another RareStage instance is not already running.",L"RareStage control error",MB_OK|MB_ICONERROR);return false;}
        StartupLog("Initializing DirectX renderer");
        if(!createGpu()){MessageBoxW(hwnd_,L"RareStage could not initialize DirectX. Update your graphics driver and try again.",L"RareStage startup error",MB_OK|MB_ICONERROR);return false;}
        refreshDisplays(false);selectMonitor(settings_.monitor);
        StartupLog("DirectX renderer ready");
        if(!validationMode_){ShowWindow(hwnd_,SW_SHOW);UpdateWindow(hwnd_);StartupLog("Window shown");}
        if(!validationMode_)audio_.start();StartupLog("Audio capture thread configured");started_=Clock::now();transitionStarted_=-100;scheduleNextWorld(audio_.frame(),0);updateHud(audio_.frame());return true;
    }
    void requestClose(){PostMessageW(hwnd_,WM_CLOSE,0,0);}
#include "visual_validation.h"
#include "choreography_validation.h"
#include "stage_lighting_validation.h"
#include "professional_validation.h"
#include "performance_validation.h"
    void enableReferenceCaptureTest(){referenceCaptureMode_=true;automatic_=false;frozen_=false;hudVisible_=false;settings_.vsync=0;settings_.quality=0;world_=0;previousWorld_=0;songSection_=DropSection;manualSectionOverrideUntil_=1.0e9f;referenceQuality_=0;referenceWorld_=0;referenceWarmup_=0;std::error_code error;std::filesystem::create_directories("RareStage-reference-captures",error);SetWindowPos(hwnd_,nullptr,0,0,640,360,SWP_NOMOVE|SWP_NOZORDER);}
    int run(){MSG msg{};while(msg.message!=WM_QUIT){if(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}else render();}audio_.stop();return(int)msg.wParam;}
private:
    struct GpuTimerSlot{ComPtr<ID3D11Query>disjoint,start,end;bool pending{};};
    static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){App*a=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){a=reinterpret_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)a);}if(a)return a->message(h,m,w,l);return DefWindowProcW(h,m,w,l);}
    LRESULT message(HWND h,UINT m,WPARAM w,LPARAM l){
        if(m==WM_DESTROY){outputs_.clear();settings_.automatic=automatic_?1:0;if(!validationMode_)settings_.save();sync_.stop();shutdownSpout();shutdownControl();PostQuitMessage(0);return 0;}
        if(m==rarestage::control::CommandMessage)return handleControlCommand(static_cast<rarestage::control::Command>(w),l);
        if(m==WM_COPYDATA){const auto*copy=reinterpret_cast<const COPYDATASTRUCT*>(l);if(!copy||copy->dwData!=rarestage::control::DjNameCopyDataId||!copy->lpData||copy->cbData<sizeof(wchar_t)||copy->cbData>sizeof(wchar_t)*32||copy->cbData%sizeof(wchar_t))return FALSE;const auto*characters=static_cast<const wchar_t*>(copy->lpData);const size_t count=copy->cbData/sizeof(wchar_t);std::wstring value(characters,characters+std::min(count,size_t(31)));const auto zero=value.find(L'\0');if(zero!=std::wstring::npos)value.resize(zero);settings_.djName=SanitizeDjName(value);settings_.save();return TRUE;}
        if(m==WM_SIZE&&w!=SIZE_MINIMIZED){if(swap_)resize(LOWORD(l),HIWORD(l));return 0;}
        if(FullscreenKey(m,w,l)){fullscreen();return 0;}
        if(m==WM_SYSCHAR&&w==VK_RETURN)return 0;
        if(m==WM_EXITSIZEMOVE&&!fullscreen_){MONITORINFO info{};info.cbSize=sizeof(info);if(GetMonitorInfoW(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&info))for(size_t n=0;n<displays_.size();++n)if(EqualRect(&info.rcMonitor,&displays_[n].bounds)){settings_.monitor=int(n);outputsDirty_=true;if(!validationMode_)settings_.save();}return 0;}
        if(m==WM_DISPLAYCHANGE){displaysDirty_=true;return 0;}
        if(m==CloseMirrorMessage){settings_.mirrorMask=0;outputsDirty_=true;if(!validationMode_)settings_.save();return 0;}
        if(m==WM_DPICHANGED){if(fullscreen_)placeFullscreen();else{const RECT*target=reinterpret_cast<const RECT*>(l);SetWindowPos(hwnd_,nullptr,target->left,target->top,target->right-target->left,target->bottom-target->top,SWP_NOZORDER|SWP_NOACTIVATE);}return 0;}
        if(m==WM_LBUTTONDOWN||m==WM_RBUTTONDOWN){POINT point{static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};handleHudPointer(point,m==WM_RBUTTONDOWN?-1:0);return 0;}
        if(m==WM_MOUSEWHEEL){POINT point{static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};ScreenToClient(h,&point);handleHudPointer(point,GET_WHEEL_DELTA_WPARAM(w)>0?1:-1);return 0;}
        if(m==WM_KEYDOWN){
            if(w>=L'1'&&w<=L'9'){selectWorld(int(w-L'1'));settings_.save();}
            else if(w==L'Q'||w==L'W'||w==L'E'){selectWorld(w==L'Q'?9:w==L'W'?10:11);settings_.save();}
            else if(w==L'R'||w==L'Y'||w==L'U'){selectWorld(w==L'R'?12:w==L'Y'?13:14);settings_.save();}
            else if(w==L'I'||w==L'O'||w==L'K'){selectWorld(w==L'I'?15:w==L'O'?16:17);settings_.save();}
            else if(w==L'Z'||w==L'X'||w==L'J'){selectWorld(w==L'Z'?18:w==L'X'?19:20);settings_.save();}
            else if(w==VK_OEM_4||w==VK_OEM_6){selectWorld((world_+(w==VK_OEM_6?1:WorldCount-1))%WorldCount);settings_.save();}
            else if(w==L'0'||w==L'A'){automatic_=!automatic_;settings_.automatic=automatic_?1:0;if(automatic_){chooseDirectorPalette(true);scheduleNextWorld(tuned(audio_.frame()),elapsed());}settings_.save();}
            else if(w==L'D')audio_.cycle();else if(w==L'M')cycleMonitor();else if(w==L'P'){settings_.palette=(settings_.palette+1)%8;directorPalette_=settings_.palette;settings_.save();}
            else if(w==L'T'){settings_.timeline=!settings_.timeline;settings_.save();timelineStep_=0;}
            else if(w==L'V'){settings_.vsync=!settings_.vsync;settings_.save();}
            else if(w==L'S'){settings_.spout=!settings_.spout;settings_.save();}
            else if(w>=VK_F1&&w<=VK_F4){int slot=int(w-VK_F1);if(GetKeyState(VK_SHIFT)&0x8000)savePreset(slot);else loadPreset(slot);}
            else if(w==VK_TAB){hudVisible_=!hudVisible_;}
            else if(w==VK_UP)hudSelection_=(hudSelection_+SettingCount-1)%SettingCount;else if(w==VK_DOWN)hudSelection_=(hudSelection_+1)%SettingCount;
            else if(w==VK_LEFT)adjustSetting(-1);else if(w==VK_RIGHT||w==VK_RETURN)adjustSetting(1);
            else if(w==VK_SPACE)frozen_=!frozen_;else if(w==VK_ESCAPE){if(fullscreen_)fullscreen();else DestroyWindow(h);}
            updateHud(audio_.frame());return 0;
        }
        return DefWindowProcW(h,m,w,l);
    }
    LRESULT handleControlCommand(rarestage::control::Command command,LPARAM packed){
        using namespace rarestage::control;const auto serial=CommandSerial(packed);const int value=CommandValue(packed);bool accepted=true;bool rebuild=false;
        switch(command){
            case Command::Ping:break;
            case Command::SelectWorld:if(value<0||value>=WorldCount){accepted=false;break;}selectWorld(value);break;
            case Command::PreviousWorld:selectWorld((world_+WorldCount-1)%WorldCount);break;
            case Command::NextWorld:selectWorld((world_+1)%WorldCount);break;
            case Command::SetAutomatic:automatic_=value!=0;settings_.automatic=automatic_?1:0;if(automatic_){chooseDirectorPalette(true);scheduleNextWorld(tuned(audio_.frame()),elapsed());}break;
            case Command::ForceSection:if(value<0||value>=SongSectionCount){accepted=false;break;}enterSongSection(value,elapsed());manualSectionOverrideUntil_=elapsed()+(value==DropSection?4.5f:8.f);worldChangeArmed_=false;sectionChangePending_=false;break;
            case Command::ClearSectionOverride:manualSectionOverrideUntil_=0;sectionCandidate_=songSection_;sectionCandidateSeconds_=0;break;
            case Command::SetFrozen:frozen_=value!=0;break;
            case Command::SetBlackout:blackout_=value!=0;break;
            case Command::CleanOpen:blackout_=false;enterSongSection(OpenSection,elapsed());manualSectionOverrideUntil_=elapsed()+8.f;worldChangeArmed_=false;sectionChangePending_=false;break;
            case Command::Fallback:blackout_=false;frozen_=false;automatic_=false;settings_.automatic=0;selectWorld(2);enterSongSection(OpenSection,elapsed());manualSectionOverrideUntil_=elapsed()+10.f;break;
            case Command::LoadPreset:if(value<0||value>=4){accepted=false;break;}loadPreset(value);break;
            case Command::SavePreset:if(value<0||value>=4){accepted=false;break;}savePreset(value);break;
            case Command::SetStageOverlay:hudVisible_=value!=0;break;
            case Command::SetProfile:if(value<0||value>=4){accepted=false;break;}settings_.profile=value;break;
            case Command::SetPalette:if(value<0||value>=8){accepted=false;break;}settings_.palette=value;directorPalette_=value;break;
            case Command::SetSensitivityPercent:if(value<50||value>150){accepted=false;break;}settings_.sensitivity=value/100.f;break;
            case Command::SetBassGainPercent:if(value<50||value>150){accepted=false;break;}settings_.bassGain=value/100.f;break;
            case Command::SetMidGainPercent:if(value<50||value>150){accepted=false;break;}settings_.midGain=value/100.f;break;
            case Command::SetHighGainPercent:if(value<50||value>150){accepted=false;break;}settings_.highGain=value/100.f;break;
            case Command::SetBeatGainPercent:if(value<50||value>150){accepted=false;break;}settings_.beatGain=value/100.f;break;
            case Command::SetLogoScalePercent:if(value<75||value>125){accepted=false;break;}settings_.logoScale=value/100.f;break;
            case Command::SetWorldDwellSeconds:if(value<40||value>120){accepted=false;break;}settings_.sceneSeconds=float(value);scheduleNextWorld(tuned(audio_.frame()),elapsed());break;
            case Command::SetTransitionTenths:if(value<2||value>30){accepted=false;break;}settings_.transitionSeconds=value/10.f;break;
            case Command::SetTransitionStyle:if(value<0||value>=4){accepted=false;break;}settings_.transitionStyle=value;break;
            case Command::SetQuality:if(value<0||value>=4){accepted=false;break;}settings_.quality=value;rebuild=true;break;
            case Command::SetVsync:settings_.vsync=value!=0;break;
            case Command::SetTimeline:settings_.timeline=value!=0;timelineStep_=0;break;
            case Command::SetSpout:settings_.spout=value!=0;if(settings_.spout)initSpout();break;
            case Command::SetAudioProfile:if(value<0||value>=4){accepted=false;break;}settings_.audioProfile=value;audio_.setProfile(value);break;
            case Command::CycleAudioDevice:audio_.cycle();break;
            case Command::CycleMonitor:cycleMonitor();break;
            case Command::SelectMonitor:if(value<0||value>=int(displays_.size())){accepted=false;break;}selectMonitor(value);break;
            case Command::SetMirrorMask:if(value<0||value>255){accepted=false;break;}settings_.mirrorMask=unsigned(value)&((1u<<displays_.size())-1u);outputsDirty_=true;break;
            case Command::SetFullscreen:if((value!=0)!=fullscreen_)fullscreen();break;
            case Command::QuitRenderer:PostMessageW(hwnd_,WM_CLOSE,0,0);break;
            case Command::CaptureFrame:captureRequested_=true;captureStatus_=0;break;
            case Command::SetCueDensityPercent:if(value<65||value>125){accepted=false;break;}settings_.cueDensity=value/100.f;break;
            case Command::SetRehearsalActive:if((value!=0)!=rehearsalActive_){rehearsalActive_=value!=0;rehearsalStartedAt_=rehearsalActive_?elapsed():0.f;++rehearsalRevision_;}break;
            case Command::SetMasteringProfile:if(value<0||value>=4){accepted=false;break;}settings_.masteringProfile=value;break;
            case Command::SetMasterBrightnessPercent:if(value<65||value>100){accepted=false;break;}settings_.masterBrightness=value/100.f;break;
            case Command::SetStageLayout:if(value<0||value>=4){accepted=false;break;}settings_.stageLayout=value;break;
            case Command::ApplyProductionDuty:if(value<0||value>=5){accepted=false;break;}applyProductionDuty(value);break;
            case Command::SetSyncMode:if(value<0||value>=3||!sync_.setMode(value)){accepted=false;settings_.syncMode=0;syncStatus_=L"SYNC SOCKET UNAVAILABLE";break;}settings_.syncMode=value;syncConnected_=false;syncStatus_=value==1?L"LEADER ARMED / GROUP "+std::to_wstring(settings_.syncGroup):value==2?L"FOLLOWER WAITING / GROUP "+std::to_wstring(settings_.syncGroup):L"LOCAL / STANDALONE";break;
            case Command::SetSyncGroup:if(value<1||value>9999){accepted=false;break;}settings_.syncGroup=value;syncConnected_=false;syncStatus_=settings_.syncMode==2?L"FOLLOWER WAITING / GROUP "+std::to_wstring(value):settings_.syncMode==1?L"LEADER ARMED / GROUP "+std::to_wstring(value):L"LOCAL / GROUP "+std::to_wstring(value);break;
            case Command::SetSceneCategory:if(value<0||value>=int(SceneCategoryCount)){accepted=false;break;}settings_.sceneCategory=value;preparedWorld_=-1;updateDirectorDiagnostics(tuned(audio_.frame()),elapsed(),true);break;
            case Command::SetTempoAnchor:if(value<0||value>=int(TempoAnchorCount)){accepted=false;break;}settings_.tempoAnchor=value;effectiveTempoAnchor_=value==2?130:128;break;
            case Command::SetDjIdentityVisible:settings_.djIdentity=value!=0;break;
            default:accepted=false;break;
        }
        lastCommandSerial_=serial;lastCommandStatus_=accepted?CommandStatus::Accepted:CommandStatus::Rejected;
        if(accepted){settings_.save();updateHud(audio_.frame());if(rebuild&&swap_)resize(width_,height_);}
        return 0;
    }
    static constexpr int WorldCount=21;
    static_assert(WorldCount==rarestage::control::WorldCount);
    enum SongSection { OpenSection, BuildSection, DropSection, BreakdownSection, DriveSection, SongSectionCount };
    static const wchar_t* worldName(int index){static const wchar_t*names[]={L"Morph Wall",L"Prism Atrium",L"Orbital Garden",L"Stagewall",L"Resonance Range",L"Pixel Bank",L"Prism Cascade",L"Aurora Range",L"Chroma Blades",L"Kinetic Array",L"Kinetic Canopy",L"Pixel Loom",L"Matrix Vault",L"Panel Cascade",L"Signal Rail",L"Kinetic Facade",L"Pulse Causeway",L"Void Gates",L"Ribbon Foundry",L"Halo Engine",L"Vector Speedway"};return names[std::clamp(index,0,WorldCount-1)];}
    static const wchar_t* sectionName(int section){static const wchar_t*names[]={L"OPEN",L"BUILD",L"DROP",L"BREAKDOWN",L"DRIVE"};return names[std::clamp(section,0,SongSectionCount-1)];}
    static const wchar_t* movementName(int movement){static const wchar_t*names[]={L"ESTABLISH",L"ASSEMBLE",L"PAYOFF",L"DEVELOP",L"DISSOLVE"};return names[std::clamp(movement,0,4)];}
    static const wchar_t* categoryName(int category){static const wchar_t*names[]={L"AUTO",L"PANEL / PIXEL",L"RIBBON / GLASS",L"DEPTH / TRANSPORT",L"ATMOSPHERE"};return names[std::clamp(category,0,4)];}
    static int worldFamily(int world){return rarestage::show::Scenes[std::clamp(world,0,WorldCount-1)].category;}
    static float familyCompatibility(int from,int to,int){return rarestage::show::Compatibility(from,to);}
    static const wchar_t* paletteName(int palette){static const wchar_t*names[]={L"RARE CYAN",L"NEBULA",L"SAKURA",L"SOLAR",L"LASER LIME",L"CORAL ICE",L"ULTRAVIOLET",L"ELECTRIC GOLD"};return names[std::clamp(palette,0,7)];}
    float elapsed()const{return started_.time_since_epoch().count()?std::chrono::duration<float>(Clock::now()-started_).count():0;}
    static int paletteForSection(int section,int phase){static constexpr int choices[SongSectionCount][5]={{0,1,5,4,6},{2,4,6,0,7},{3,7,4,2,5},{1,5,6,0,2},{0,3,4,7,6}};return choices[std::clamp(section,0,SongSectionCount-1)][std::abs(phase)%5];}
    void chooseDirectorPalette(bool sectionChange){int phase=sectionRevision_+(sectionChange?0:autoSequence_/2);directorPalette_=setMemory_.palette(world_,paletteForSection(songSection_,phase),directorPalette_);}
    void serviceQueuedWorld(){
        if((queuedDirected_&&frozen_)||queuedWorld_<0||elapsed()-transitionStarted_<activeTransitionSeconds_)return;
        const int next=queuedWorld_,style=queuedMotionStyle_;const bool directed=queuedDirected_;const float duration=queuedMotionSeconds_;
        queuedWorld_=-1;selectWorld(next,directed);
        if(duration>0){activeTransitionSeconds_=duration;activeTransitionStyle_=style;worldBeatOrigin_=tempoPhase_+duration*visualBpm_/60.f;}
    }
    void selectWorld(int next,bool directed=false){next=std::clamp(next,0,WorldCount-1);if(next==world_){if(pixelExitWorld_>=0){pixelExitWorld_=-1;cornerMotion_.release(elapsed());}queuedWorld_=-1;worldChangeArmed_=false;sectionChangePending_=false;return;}if(elapsed()-transitionStarted_<activeTransitionSeconds_){queuedWorld_=next;queuedDirected_=directed;queuedMotionSeconds_=-1;return;}if(world_==5&&next!=5&&settings_.syncMode!=2&&!pixelExitCommit_){pixelExitWorld_=next;pixelExitDirected_=directed;if(!cornerMotion_.holdCorner){const float beats=8.f-std::fmod(tempoPhase_,4.f);cornerMotion_.aim(elapsed(),elapsed()+beats*60.f/std::clamp(visualBpm_,65.f,190.f),true);}return;}if(next==5){cornerMotion_.release(elapsed());}if(directed){activeTransitionStyle_=rarestage::show::HandoffStyle(world_,next);const float motionBeats=(songSection_==0||songSection_==3)?8.f:4.f;activeTransitionSeconds_=motionBeats*60.f/std::clamp(visualBpm_,65.f,190.f);}else{activeTransitionStyle_=settings_.transitionStyle;activeTransitionSeconds_=settings_.transitionSeconds;}transitionCompatibility_=familyCompatibility(world_,next,songSection_);previousPatternSeed_=patternSeed_;handoffIdentityWeight_=identityWeight_;handoffIdentityKind_=identityKind_;handoffIdentityStyle_=stanzaProgress_>.72f?identityExit_:identityEntrance_;previousWorld_=world_;world_=next;if(directed&&autoSequence_%2==0)chooseDirectorPalette(false);showDirector_.visit(next);route_={};routeSection_=-1;transitionStarted_=elapsed();worldStartedAt_=transitionStarted_;worldBeatOrigin_=tempoPhase_+activeTransitionSeconds_*visualBpm_/60.f;identityCycle_=-1;worldChangeArmed_=false;sectionChangePending_=false;for(size_t i=recentWorlds_.size()-1;i>0;--i)recentWorlds_[i]=recentWorlds_[i-1];recentWorlds_[0]=next;if(worldActUse_[next][songSection_]<65535)++worldActUse_[next][songSection_];++visitSeed_;selectPatternProgram();movementRevision_=-1;++setMemoryRevision_;if(directed){++acceptedDirectorCueCount_;lastDirectorDecision_=L"WORLD ACCEPTED / "+std::wstring(worldName(next))+L" / "+sectionName(songSection_);++directorDecisionRevision_;}preparedWorld_=-1;preparedTargetSection_=-1;predictionArmed_=false;if(!directed&&automatic_)scheduleNextWorld(audio_.frame(),transitionStarted_);}
    static float directorEnergy(const AudioFrame&af){return std::clamp(af.level*.22f+af.bass*.20f+af.mid*.08f+af.beat*.08f+af.kick*.16f+af.snare*.08f+af.percussion*.04f+af.flux*.06f+af.onset*.08f,0.f,1.f);}
    float sectionEvidence(const AudioFrame&af,float energy)const{float rise=sectionShortEnergy_-sectionLongEnergy_;if(songSection_==DropSection)return std::clamp(af.kick*.34f+af.onset*.24f+af.bass*.20f+dropPulse_*.22f,0.f,1.f);if(songSection_==BreakdownSection)return std::clamp((.24f-energy)*2.6f+std::max(0.f,-rise)*3.8f+endingConfidence_*.28f,0.f,1.f);if(songSection_==BuildSection)return std::clamp(std::max(0.f,rise)*5.2f+af.flux*.72f+af.mid*.24f,0.f,1.f);if(songSection_==DriveSection)return std::clamp(energy*.92f+af.bass*.18f+af.mid*.15f,0.f,1.f);return std::clamp((.24f-energy)*2.2f+(heardActiveMusic_?0.f:.35f),0.f,1.f);}
    void enterSongSection(int next,float seconds){songSection_=std::clamp(next,0,SongSectionCount-1);static constexpr int movementForSection[SongSectionCount]={0,1,2,4,3};worldMovement_=movementForSection[songSection_];sectionCandidate_=songSection_;sectionCandidateSeconds_=0;sectionStartedAt_=seconds;sectionBeatOrigin_=tempoPhase_;++sectionRevision_;if(worldActUse_[world_][songSection_]<65535)++worldActUse_[world_][songSection_];++setMemoryRevision_;chooseDirectorPalette(true);if(songSection_==DropSection){dropPulse_=1;lastDropAt_=seconds;++dropCount_;}sectionLockUntil_=seconds+(songSection_==DropSection?3.5f:5.5f);float dwell=seconds-worldStartedAt_;bool chapterBoundary=songSection_==DropSection||songSection_==BreakdownSection||songSection_==OpenSection;if(automatic_&&chapterBoundary&&dwell>=40.f&&seconds>=nextWorldAt_-12.f){sectionChangePending_=true;worldChangeArmed_=true;worldChangeReadyAt_=seconds;worldCueBeat_=-1;}}
    void updateSongSection(const AudioFrame&af,float dt,float seconds){const float energy=directorEnergy(af);const float previousFast=sectionShortEnergy_;sectionShortEnergy_+=(energy-sectionShortEnergy_)*(1-std::exp(-dt/0.34f));sectionPhraseEnergy_+=(energy-sectionPhraseEnergy_)*(1-std::exp(-dt/2.8f));sectionLongEnergy_+=(energy-sectionLongEnergy_)*(1-std::exp(-dt/11.f));const float rise=sectionPhraseEnergy_-sectionLongEnergy_;musicalNovelty_+=(std::clamp(std::abs(sectionShortEnergy_-previousFast)*4.f+af.flux*.45f+af.onset*.20f,0.f,1.f)-musicalNovelty_)*std::clamp(dt*3.2f,0.f,1.f);const float tensionTarget=std::clamp(std::max(0.f,rise)*4.4f+af.high*.18f+af.snare*.18f+af.onset*.14f+std::max(0.f,sectionPhraseEnergy_-af.bass)*.20f,0.f,1.f);buildTension_+=(tensionTarget-buildTension_)*std::clamp(dt*(tensionTarget>buildTension_?1.8f:.45f),0.f,1.f);const float impactContrast=std::max(0.f,energy-sectionPhraseEnergy_);dropReadiness_=std::clamp(buildTension_*.58f+impactContrast*2.4f+af.kick*.20f+af.onset*.12f,0.f,1.f);if(sectionLongEnergy_>.24f){heardActiveMusic_=true;if(musicStartedAt_<0)musicStartedAt_=seconds;}if(heardActiveMusic_&&energy<.18f&&af.onset<.12f)lowEnergySeconds_+=dt;else lowEnergySeconds_=std::max(0.f,lowEnergySeconds_-dt*2.4f);float fadeEvidence=std::clamp((sectionLongEnergy_-sectionShortEnergy_-.020f)*4.8f+(.18f-energy)*2.2f,0.f,1.f);float endingTarget=heardActiveMusic_&&lowEnergySeconds_>1.5f?std::clamp(fadeEvidence*.70f+(lowEnergySeconds_-1.5f)*.035f,0.f,1.f):0.f;if(energy>.28f||af.onset>.24f)endingTarget=0;endingConfidence_+=(endingTarget-endingConfidence_)*std::clamp(dt*(endingTarget>endingConfidence_?.52f:1.45f),0.f,1.f);dropPulse_*=std::exp(-dt*3.5f);songArc_=musicStartedAt_>=0?std::clamp((seconds-musicStartedAt_)/420.f,0.f,1.f):0.f;int phrase=int(std::floor(tempoPhase_/8.f));bool phraseBoundary=af.tempoConfidence>.14f&&phrase!=lastDirectorPhrase_;if(phraseBoundary)lastDirectorPhrase_=phrase;sectionEvidence_=sectionEvidence(af,energy);if(seconds<manualSectionOverrideUntil_)return;const bool contextArmed=(songSection_==BuildSection&&seconds-sectionStartedAt_>.9f)||buildTension_>.48f;const bool qualifiedImpact=contextArmed&&af.kick>.34f&&af.onset>.30f&&af.bass>.18f&&impactContrast>.035f&&dropReadiness_>.56f&&seconds-lastDropAt_>7.f;if(qualifiedImpact&&songSection_!=DropSection){endingConfidence_=0;lowEnergySeconds_=0;buildTension_*=.30f;enterSongSection(DropSection,seconds);return;}int proposed=songSection_;if(songSection_==DropSection&&seconds-sectionStartedAt_>8.f&&energy>.32f)proposed=DriveSection;else if(songSection_==DropSection&&seconds-sectionStartedAt_<6.f)proposed=DropSection;else if(buildTension_>.46f&&energy>.23f)proposed=BuildSection;else if(energy>.46f&&af.bass>.20f)proposed=DriveSection;else if(energy<.17f||rise<-.075f)proposed=heardActiveMusic_?BreakdownSection:OpenSection;if(endingConfidence_>.30f)proposed=BreakdownSection;if(proposed!=sectionCandidate_){sectionCandidate_=proposed;sectionCandidateSeconds_=0;}else sectionCandidateSeconds_+=dt;float required=proposed==DropSection?.025f:(phraseBoundary?.24f:.70f);bool unlocked=seconds>=sectionLockUntil_||proposed==DropSection||songSection_==OpenSection;if(proposed!=songSection_&&unlocked&&sectionCandidateSeconds_>=required)enterSongSection(proposed,seconds);}
    float baseWorldFitScore(int candidate,const AudioFrame&af,float energy,bool tieBreak,int targetSection=-1)const{const int scoringSection=targetSection<0?songSection_:std::clamp(targetSection,0,SongSectionCount-1);static constexpr float targetEnergy[WorldCount]={.42f,.54f,.25f,.68f,.36f,.58f,.72f,.55f,.48f,.64f,.50f,.60f,.70f,.66f,.57f,.66f,.46f,.40f,.48f,.56f,.65f};static constexpr float bassAffinity[WorldCount]={.35f,.42f,.18f,.82f,.30f,.52f,.68f,.38f,.55f,.76f,.52f,.60f,.74f,.80f,.66f,.78f,.48f,.54f,.48f,.72f,.80f};static constexpr float midAffinity[WorldCount]={.80f,.82f,.55f,.50f,.78f,.78f,.70f,.95f,.45f,.62f,.70f,.84f,.64f,.70f,.76f,.62f,.72f,.46f,.92f,.70f,.65f};static constexpr float highAffinity[WorldCount]={.70f,.68f,.68f,.72f,.52f,.92f,.88f,.75f,.66f,.82f,.70f,.92f,.96f,.84f,.88f,.80f,.64f,.58f,.72f,.66f,.82f};static constexpr float sectionAffinity[SongSectionCount][WorldCount]={{.72f,.58f,.96f,.35f,.88f,.70f,.44f,.76f,.94f,.42f,.82f,.66f,.48f,.40f,.78f,.42f,.86f,.94f,.90f,.72f,.52f},{.94f,.92f,.58f,.92f,.76f,.98f,.96f,1.f,.82f,.94f,.86f,1.f,.94f,.98f,.92f,.96f,.90f,.72f,.88f,.96f,.94f},{.74f,.84f,.28f,1.f,.68f,.92f,1.f,.86f,.90f,1.f,.90f,.96f,1.f,1.f,.98f,1.f,.92f,.88f,.72f,.98f,1.f},{.62f,.88f,1.f,.42f,.94f,.78f,.36f,.72f,1.f,.46f,.94f,.72f,.55f,.48f,.82f,.48f,.96f,1.f,.98f,.84f,.46f},{.88f,.96f,.48f,.98f,.82f,1.f,.96f,1.f,.72f,1.f,.92f,.98f,.98f,1.f,1.f,1.f,.94f,.82f,.94f,.96f,1.f}};float score=1.20f-std::abs(energy-targetEnergy[candidate])*2.15f;score+=bassAffinity[candidate]*af.bass*.30f+midAffinity[candidate]*af.mid*.28f+highAffinity[candidate]*af.high*.22f+sectionAffinity[scoringSection][candidate]*.76f;if(candidate==3||candidate==6||candidate>=9)score+=af.flux*.12f+af.kick*.16f+af.snare*.12f+af.percussion*.10f;if(candidate==2||candidate==4||candidate==8||candidate==10||candidate==16||candidate==17)score+=(1-energy)*.16f;if(candidate==world_)score-=4.f;for(size_t h=0;h<recentWorlds_.size();++h)if(candidate==recentWorlds_[h])score-=1.20f/float(h+1);std::uint16_t minimumUse=worldActUse_[0][scoringSection];for(int w=1;w<WorldCount;++w)minimumUse=std::min(minimumUse,worldActUse_[w][scoringSection]);score-=float(worldActUse_[candidate][scoringSection]-minimumUse)*.28f;if(tieBreak){uint32_t mix=uint32_t((autoSequence_+1)*2654435761u)^uint32_t((candidate+3)*2246822519u);score+=float(mix%1000u)/1000.f*.12f;}return score;}
    float worldFitScore(int candidate,const AudioFrame&af,float energy,bool tieBreak,int targetSection=-1)const{if(settings_.sceneCategory>0&&worldFamily(candidate)!=settings_.sceneCategory-1)return -1000.f;const int section=targetSection<0?songSection_:std::clamp(targetSection,0,SongSectionCount-1);float score=baseWorldFitScore(candidate,af,energy,tieBreak,targetSection);if(surfaceWall(candidate)||candidate==16)score+=.20f;score+=familyCompatibility(world_,candidate,section)*.62f;if(section==DropSection&&(candidate==3||candidate==9||candidate==11||candidate==12||candidate==13||candidate==15))score+=.24f;if(section==BreakdownSection&&worldFamily(candidate)==3)score+=.18f;if(candidate==1&&(section==DropSection||section==BuildSection))score+=.28f;if((candidate==7||candidate==16)&&(section==OpenSection||section==BreakdownSection))score+=.26f;return score;}
    void noteRejectedCue(const std::wstring&reason){if(reason==lastRejectedCue_)return;lastRejectedCue_=reason;++rejectedDirectorCueCount_;++directorDecisionRevision_;}
    void preparePredictedWorld(const AudioFrame&af,float seconds){
        if(preparedWorld_>=0||settings_.timeline)return;
        rarestage::show::Context context;context.current=world_;context.section=predictedSection_;context.category=settings_.sceneCategory;
        context.energy=std::clamp(sectionPhraseEnergy_+.15f,0.f,1.f);context.trend=sectionPhraseEnergy_-sectionLongEnergy_;
        for(int candidate=0;candidate<WorldCount;++candidate)context.fit[candidate]=worldFitScore(candidate,af,context.energy,false,predictedSection_);
        const auto plan=showDirector_.plan(context);if(plan.next<0)return;
        preparedWorld_=plan.next;preparedWorldAt_=seconds;preparedTargetSection_=predictedSection_;preparedSectionRevision_=sectionRevision_;
        directorCandidateWorld_=plan.next;directorRunnerUpWorld_=plan.alternative;directorCandidateScore_=plan.score;directorRunnerUpScore_=plan.runner;
        lastDirectorDecision_=L"REHEARSED / "+std::wstring(worldName(plan.next))+L" > "+worldName(plan.then);++directorDecisionRevision_;
    }
    void updateDirectorPrediction(const AudioFrame&af,float dt,float seconds){
        const float energy=directorEnergy(af),rise=sectionPhraseEnergy_-sectionLongEnergy_;
        const float dropEvidence=std::clamp(buildTension_*.70f+dropReadiness_*.18f+af.flux*.16f+(songSection_==BuildSection?.12f:0.f),0.f,1.f);
        const float breakdownEvidence=std::clamp(std::max(0.f,-rise)*4.0f+(.22f-energy)*2.0f+endingConfidence_*.55f,0.f,1.f);
        const float driveEvidence=std::clamp(energy*.78f+af.bass*.20f+af.mid*.12f-(songSection_==DropSection?.08f:0.f),0.f,1.f);
        const float buildEvidence=std::clamp(std::max(0.f,rise)*3.2f+af.flux*.45f+af.mid*.20f,0.f,1.f);
        int forecast=songSection_;float confidence=.34f;
        if(dropEvidence>confidence&&songSection_!=DropSection){forecast=DropSection;confidence=dropEvidence;}
        if(breakdownEvidence>confidence){forecast=heardActiveMusic_?BreakdownSection:OpenSection;confidence=breakdownEvidence;}
        if(driveEvidence>confidence&&songSection_==DropSection){forecast=DriveSection;confidence=driveEvidence;}
        if(buildEvidence>confidence&&songSection_!=BuildSection&&songSection_!=DropSection){forecast=BuildSection;confidence=buildEvidence;}
        if(forecast!=predictedSection_)predictedSectionConfidence_=std::min(predictedSectionConfidence_,.18f);predictedSection_=forecast;predictedSectionConfidence_+=(confidence-predictedSectionConfidence_)*std::clamp(dt*(confidence>predictedSectionConfidence_?5.2f:1.4f),0.f,1.f);
        float boundary=4.f-std::fmod(tempoPhase_,4.f);if(boundary<.05f)boundary=4.f;predictionLeadBeats_=std::clamp(boundary,0.f,8.f);
        predictionArmed_=predictedSection_!=songSection_&&predictedSectionConfidence_>.58f;
        if(predictionArmed_&&predictedSection_==DropSection&&predictedSectionConfidence_>.66f&&automatic_&&!frozen_&&seconds-worldStartedAt_>=36.f&&seconds>=nextWorldAt_-16.f)preparePredictedWorld(af,seconds);
        if(preparedWorld_>=0&&seconds-preparedWorldAt_>12.f&&songSection_!=DropSection){preparedWorld_=-1;noteRejectedCue(L"PREPARED CUE EXPIRED / DROP NOT CONFIRMED");}
        float densityTarget=songSection_==OpenSection?.88f:songSection_==BuildSection?1.02f:songSection_==DropSection?1.08f:songSection_==BreakdownSection?.78f:1.f;
        if(predictionArmed_&&predictedSection_==DropSection)densityTarget=std::lerp(densityTarget,1.12f,predictedSectionConfidence_);
        if(predictedSection_==BreakdownSection||endingConfidence_>.24f)densityTarget=std::lerp(densityTarget,.72f,std::max(predictedSectionConfidence_,endingConfidence_));
        if(energy<.06f)densityTarget=std::min(densityTarget,.84f);
        densityTarget=std::clamp(densityTarget,.70f,1.14f);const float rate=densityTarget>adaptiveCueDensity_?1.10f:.52f;adaptiveCueDensity_+=(densityTarget-adaptiveCueDensity_)*std::clamp(dt*rate,0.f,1.f);
    }
    int preparedOrChosenWorld(const AudioFrame&af){const bool valid=preparedWorld_>=0&&preparedTargetSection_==songSection_&&songSection_==DropSection&&sectionRevision_>=preparedSectionRevision_&&elapsed()-preparedWorldAt_<=12.f;if(valid){++autoSequence_;directorReason_=L"PREDICTED CUE / COMMIT ON CONFIRMED DROP";return preparedWorld_;}if(preparedWorld_>=0){preparedWorld_=-1;preparedTargetSection_=-1;noteRejectedCue(L"PREPARED CUE INVALID / SECTION CHANGED");}return chooseAutoWorld(af);}
    static float liquidProgress(float value){value=std::clamp(value,0.f,1.f);return value*value*value*(value*(value*6.f-15.f)+10.f);}
    static bool surfaceWall(int world){return world==0||world==3||world==9||world==11||world==13||world==15;}
    void selectPatternProgram(){visitPattern_=setMemory_.program(world_,songSection_);}
    static int authoredMotif(int step,int seed){static constexpr int development[16]={0,0,1,8,1,9,0,8,2,2,3,10,3,11,2,10};return (seed+development[(step%16+16)%16])%16;}
    void updateWorldProgression(){
        const float ageBeats=std::max(0.f,tempoPhase_-worldBeatOrigin_);
        loopPeriod_=worldFamily(world_)==1||worldFamily(world_)==3?96:64;
        loopPhase_=std::fmod(ageBeats,float(loopPeriod_))/float(loopPeriod_);
        const int movementCycle=int(std::floor(ageBeats/16.f));
        if(movementCycle!=movementRevision_){
            movementRevision_=movementCycle;
            static constexpr int preferred[SongSectionCount]={0,1,2,4,3};
            worldMovement_=preferred[songSection_];
            if(songSection_==DriveSection&&movementCycle%3==2)worldMovement_=1;
            if(songSection_==BuildSection&&movementCycle%3==2)worldMovement_=3;
            // A visit owns its pattern seed. A new stanza must not replace both sides of a blend.
            patternSeed_=float(visitPattern_);
            ++setMemoryRevision_;
        }
        const float stanza=std::fmod(ageBeats,16.f)/16.f;
        stanzaProgress_=stanza;choreographyPhase_=ageBeats/16.f;movementProgress_=liquidProgress(stanza);
        const float patternClock=tempoPhase_*.125f;
        const bool eightPatterns=world_!=2;
        const float phraseRamp=eightPatterns?std::clamp((std::fmod(patternClock,1.f)-.48f)/.51f,0.f,1.f):std::clamp((std::fmod(patternClock,1.f)-.08f)/.86f,0.f,1.f);
        patternBlend_=liquidProgress(phraseRamp*phraseRamp*(3.f-2.f*phraseRamp));
        const int patternCount=16;
        patternFrom_=eightPatterns?authoredMotif(int(std::floor(patternClock)),int(patternSeed_)):int(std::floor(patternClock)+patternSeed_)%patternCount;
        patternTo_=eightPatterns?authoredMotif(int(std::floor(patternClock))+1,int(patternSeed_)):(patternFrom_+1)%patternCount;
        // Freeze both the decision and the target for the complete stanza, even across a drop.
        if(identityCycle_!=movementCycle){
            identityCycle_=movementCycle;identityStanza_=(movementCycle%2==0);
            identityKind_=float((movementCycle/2)%2);
            if(identityStanza_){const int program=setMemory_.identity(int(identityKind_),rarestage::show::Scenes[world_].motion);identityEntrance_=program%8;identityExit_=program/8;}
        }
        const float assemble=liquidProgress((stanza-.16f)/.30f);
        const float release=1.f-liquidProgress((stanza-.72f)/.24f);
        identityWeight_=settings_.djIdentity&&identityStanza_?assemble*release:0.f;
    }
    void updateDirectorDiagnostics(const AudioFrame&af,float seconds,bool force=false){if(!force&&seconds-lastDirectorDiagnosticAt_<.20f)return;lastDirectorDiagnosticAt_=seconds;float energy=directorEnergy(af),bestScore=-1000.f,runnerScore=-1000.f;int best=world_,runner=world_;for(int candidate=0;candidate<WorldCount;++candidate){float score=worldFitScore(candidate,af,energy,false);if(score>bestScore){runnerScore=bestScore;runner=best;bestScore=score;best=candidate;}else if(score>runnerScore){runnerScore=score;runner=candidate;}}directorCandidateWorld_=preparedWorld_>=0?preparedWorld_:(route_.next>=0?route_.next:best);directorRunnerUpWorld_=route_.next>=0?route_.alternative:runner;directorCandidateScore_=route_.next>=0?route_.score:bestScore;directorRunnerUpScore_=route_.next>=0?route_.runner:runnerScore;directorConfidence_=std::clamp((bestScore-runnerScore)*.85f+sectionEvidence_*.32f+predictedSectionConfidence_*.12f,0.f,1.f);if(!automatic_)directorReason_=L"MANUAL WORLD / DIRECTOR STANDBY";else if(frozen_)directorReason_=L"AUTOMATION HOLD / WORLD PRESERVED";else if(settings_.timeline)directorReason_=L"SHOW TIMELINE / PHRASE ORDER";else if(preparedWorld_>=0)directorReason_=L"PREPARED / WAITING FOR CONFIRMED DROP";else if(seconds<nextWorldAt_){directorReason_=route_.next>=0?L"DEVELOP / NEXT "+std::wstring(worldName(route_.next))+L" > "+worldName(route_.then):L"DWELL / DEVELOP CURRENT WORLD";}else directorReason_=L"TIMED CYCLE / NEXT EIGHT-BEAT PHRASE";if(directorReason_!=previousDirectorReason_){if(directorReason_.find(L"WAITING")!=std::wstring::npos||directorReason_.find(L"HOLD")!=std::wstring::npos||directorReason_.find(L"DWELL")!=std::wstring::npos)noteRejectedCue(directorReason_);previousDirectorReason_=directorReason_;}}
    int chooseAutoWorld(const AudioFrame&af){if(settings_.timeline){static constexpr int orders[4][WorldCount]={{2,10,16,8,0,4,7,1,11,14,17,6,5,12,15,9,13,3,18,19,20},{0,7,2,16,10,4,8,1,11,14,17,6,5,12,15,9,13,3,18,19,20},{3,9,15,13,6,5,11,12,1,8,14,17,7,0,16,10,4,2,18,19,20},{3,13,9,15,12,11,5,14,6,8,17,7,1,0,16,10,4,2,18,19,20}};int choice=world_;for(int attempt=0;attempt<WorldCount;++attempt){const int candidate=orders[presetIndex_][timelineStep_++%WorldCount];if(settings_.sceneCategory==0||worldFamily(candidate)==settings_.sceneCategory-1){choice=candidate;break;}}directorCandidateWorld_=choice;directorRunnerUpWorld_=world_;directorReason_=L"SHOW TIMELINE / CATEGORY-SAFE PHRASE ORDER";return choice;}refreshShowPlan(af,true);++autoSequence_;
        directorCandidateWorld_=route_.next;directorRunnerUpWorld_=route_.alternative;
        directorCandidateScore_=route_.score;directorRunnerUpScore_=route_.runner;
        directorReason_=L"ROUTE / "+std::wstring(worldName(route_.next))+L" > "+worldName(route_.then);
        return route_.next>=0?route_.next:(world_+1)%WorldCount;
    }
    void refreshShowPlan(const AudioFrame&af,bool commit=false){
        const int phrase=int(tempoPhase_/16.f);
        const bool invalid=route_.next<0||routeSection_!=songSection_||routeCategory_!=settings_.sceneCategory;
        if(!invalid&&(commit||phrase==routePhrase_))return;
        rarestage::show::Context context;context.current=world_;context.section=songSection_;context.category=settings_.sceneCategory;context.sequence=autoSequence_;context.preferred=invalid?-1:route_.next;
        // Sustained musical energy keeps route choices from following individual kicks.
        context.energy=std::clamp(sectionLongEnergy_*.65f+sectionShortEnergy_*.35f,0.f,1.f);
        context.trend=sectionShortEnergy_-sectionLongEnergy_;
        AudioFrame sustained=af;sustained.kick=sustained.beat=sustained.snare=sustained.percussion=sustained.onset=sustained.flux=0;
        for(int candidate=0;candidate<WorldCount;++candidate)context.fit[candidate]=worldFitScore(candidate,sustained,context.energy,false);
        auto next=showDirector_.plan(context);
        // The final eight beats are a rehearsal lock; only a confirmed chapter or filter
        // change may replace the route. Earlier changes require a meaningful advantage.
        const float remainingBeats=(nextWorldAt_-elapsed())*visualBpm_/60.f;
        if(invalid||remainingBeats>8.f)route_=next;
        routeSection_=songSection_;routeCategory_=settings_.sceneCategory;routePhrase_=phrase;
    }
    float phraseDuration(const AudioFrame&af)const{if(af.bpm>=65.f&&af.bpm<=190.f&&af.tempoConfidence>.16f){float beatSeconds=60.f/af.bpm;int phraseBeats=std::clamp(int(std::round(settings_.sceneSeconds/(beatSeconds*8.f)))*8,8,256);return phraseBeats*beatSeconds;}return settings_.sceneSeconds;}
    float masteredCueDensity()const{return MasteredCueDensity(world_,songSection_,settings_.cueDensity,adaptiveCueDensity_);}
    void updateTimedDirector(const AudioFrame&af,float seconds){
        if(!automatic_||frozen_)return;
        refreshShowPlan(af);
        if(world_==5&&!worldChangeArmed_&&nextWorldAt_-seconds<=4.f*60.f/visualBpm_){
            worldChangeArmed_=true;worldCueBeat_=std::ceil((tempoPhase_+std::max(0.f,nextWorldAt_-seconds)*visualBpm_/60.f)/8.f)*8.f;
            if(worldCueBeat_-tempoPhase_<4)worldCueBeat_+=8;
            cornerMotion_.aim(seconds,seconds+(worldCueBeat_-tempoPhase_)*60.f/visualBpm_,true);
        }
        if(seconds>=nextWorldAt_&&!worldChangeArmed_){worldChangeArmed_=true;worldChangeReadyAt_=seconds;worldCueBeat_=std::ceil(tempoPhase_/8.f)*8.f;}
        if(worldChangeArmed_&&worldCueBeat_<0)worldCueBeat_=std::ceil(tempoPhase_);
        if(worldChangeArmed_&&tempoPhase_>=worldCueBeat_){selectWorld(preparedOrChosenWorld(af),true);scheduleNextWorld(af,seconds);updateDirectorDiagnostics(af,seconds,true);}
    }
    void servicePixelExit(const AudioFrame&af,float seconds){
        if(settings_.syncMode==2){pixelExitWorld_=-1;cornerMotion_.release(seconds);return;}
        if(pixelExitWorld_<0||!cornerMotion_.arrived()||(pixelExitDirected_&&frozen_))return;
        const int destination=pixelExitWorld_;const bool directed=pixelExitDirected_;
        pixelExitWorld_=-1;pixelExitCommit_=true;selectWorld(destination,directed);pixelExitCommit_=false;scheduleNextWorld(af,seconds);
    }
    void scheduleNextWorld(const AudioFrame&af,float now){nextWorldAt_=now+std::clamp(phraseDuration(af),40.f,120.f);worldChangeReadyAt_=nextWorldAt_;worldChangeArmed_=false;worldCueBeat_=-1;}
    void applyProductionDuty(int duty){settings_.productionDuty=std::clamp(duty,0,4);automatic_=true;settings_.automatic=1;settings_.timeline=0;switch(settings_.productionDuty){case 0:settings_.profile=0;settings_.masteringProfile=1;settings_.masterBrightness=.84f;settings_.cueDensity=.76f;settings_.sceneSeconds=96;settings_.transitionSeconds=1.8f;settings_.transitionStyle=1;break;case 1:settings_.profile=1;settings_.masteringProfile=1;settings_.masterBrightness=.88f;settings_.cueDensity=.90f;settings_.sceneSeconds=84;settings_.transitionSeconds=1.55f;settings_.transitionStyle=0;break;case 2:settings_.profile=2;settings_.masteringProfile=2;settings_.masterBrightness=.98f;settings_.cueDensity=1.14f;settings_.sceneSeconds=56;settings_.transitionSeconds=1.05f;settings_.transitionStyle=3;break;case 3:settings_.profile=0;settings_.masteringProfile=3;settings_.masterBrightness=.82f;settings_.cueDensity=.72f;settings_.sceneSeconds=104;settings_.transitionSeconds=2.2f;settings_.transitionStyle=2;break;default:settings_.profile=1;settings_.masteringProfile=1;settings_.masterBrightness=.80f;settings_.cueDensity=.80f;settings_.sceneSeconds=112;settings_.transitionSeconds=2.4f;settings_.transitionStyle=1;break;}if(elapsed()-transitionStarted_>=activeTransitionSeconds_){activeTransitionSeconds_=settings_.transitionSeconds;activeTransitionStyle_=settings_.transitionStyle;}chooseDirectorPalette(true);scheduleNextWorld(tuned(audio_.frame()),elapsed());}
    bool receiveStageSync(float seconds){if(settings_.syncMode!=2)return false;StageSyncPacket packet{};if(sync_.receive(packet,static_cast<std::uint32_t>(settings_.syncGroup))){const float phaseError=packet.tempoPhase-tempoPhase_;syncPhaseErrorMs_=std::abs(phaseError)*60000.f/std::clamp(packet.bpm,65.f,190.f);phaseCorrection_=std::clamp(phaseError,-.30f,.30f);visualBpm_+=(std::clamp(packet.bpm,65.f,190.f)-visualBpm_)*.30f;settings_.profile=packet.reactionProfile;if(packet.world!=world_){selectWorld(packet.world);if(packet.world==world_){activeTransitionStyle_=packet.transitionStyle;activeTransitionSeconds_=packet.transitionSeconds;worldBeatOrigin_=tempoPhase_+activeTransitionSeconds_*visualBpm_/60.f;}else if(queuedWorld_==packet.world){queuedMotionSeconds_=packet.transitionSeconds;queuedMotionStyle_=packet.transitionStyle;}}if(packet.section!=songSection_){songSection_=packet.section;sectionStartedAt_=seconds;sectionBeatOrigin_=packet.tempoPhase;}directorPalette_=packet.palette;settings_.productionDuty=packet.productionDuty;settings_.masteringProfile=packet.masteringProfile;settings_.stageLayout=packet.stageLayout;settings_.cueDensity=packet.cueDensity;settings_.masterBrightness=packet.masterBrightness;syncSequence_=packet.sequence;lastSyncPacketAt_=seconds;syncConnected_=true;syncStatus_=L"FOLLOWER G"+std::to_wstring(settings_.syncGroup)+L" / PACKET "+std::to_wstring(syncSequence_);}syncPacketAgeMs_=std::max(0.f,(seconds-lastSyncPacketAt_)*1000.f);if(syncPacketAgeMs_>2000.f){syncConnected_=false;syncStatus_=L"FOLLOWER HOLD / GROUP "+std::to_wstring(settings_.syncGroup);}return syncConnected_;}
    void publishStageSync(float seconds){if(settings_.syncMode!=1)return;if(seconds-lastSyncBroadcastAt_<.033f)return;lastSyncBroadcastAt_=seconds;StageSyncPacket packet{};packet.sequence=++syncSequence_;packet.groupId=static_cast<std::uint32_t>(settings_.syncGroup);packet.world=world_;packet.section=songSection_;packet.palette=directorPalette_;packet.productionDuty=settings_.productionDuty;packet.masteringProfile=settings_.masteringProfile;packet.stageLayout=settings_.stageLayout;packet.reactionProfile=settings_.profile;packet.transitionStyle=activeTransitionStyle_;packet.tempoPhase=tempoPhase_;packet.bpm=visualBpm_;packet.cueDensity=settings_.cueDensity;packet.masterBrightness=settings_.masterBrightness;packet.transitionSeconds=activeTransitionSeconds_;syncConnected_=sync_.broadcast(packet);lastSyncPacketAt_=seconds;syncPacketAgeMs_=0;syncPhaseErrorMs_=0;syncStatus_=syncConnected_?L"LEADER G"+std::to_wstring(settings_.syncGroup)+L" / PACKET "+std::to_wstring(syncSequence_):L"LEADER BROADCAST FAILED";}
    AudioFrame tuned(AudioFrame value)const{static const float profiles[]{.70f,.88f,1.04f,1.20f};float base=profiles[settings_.profile]*settings_.sensitivity;value.bass=std::clamp(value.bass*base*settings_.bassGain,0.f,1.f);value.mid=std::clamp(value.mid*base*settings_.midGain,0.f,1.f);value.high=std::clamp(value.high*base*settings_.highGain,0.f,1.f);value.level=std::clamp(value.level*base,0.f,1.f);value.peak=std::clamp(value.peak*base,0.f,1.f);value.beat=std::clamp(value.beat*base*settings_.beatGain,0.f,1.f);value.flux=std::clamp(value.flux*base,0.f,1.f);value.onset=std::clamp(value.onset*base*settings_.beatGain,0.f,1.f);value.percussion=std::clamp(value.percussion*base*settings_.highGain,0.f,1.f);value.snare=std::clamp(value.snare*base*settings_.midGain*settings_.beatGain,0.f,1.f);value.kick=std::clamp(value.kick*base*settings_.bassGain*settings_.beatGain,0.f,1.f);return value;}
    void adjustSetting(int direction){float d=direction*.05f;bool rebuild=false;switch(hudSelection_){case 0:settings_.profile=(settings_.profile+direction+4)%4;break;case 1:settings_.palette=(settings_.palette+direction+8)%8;directorPalette_=settings_.palette;break;case 2:settings_.sensitivity=std::clamp(settings_.sensitivity+d,.5f,1.5f);break;case 3:settings_.bassGain=std::clamp(settings_.bassGain+d,.5f,1.5f);break;case 4:settings_.midGain=std::clamp(settings_.midGain+d,.5f,1.5f);break;case 5:settings_.highGain=std::clamp(settings_.highGain+d,.5f,1.5f);break;case 6:settings_.beatGain=std::clamp(settings_.beatGain+d,.5f,1.5f);break;case 7:settings_.logoScale=std::clamp(settings_.logoScale+d,.75f,1.25f);break;case 8:settings_.sceneSeconds=std::clamp(settings_.sceneSeconds+direction*4.f,40.f,120.f);scheduleNextWorld(tuned(audio_.frame()),elapsed());break;case 9:settings_.transitionSeconds=std::clamp(settings_.transitionSeconds+direction*.1f,.2f,3.f);break;case 10:settings_.transitionStyle=(settings_.transitionStyle+direction+4)%4;break;case 11:settings_.quality=(settings_.quality+direction+4)%4;rebuild=true;break;case 12:settings_.vsync=!settings_.vsync;break;case 13:settings_.timeline=!settings_.timeline;timelineStep_=0;break;case 14:settings_.spout=!settings_.spout;break;case 15:settings_.cueDensity=std::clamp(settings_.cueDensity+d,.65f,1.25f);break;}settings_.save();if(rebuild&&swap_)resize(width_,height_);}
    static const wchar_t* presetName(int index){static const wchar_t*names[]={L"CLEAN OPEN",L"MELODIC BLOOM",L"RARE DROP",L"GLITCH FINALE"};return names[std::clamp(index,0,3)];}
    static std::wstring presetSection(int index){return L"Preset"+std::to_wstring(index+1);}
    void savePreset(int index){presetIndex_=std::clamp(index,0,3);auto section=presetSection(presetIndex_);settings_.save(section.c_str());auto path=SettingsPath().wstring();WritePrivateProfileStringW(section.c_str(),L"Valid",L"1",path.c_str());wchar_t value[8]{};std::swprintf(value,std::size(value),L"%d",world_);WritePrivateProfileStringW(section.c_str(),L"World",value,path.c_str());}
    void loadPreset(int index){presetIndex_=std::clamp(index,0,3);auto section=presetSection(presetIndex_);auto path=SettingsPath().wstring();if(GetPrivateProfileIntW(section.c_str(),L"Valid",0,path.c_str()))settings_.load(section.c_str());else{settings_=ShowSettings{};if(index==0){settings_.profile=0;settings_.sceneSeconds=88;settings_.transitionSeconds=1.8f;settings_.transitionStyle=1;}else if(index==1){settings_.profile=1;settings_.palette=2;settings_.sceneSeconds=72;settings_.transitionStyle=2;}else if(index==2){settings_.profile=2;settings_.sceneSeconds=56;settings_.transitionSeconds=1.2f;settings_.transitionStyle=3;}else{settings_.profile=3;settings_.palette=1;settings_.sceneSeconds=48;settings_.transitionSeconds=.9f;settings_.transitionStyle=2;settings_.timeline=1;}settings_.monitor=std::max(0,settings_.monitor);}
        settings_.djName=SanitizeDjName(settings_.djName);effectiveTempoAnchor_=settings_.tempoAnchor==2?130:128;audio_.setProfile(settings_.audioProfile);sync_.setMode(settings_.syncMode);automatic_=settings_.automatic!=0;directorPalette_=settings_.palette;if(automatic_&&!settings_.timeline)chooseDirectorPalette(true);int legacyWorld=GetPrivateProfileIntW(section.c_str(),L"Scene",index==0?2:index==1?0:index==2?1:3,path.c_str())%WorldCount;selectWorld(GetPrivateProfileIntW(section.c_str(),L"World",legacyWorld,path.c_str()));timelineStep_=0;scheduleNextWorld(tuned(audio_.frame()),elapsed());settings_.save();if(swap_)resize(width_,height_);
    }
    void handleHudPointer(POINT point,int direction){if(!hudVisible_)return;float left=panelLeft(),top=HudTop+HudRowsTop;int row=int((point.y-top)/HudRowHeight);if(point.x<left||point.x>left+HudWidth||row<0||row>=SettingCount)return;hudSelection_=row;if(direction==0){if(point.x>=left+HudButtonLeft&&point.x<=left+HudButtonLeft+HudButtonSize)direction=-1;else if(point.x>=left+HudButtonRight&&point.x<=left+HudButtonRight+HudButtonSize)direction=1;}if(direction)adjustSetting(direction);}
    void updateHud(const AudioFrame&raw){hudFrame_=tuned(raw);hudDevice_=audio_.name();if(hudDevice_.size()>43)hudDevice_=hudDevice_.substr(0,40)+L"...";}
    float panelLeft()const{return std::max(12.f,float(width_)-HudWidth-HudTop);}
    static std::wstring percent(float value){return std::to_wstring((int)std::round(value*100))+L"%";}
    std::wstring settingValue(int index)const{static const wchar_t*profiles[]={L"CALM",L"BALANCED",L"STAGE",L"AGGRESSIVE"};static const wchar_t*transitions[]={L"ROW FLOW",L"RIBBON FLOW",L"ORBIT FLOW",L"DEPTH FOLD"};static const wchar_t*quality[]={L"PERFORMANCE",L"BALANCED",L"NATIVE HD",L"ULTRA 4K"};switch(index){case 0:return profiles[settings_.profile];case 1:return automatic_&&!settings_.timeline?std::wstring(L"AUTO / ")+paletteName(directorPalette_):paletteName(settings_.palette);case 2:return percent(settings_.sensitivity);case 3:return percent(settings_.bassGain);case 4:return percent(settings_.midGain);case 5:return percent(settings_.highGain);case 6:return percent(settings_.beatGain);case 7:return percent(settings_.logoScale);case 8:return std::to_wstring((int)settings_.sceneSeconds)+L" SEC";case 9:return std::to_wstring((int)std::round(settings_.transitionSeconds*10))+L"/10 SEC";case 10:return transitions[settings_.transitionStyle];case 11:return quality[settings_.quality];case 12:return settings_.vsync?L"DISPLAY / 60":L"UNLOCKED / 120";case 13:return settings_.timeline?L"SHOW TIMELINE":L"SMART DIRECTOR";case 14:
#ifdef RARESTAGE_SPOUT
        return settings_.spout?(spoutReady_?L"ON / CLEAN":L"STARTING"):L"OFF";
#else
        return L"NOT BUILT";
#endif
        case 15:return percent(settings_.cueDensity);default:return L"";}}
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
        static const wchar_t*labels[]={L"REACTION",L"PALETTE",L"SENSITIVITY",L"BASS / KICK",L"MIDS / SNARE",L"HIGHS / HATS",L"BEAT",L"LOGO",L"WORLD DWELL",L"MOTION TIME",L"MOTION PATH",L"RENDER PROFILE",L"FRAME OUTPUT",L"SHOW MODE",L"SPOUT OUTPUT",L"CUE DENSITY"};
        for(int row=0;row<SettingCount;++row){float y=HudTop+HudRowsTop+row*HudRowHeight;auto rowRect=D2D1::RoundedRect(D2D1::RectF(left+12,y,right-12,y+HudRowHeight-2),5,5);if(row==hudSelection_)d2dContext_->FillRoundedRectangle(rowRect,selectionBrush_.Get());text(labels[row],hudFormat_.Get(),row==hudSelection_?textBrush_.Get():mutedBrush_.Get(),D2D1::RectF(left+22,y,left+HudButtonLeft-8,y+HudRowHeight-2));text(L"−",centerFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+HudButtonLeft,y+2,left+HudButtonLeft+HudButtonSize,y+HudRowHeight-4));text(settingValue(row),valueFormat_.Get(),textBrush_.Get(),D2D1::RectF(left+HudButtonLeft+HudButtonSize+5,y,right-HudButtonSize-32,y+HudRowHeight-2));text(L"+",centerFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+HudButtonRight,y+2,left+HudButtonRight+HudButtonSize,y+HudRowHeight-4));}
        float signalTop=HudTop+HudRowsTop+SettingCount*HudRowHeight+9;text(L"LIVE SIGNAL",smallFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+20,signalTop,right-20,signalTop+16));const float values[]={hudFrame_.bass,hudFrame_.mid,hudFrame_.kick,hudFrame_.snare,hudFrame_.percussion};const wchar_t*meterNames[]={L"BASS",L"MID",L"KICK",L"SNARE",L"HATS"};for(int i=0;i<5;++i){float y=signalTop+19+i*15;text(meterNames[i],smallFormat_.Get(),mutedBrush_.Get(),D2D1::RectF(left+20,y,left+62,y+14));auto track=D2D1::RoundedRect(D2D1::RectF(left+68,y+3,right-70,y+10),3,3);d2dContext_->FillRoundedRectangle(track,meterBrush_.Get());auto fill=D2D1::RoundedRect(D2D1::RectF(left+68,y+3,left+68+(right-left-138)*std::clamp(values[i],0.f,1.f),y+10),3,3);d2dContext_->FillRoundedRectangle(fill,accentBrush_.Get());text(percent(values[i]),smallFormat_.Get(),textBrush_.Get(),D2D1::RectF(right-64,y,right-20,y+14));}
        float statusTop=signalTop+101;std::wstring bpm=hudFrame_.bpm>0?std::to_wstring((int)std::round(hudFrame_.bpm))+L" BPM":L"BPM --";text(std::wstring(L"WORLD ")+std::to_wstring(world_+1)+L"  /  "+worldName(world_)+(automatic_?L"  [AUTO]":L"  [MANUAL]"),hudFormat_.Get(),textBrush_.Get(),D2D1::RectF(left+20,statusTop,right-20,statusTop+20));text(bpm+L"  •  ACT "+sectionName(songSection_)+L"  •  "+hudDevice_,smallFormat_.Get(),mutedBrush_.Get(),D2D1::RectF(left+20,statusTop+21,right-20,statusTop+38));text(L"CLICK / WHEEL TO ADJUST   •   TAB HIDE   •   F1-F4 PRESETS",smallFormat_.Get(),accentBrush_.Get(),D2D1::RectF(left+20,statusTop+43,right-20,statusTop+60));
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
    void refreshDisplays(bool preserve){
        auto next=EnumerateStageDisplays();
        if(preserve&&!displays_.empty()){
            unsigned mask=0;int selected=0;
            for(size_t n=0;n<next.size();++n)for(size_t old=0;old<displays_.size();++old)if(next[n].name==displays_[old].name){if(settings_.mirrorMask&(1u<<old))mask|=1u<<n;if(int(old)==settings_.monitor)selected=int(n);}
            settings_.mirrorMask=mask;settings_.monitor=selected;
        }
        displays_=std::move(next);monitorCount_=int(displays_.size());settings_.monitor=std::clamp(settings_.monitor,0,std::max(0,monitorCount_-1));
        settings_.mirrorMask&=(1u<<displays_.size())-1u;displaysDirty_=false;outputsDirty_=true;if(fullscreen_)placeFullscreen();
    }
    void placeFullscreen(){if(displays_.empty())return;const RECT r=displays_[settings_.monitor].bounds;SetWindowPos(hwnd_,HWND_TOP,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED|SWP_NOACTIVATE);}
    void selectMonitor(int index){
        if(displays_.empty())return;settings_.monitor=std::clamp(index,0,int(displays_.size())-1);outputsDirty_=true;
        if(fullscreen_)placeFullscreen();else{const RECT r=displays_[settings_.monitor].bounds;RECT current{};GetWindowRect(hwnd_,&current);const int w=std::min(current.right-current.left,r.right-r.left),h=std::min(current.bottom-current.top,r.bottom-r.top);SetWindowPos(hwnd_,nullptr,r.left+(r.right-r.left-w)/2,r.top+(r.bottom-r.top-h)/2,w,h,SWP_NOZORDER|SWP_NOACTIVATE);}
        if(!validationMode_)settings_.save();
    }
    void cycleMonitor(){selectMonitor((settings_.monitor+1)%std::max(1,monitorCount_));}
    bool initControl(){
        using namespace rarestage::control;controlMapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(SharedState),MappingName);
        if(!controlMapping_)return false;if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(controlMapping_);controlMapping_=nullptr;StartupLog("RareStage control endpoint already exists");return false;}
        controlState_=static_cast<SharedState*>(MapViewOfFile(controlMapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedState)));if(!controlState_){CloseHandle(controlMapping_);controlMapping_=nullptr;return false;}
        ZeroMemory(controlState_,sizeof(SharedState));controlState_->protocolVersion=ProtocolVersion;controlState_->structSize=sizeof(SharedState);controlState_->worldCount=WorldCount;controlState_->sectionCount=SongSectionCount;controlState_->rendererProcessId=GetCurrentProcessId();controlState_->heartbeatTickMs=GetTickCount64();return true;
    }
    void shutdownControl(){if(controlState_){UnmapViewOfFile(controlState_);controlState_=nullptr;}if(controlMapping_){CloseHandle(controlMapping_);controlMapping_=nullptr;}}
    void publishControlState(const AudioFrame&af,float energy,float transition,float seconds){
        using namespace rarestage::control;if(!controlState_||seconds-lastControlPublish_<.025f)return;lastControlPublish_=seconds;BeginWrite(controlState_);controlState_->protocolVersion=ProtocolVersion;controlState_->structSize=sizeof(SharedState);controlState_->worldCount=WorldCount;controlState_->sectionCount=SongSectionCount;controlState_->heartbeatTickMs=GetTickCount64();controlState_->stateRevision=++controlRevision_;controlState_->rendererProcessId=GetCurrentProcessId();
        std::uint32_t flags=RendererReady;if(automatic_)flags|=Automatic;if(frozen_)flags|=Frozen;if(blackout_)flags|=Blackout;if(hudVisible_)flags|=StageOverlayVisible;if(settings_.timeline)flags|=Timeline;if(settings_.spout)flags|=SpoutEnabled;if(fullscreen_)flags|=Fullscreen;if(settings_.vsync)flags|=VsyncEnabled;if(seconds<manualSectionOverrideUntil_)flags|=ManualSectionOverride;if(endingConfidence_>.42f)flags|=MusicalEnding;if(captureRequested_)flags|=CapturePending;if(rehearsalActive_)flags|=RehearsalActive;if(predictionArmed_)flags|=PredictiveCueArmed;if(syncConnected_)flags|=SyncConnected;if(settings_.syncMode==1)flags|=SyncLeader;if(settings_.syncMode==2)flags|=SyncFollower;if(settings_.masteringProfile==1)flags|=LedSafeMaster;if(settings_.djIdentity&&!settings_.djName.empty())flags|=DjIdentityVisible;
#ifdef RARESTAGE_SPOUT
        if(spoutReady_)flags|=SpoutReady;
#endif
        if(hudDevice_.find(L"unavailable")==std::wstring::npos&&hudDevice_.find(L"Starting")==std::wstring::npos)flags|=AudioAvailable;
        controlState_->mirrorMask=settings_.mirrorMask;controlState_->mirrorCount=int(outputs_.count());controlState_->displayOutputStatus=displayOutputStatus_;
        for(size_t n=0;n<8;++n){std::wstring label;if(n<displays_.size()){const auto&d=displays_[n];label=L"DISPLAY "+std::to_wstring(n+1)+L"  /  "+std::to_wstring(d.bounds.right-d.bounds.left)+L" x "+std::to_wstring(d.bounds.bottom-d.bounds.top);}wcsncpy_s(controlState_->displayNames[n],label.c_str(),_TRUNCATE);}
        controlState_->flags=flags;controlState_->world=world_;controlState_->previousWorld=previousWorld_;controlState_->songSection=songSection_;controlState_->preset=presetIndex_;controlState_->reactionProfile=settings_.profile;controlState_->directorPalette=directorPalette_;controlState_->fallbackPalette=settings_.palette;controlState_->transitionStyle=activeTransitionStyle_;controlState_->quality=settings_.quality;controlState_->audioProfile=settings_.audioProfile;controlState_->resolvedAudioProfile=af.resolvedProfile;controlState_->monitor=settings_.monitor;controlState_->monitorCount=monitorCount_;controlState_->directorCandidateWorld=directorCandidateWorld_;controlState_->directorRunnerUpWorld=directorRunnerUpWorld_;controlState_->predictedSection=predictedSection_;controlState_->preparedWorld=preparedWorld_;controlState_->masteringProfile=settings_.masteringProfile;controlState_->stageLayout=settings_.stageLayout;controlState_->productionDuty=settings_.productionDuty;controlState_->syncMode=settings_.syncMode;controlState_->syncGroup=settings_.syncGroup;controlState_->worldFamily=worldFamily(world_);controlState_->worldMovement=worldMovement_;controlState_->patternFrom=patternFrom_;controlState_->patternTo=patternTo_;controlState_->sceneCategory=settings_.sceneCategory;controlState_->tempoAnchor=settings_.tempoAnchor;controlState_->effectiveTempoAnchor=effectiveTempoAnchor_;controlState_->loopPeriod=loopPeriod_;controlState_->choreographyStage=stanzaProgress_<.16f?0:stanzaProgress_<.46f?1:stanzaProgress_<.72f?2:3;controlState_->identityKind=int(identityKind_);controlState_->lastCommandSerial=lastCommandSerial_;controlState_->lastCommandStatus=lastCommandStatus_;
        controlState_->fps=lastFps_;controlState_->bpm=af.bpm;controlState_->tempoConfidence=af.tempoConfidence;controlState_->bass=af.bass;controlState_->mid=af.mid;controlState_->high=af.high;controlState_->beat=af.beat;controlState_->onset=af.onset;controlState_->percussion=af.percussion;controlState_->snare=af.snare;controlState_->kick=af.kick;controlState_->kickConfidence=af.kickConfidence;controlState_->snareConfidence=af.snareConfidence;controlState_->hatConfidence=af.hatConfidence;controlState_->kickEvidence=af.kickEvidence;controlState_->snareEvidence=af.snareEvidence;controlState_->hatEvidence=af.hatEvidence;controlState_->transientAmbiguity=af.transientAmbiguity;controlState_->detectorThreshold=af.detectorThreshold;controlState_->calibrationStability=af.calibrationStability;controlState_->analysisLatencyMs=af.analysisLatencyMs;controlState_->lowFluxShare=af.lowFluxShare;controlState_->midFluxShare=af.midFluxShare;controlState_->highFluxShare=af.highFluxShare;controlState_->lowPowerShare=af.lowPowerShare;controlState_->midPowerShare=af.midPowerShare;controlState_->highPowerShare=af.highPowerShare;controlState_->calibrationProgress=af.calibrationProgress;controlState_->sampleRate=af.sampleRate;controlState_->transientClass=af.transientClass;controlState_->acceptedKickCount=af.acceptedKickCount;controlState_->acceptedSnareCount=af.acceptedSnareCount;controlState_->acceptedHatCount=af.acceptedHatCount;controlState_->rejectedTransientCount=af.rejectedTransientCount;controlState_->energy=energy;controlState_->transitionProgress=transition;controlState_->manualOverrideRemainingSeconds=std::max(0.f,manualSectionOverrideUntil_-seconds);controlState_->sensitivity=settings_.sensitivity;controlState_->bassGain=settings_.bassGain;controlState_->midGain=settings_.midGain;controlState_->highGain=settings_.highGain;controlState_->beatGain=settings_.beatGain;controlState_->logoScale=settings_.logoScale;controlState_->worldDwellSeconds=settings_.sceneSeconds;controlState_->transitionSeconds=settings_.transitionSeconds;controlState_->directorCandidateScore=directorCandidateScore_;controlState_->directorRunnerUpScore=directorRunnerUpScore_;controlState_->sectionEvidence=sectionEvidence_;controlState_->endingConfidence=endingConfidence_;controlState_->gpuFrameMs=gpuFrameMs_;controlState_->gpuPeakMs=gpuPeakMs_;controlState_->gpuBudgetMs=gpuBudgetMs();controlState_->gpuBudgetUtilization=gpuBudgetMs()>0?gpuFrameMs_/gpuBudgetMs():0;controlState_->cueDensity=settings_.cueDensity;controlState_->worldCueDensity=masteredCueDensity();controlState_->rehearsalSeconds=rehearsalActive_?std::max(0.f,seconds-rehearsalStartedAt_):0.f;controlState_->captureCount=captureCount_;controlState_->captureStatus=captureStatus_;controlState_->rehearsalRevision=rehearsalRevision_;
        controlState_->predictedSectionConfidence=predictedSectionConfidence_;controlState_->predictionLeadBeats=predictionLeadBeats_;controlState_->adaptiveCueDensity=adaptiveCueDensity_;controlState_->tempoRecoveryConfidence=tempoRecoveryConfidence_;controlState_->masterBrightness=settings_.masterBrightness;controlState_->syncPhaseErrorMs=syncPhaseErrorMs_;controlState_->syncPacketAgeMs=syncPacketAgeMs_;controlState_->sectionAgeBeats=std::max(0.f,tempoPhase_-sectionBeatOrigin_);controlState_->buildTension=buildTension_;controlState_->dropReadiness=dropReadiness_;controlState_->musicalNovelty=musicalNovelty_;controlState_->songArc=songArc_;controlState_->movementProgress=movementProgress_;controlState_->patternBlend=patternBlend_;controlState_->loopPhase=loopPhase_;controlState_->transitionCompatibility=transitionCompatibility_;controlState_->choreographyPhase=choreographyPhase_;controlState_->identityWeight=identityWeight_;controlState_->stanzaProgress=stanzaProgress_;controlState_->directorDecisionRevision=directorDecisionRevision_;controlState_->acceptedDirectorCueCount=acceptedDirectorCueCount_;controlState_->rejectedDirectorCueCount=rejectedDirectorCueCount_;controlState_->syncSequence=syncSequence_;
        wcsncpy_s(controlState_->playbackDevice,std::size(controlState_->playbackDevice),hudDevice_.c_str(),_TRUNCATE);wcsncpy_s(controlState_->worldName,std::size(controlState_->worldName),worldName(world_),_TRUNCATE);wcsncpy_s(controlState_->directorReason,std::size(controlState_->directorReason),directorReason_.c_str(),_TRUNCATE);wcsncpy_s(controlState_->lastCapturePath,std::size(controlState_->lastCapturePath),lastCapturePath_.c_str(),_TRUNCATE);wcsncpy_s(controlState_->lastDirectorDecision,std::size(controlState_->lastDirectorDecision),lastDirectorDecision_.c_str(),_TRUNCATE);wcsncpy_s(controlState_->lastRejectedCue,std::size(controlState_->lastRejectedCue),lastRejectedCue_.c_str(),_TRUNCATE);wcsncpy_s(controlState_->syncStatus,std::size(controlState_->syncStatus),syncStatus_.c_str(),_TRUNCATE);wcsncpy_s(controlState_->movementName,std::size(controlState_->movementName),movementName(worldMovement_),_TRUNCATE);static const wchar_t*motifNames[]={L"ROW CONVEYOR",L"CHEVRON SWEEP",L"RADIAL RIPPLE",L"SPLIT GATE",L"SERPENTINE",L"HELIX",L"BLOCK BANKS",L"CROSS WAVE",L"COLUMN ELEVATOR",L"ROW REVEAL",L"STAGGERED RACK",L"PERIMETER CHASE",L"LADDER",L"OPPOSING ROWS",L"DIAMOND CHASE",L"CROSS PULSE"};const std::wstring patternLabel=surfaceWall(world_)?motifNames[patternFrom_%16]:L"PATTERN "+std::to_wstring(patternFrom_+1)+L" / 16";wcsncpy_s(controlState_->patternName,std::size(controlState_->patternName),patternLabel.c_str(),_TRUNCATE);wcsncpy_s(controlState_->djName,std::size(controlState_->djName),settings_.djName.c_str(),_TRUNCATE);wcsncpy_s(controlState_->categoryName,std::size(controlState_->categoryName),categoryName(settings_.sceneCategory),_TRUNCATE);static const wchar_t*choreoNames[]={L"ESTABLISH",L"ROLL",L"RESOLVE",L"RELEASE"};wcsncpy_s(controlState_->choreographyName,std::size(controlState_->choreographyName),choreoNames[controlState_->choreographyStage],_TRUNCATE);wcsncpy_s(controlState_->identityTarget,std::size(controlState_->identityTarget),identityKind_>.5f?settings_.djName.c_str():L"RARE MARK",_TRUNCATE);EndWrite(controlState_);
    }
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
    bool rebuildIdentityTexture(){
        constexpr int textureWidth=1024,textureHeight=256;BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=textureWidth;info.bmiHeader.biHeight=-textureHeight;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void*rawPixels=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&rawPixels,nullptr,0);if(!dc||!bitmap||!rawPixels){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return false;}HGDIOBJ oldBitmap=SelectObject(dc,bitmap);std::memset(rawPixels,0,textureWidth*textureHeight*4);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));int fontHeight=164;HFONT font=CreateFontW(-fontHeight,0,0,0,FW_BLACK,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FF_SWISS,L"Segoe UI Variable Display");HGDIOBJ oldFont=SelectObject(dc,font);SIZE measured{};GetTextExtentPoint32W(dc,settings_.djName.c_str(),static_cast<int>(settings_.djName.size()),&measured);if(measured.cx>930){SelectObject(dc,oldFont);DeleteObject(font);fontHeight=std::max(48,int(fontHeight*930.f/measured.cx));font=CreateFontW(-fontHeight,0,0,0,FW_BLACK,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FF_SWISS,L"Segoe UI Variable Display");oldFont=SelectObject(dc,font);}RECT bounds{32,12,textureWidth-32,textureHeight-12};DrawTextW(dc,settings_.djName.c_str(),static_cast<int>(settings_.djName.size()),&bounds,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);GdiFlush();std::vector<BYTE>rgba(size_t(textureWidth)*textureHeight*4);const auto*bgra=static_cast<const BYTE*>(rawPixels);for(size_t pixel=0;pixel<size_t(textureWidth)*textureHeight;++pixel){const BYTE alpha=std::max({bgra[pixel*4],bgra[pixel*4+1],bgra[pixel*4+2]});rgba[pixel*4]=rgba[pixel*4+1]=rgba[pixel*4+2]=255;rgba[pixel*4+3]=alpha;}SelectObject(dc,oldFont);SelectObject(dc,oldBitmap);DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);D3D11_TEXTURE2D_DESC td{};td.Width=textureWidth;td.Height=textureHeight;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{rgba.data(),textureWidth*4,0};ComPtr<ID3D11Texture2D>texture;ComPtr<ID3D11ShaderResourceView>view;if(FAILED(device_->CreateTexture2D(&td,&data,&texture))||FAILED(device_->CreateShaderResourceView(texture.Get(),nullptr,&view)))return false;identitySrv_=view;identityTextureName_=settings_.djName;StartupLog("DJ identity mask rebuilt");return true;
    }
    bool initGpuTimers(){
        for(auto&slot:gpuTimers_){D3D11_QUERY_DESC query{};query.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;if(FAILED(device_->CreateQuery(&query,&slot.disjoint)))return false;query.Query=D3D11_QUERY_TIMESTAMP;if(FAILED(device_->CreateQuery(&query,&slot.start))||FAILED(device_->CreateQuery(&query,&slot.end)))return false;}return true;
    }
    float gpuBudgetMs()const{static constexpr float budgets[]{4.8f,6.4f,8.0f,8.0f};return budgets[std::clamp(settings_.quality,0,3)];}
    void pollGpuTimers(){
        for(auto&slot:gpuTimers_){if(!slot.pending)continue;D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};if(context_->GetData(slot.disjoint.Get(),&disjoint,sizeof(disjoint),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)continue;UINT64 start=0,end=0;if(context_->GetData(slot.start.Get(),&start,sizeof(start),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK||context_->GetData(slot.end.Get(),&end,sizeof(end),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)continue;slot.pending=false;if(disjoint.Disjoint||!disjoint.Frequency||end<=start)continue;const float milliseconds=float(double(end-start)*1000.0/double(disjoint.Frequency));if(collectValidationGpu_)validationGpuSamples_.push_back(milliseconds);gpuFrameMs_=gpuFrameMs_<=0?milliseconds:gpuFrameMs_+(milliseconds-gpuFrameMs_)*.12f;gpuPeakMs_=std::max(milliseconds,gpuPeakMs_*.996f);}
    }
    void beginGpuTimer(){auto&slot=gpuTimers_[gpuTimerWrite_];if(slot.pending){gpuTimerActive_=false;return;}context_->Begin(slot.disjoint.Get());context_->End(slot.start.Get());gpuTimerActive_=true;}
    void endGpuTimer(){if(!gpuTimerActive_)return;auto&slot=gpuTimers_[gpuTimerWrite_];context_->End(slot.end.Get());context_->End(slot.disjoint.Get());slot.pending=true;gpuTimerWrite_=(gpuTimerWrite_+1)%gpuTimers_.size();gpuTimerActive_=false;}
    bool saveBackBufferPng(const std::filesystem::path&path){
        if(!backBuffer_)return false;D3D11_TEXTURE2D_DESC desc{};backBuffer_->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;ComPtr<ID3D11Texture2D>staging;if(FAILED(device_->CreateTexture2D(&desc,nullptr,&staging)))return false;context_->CopyResource(staging.Get(),backBuffer_.Get());D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context_->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
        ComPtr<IWICImagingFactory>factory;ComPtr<IWICStream>stream;ComPtr<IWICBitmapEncoder>encoder;ComPtr<IWICBitmapFrameEncode>frame;ComPtr<IPropertyBag2>properties;bool ok=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateStream(&stream))&&SUCCEEDED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE))&&SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))&&SUCCEEDED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache))&&SUCCEEDED(encoder->CreateNewFrame(&frame,&properties))&&SUCCEEDED(frame->Initialize(properties.Get()))&&SUCCEEDED(frame->SetSize(desc.Width,desc.Height));WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;if(ok)ok=SUCCEEDED(frame->SetPixelFormat(&format))&&IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA)&&SUCCEEDED(frame->WritePixels(desc.Height,mapped.RowPitch,mapped.RowPitch*desc.Height,static_cast<BYTE*>(mapped.pData)))&&SUCCEEDED(frame->Commit())&&SUCCEEDED(encoder->Commit());context_->Unmap(staging.Get(),0);return ok;
    }
    std::filesystem::path capturePath(bool reference)const{
        if(reference){wchar_t name[96]{};std::swprintf(name,std::size(name),L"q%d-world-%02d-drop.png",referenceQuality_,referenceWorld_+1);return std::filesystem::path(L"RareStage-reference-captures")/name;}SYSTEMTIME now{};GetLocalTime(&now);wchar_t name[128]{};std::swprintf(name,std::size(name),L"RareStage-%04u%02u%02u-%02u%02u%02u-w%02d-a%d.png",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,world_+1,songSection_);auto directory=SettingsPath().parent_path()/L"Captures";std::error_code error;std::filesystem::create_directories(directory,error);return directory/name;
    }
    void serviceCapture(){
        if(referenceCaptureMode_){if(++referenceWarmup_<8)return;referenceWarmup_=0;const auto path=capturePath(true);const bool saved=saveBackBufferPng(path);std::ofstream manifest("RareStage-reference-captures.csv",std::ios::app|std::ios::binary);if(referenceQuality_==0&&referenceWorld_==0)manifest<<"quality,world,act,cue_density,file,gpu_ms,budget_ms,status\n";manifest<<referenceQuality_<<','<<referenceWorld_<<",Drop,"<<masteredCueDensity()<<','<<path.generic_string()<<','<<gpuFrameMs_<<','<<gpuBudgetMs()<<','<<(saved?"captured":"failed")<<'\n';if(!saved){captureStatus_=-1;PostMessageW(hwnd_,WM_CLOSE,0,0);return;}++captureCount_;++referenceWorld_;if(referenceWorld_>=WorldCount){referenceWorld_=0;++referenceQuality_;if(referenceQuality_>=4){captureStatus_=1;PostMessageW(hwnd_,WM_CLOSE,0,0);return;}settings_.quality=referenceQuality_;resize(width_,height_);}world_=referenceWorld_;previousWorld_=world_;songSection_=DropSection;return;}
        if(!captureRequested_)return;captureRequested_=false;const auto path=capturePath(false);const bool saved=saveBackBufferPng(path);captureStatus_=saved?1:-1;if(saved){++captureCount_;lastCapturePath_=path.wstring();}else lastCapturePath_=L"Capture failed";
    }
    bool createGpu(){DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=hwnd_;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        D3D_FEATURE_LEVEL fl;UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        flags|=D3D11_CREATE_DEVICE_DEBUG;
#endif
        HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&sd,&swap_,&device_,&fl,&context_);
        if(FAILED(hr)){swap_.Reset();device_.Reset();context_.Reset();hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&sd,&swap_,&device_,&fl,&context_);}
        if(FAILED(hr)){StartupLog("D3D11 device and swap chain creation failed");return false;}
        if(!outputs_.initialize(device_.Get(),hwnd_,validationMode_)){StartupLog("Display output initialization failed");return false;}
        ComPtr<IDXGIDevice> dxgiDevice;ComPtr<IDXGIAdapter> activeAdapter;
        if(SUCCEEDED(device_.As(&dxgiDevice))&&SUCCEEDED(dxgiDevice->GetAdapter(&activeAdapter))){DXGI_ADAPTER_DESC adapterDesc{};activeAdapter->GetDesc(&adapterDesc);std::ofstream adapterLog("RareStage-adapter.txt");std::wstring adapterName=adapterDesc.Description;adapterLog<<std::string(adapterName.begin(),adapterName.end());}
        ComPtr<ID3DBlob> vsb,sceneBlob,morphBlob,bloomBlob,compositeBlob,err;
        if(FAILED(CompileStageShader("vs","vs_5_0",&vsb,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage vertex shader error",MB_OK|MB_ICONERROR);return false;}
        err.Reset();if(FAILED(CompileStageShader("ps","ps_5_0",&sceneBlob,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage scene shader error",MB_OK|MB_ICONERROR);return false;}
        err.Reset();if(FAILED(CompileStageShader("psBloom","ps_5_0",&bloomBlob,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage bloom shader error",MB_OK|MB_ICONERROR);return false;}
        err.Reset();if(FAILED(CompileStageShader("psComposite","ps_5_0",&compositeBlob,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage composite shader error",MB_OK|MB_ICONERROR);return false;}
        err.Reset();if(FAILED(CompileStageShader("psMorph","ps_5_0",&morphBlob,&err))){if(err)MessageBoxA(hwnd_,(const char*)err->GetBufferPointer(),"RareStage morph shader error",MB_OK|MB_ICONERROR);return false;}
        if(FAILED(device_->CreatePixelShader(morphBlob->GetBufferPointer(),morphBlob->GetBufferSize(),nullptr,&morphPs_)))return false;
        if(FAILED(device_->CreateVertexShader(vsb->GetBufferPointer(),vsb->GetBufferSize(),nullptr,&vs_))){StartupLog("Vertex shader creation failed");return false;}
        if(FAILED(device_->CreatePixelShader(sceneBlob->GetBufferPointer(),sceneBlob->GetBufferSize(),nullptr,&scenePs_))||FAILED(device_->CreatePixelShader(bloomBlob->GetBufferPointer(),bloomBlob->GetBufferSize(),nullptr,&bloomPs_))||FAILED(device_->CreatePixelShader(compositeBlob->GetBufferPointer(),compositeBlob->GetBufferSize(),nullptr,&compositePs_))){StartupLog("Festival shader creation failed");return false;}
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(device_->CreateBuffer(&bd,nullptr,&constants_))){StartupLog("Frame constant buffer creation failed");return false;}if(!loadLogo())return false;if(!rebuildIdentityTexture()){StartupLog("DJ identity mask creation failed");return false;}if(!initGpuTimers()){StartupLog("GPU timer query initialization failed");return false;}if(!initOverlay()){StartupLog("Direct2D show control initialization failed");return false;}initSpout();RECT r;GetClientRect(hwnd_,&r);resize(r.right,r.bottom);return morphRtv_.Get()&&morphSrv_.Get()&&morphSourceRtv_.Get()&&morphSourceSrv_.Get()&&rtv_.Get()&&sceneRtv_.Get()&&sceneSrv_.Get()&&bloomRtv_.Get()&&bloomSrv_.Get()&&d2dTarget_.Get();}
    float renderScale()const{static constexpr float scales[]{.62f,.78f,1.f,1.25f};return scales[std::clamp(settings_.quality,0,3)];}
    void resize(UINT w,UINT h){if(!w||!h)return;ID3D11ShaderResourceView*nullViews[5]{};context_->PSSetShaderResources(0,5,nullViews);context_->OMSetRenderTargets(0,nullptr,nullptr);if(d2dContext_)d2dContext_->SetTarget(nullptr);d2dTarget_.Reset();backBuffer_.Reset();rtv_.Reset();sceneRtv_.Reset();sceneSrv_.Reset();sceneTexture_.Reset();bloomRtv_.Reset();bloomSrv_.Reset();bloomTexture_.Reset();morphSourceTexture_.Reset();morphSourceRtv_.Reset();morphSourceSrv_.Reset();morphTexture_.Reset();morphRtv_.Reset();morphSrv_.Reset();if(FAILED(swap_->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0))||FAILED(swap_->GetBuffer(0,IID_PPV_ARGS(&backBuffer_)))||FAILED(device_->CreateRenderTargetView(backBuffer_.Get(),nullptr,&rtv_))){StartupLog("Swap chain resize failed");return;}
        auto createTarget=[&](UINT tw,UINT th,ComPtr<ID3D11Texture2D>&texture,ComPtr<ID3D11RenderTargetView>&target,ComPtr<ID3D11ShaderResourceView>&view){D3D11_TEXTURE2D_DESC td{};td.Width=tw;td.Height=th;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;return SUCCEEDED(device_->CreateTexture2D(&td,nullptr,&texture))&&SUCCEEDED(device_->CreateRenderTargetView(texture.Get(),nullptr,&target))&&SUCCEEDED(device_->CreateShaderResourceView(texture.Get(),nullptr,&view));};
        float scale=renderScale();if(settings_.quality==3)scale=std::min(scale,std::min(3840.f/float(w),2160.f/float(h)));renderWidth_=std::max(1u,(UINT)std::round(w*scale));renderHeight_=std::max(1u,(UINT)std::round(h*scale));if(!createTarget(renderWidth_,renderHeight_,morphSourceTexture_,morphSourceRtv_,morphSourceSrv_)||!createTarget(renderWidth_,renderHeight_,morphTexture_,morphRtv_,morphSrv_)||!createTarget(renderWidth_,renderHeight_,sceneTexture_,sceneRtv_,sceneSrv_)||!createTarget(std::max(1u,renderWidth_/2),std::max(1u,renderHeight_/2),bloomTexture_,bloomRtv_,bloomSrv_))StartupLog("HDR render target creation failed");width_=w;height_=h;if(!createOverlayTarget())StartupLog("Show control compositor target creation failed");}
    void uploadConstants(const Constants&values){D3D11_MAPPED_SUBRESOURCE map{};if(SUCCEEDED(context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map))){memcpy(map.pData,&values,sizeof(values));context_->Unmap(constants_.Get(),0);}}
    void drawScene(Constants c){
        D3D11_VIEWPORT sceneViewport{0,0,float(renderWidth_),float(renderHeight_),0,1};
        D3D11_VIEWPORT bloomViewport{0,0,float(std::max(1u,renderWidth_/2)),float(std::max(1u,renderHeight_/2)),0,1};
        D3D11_VIEWPORT outputViewport{0,0,float(width_),float(height_),0,1};
        beginGpuTimer();context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetConstantBuffers(0,1,constants_.GetAddressOf());context_->PSSetSamplers(0,1,logoSampler_.GetAddressOf());
        context_->RSSetViewports(1,&sceneViewport);
        context_->PSSetShaderResources(0,1,logoSrv_.GetAddressOf());context_->PSSetShaderResources(3,1,identitySrv_.GetAddressOf());
        auto worldPass=[&](Constants frame,ID3D11RenderTargetView*target){uploadConstants(frame);context_->OMSetRenderTargets(1,&target,nullptr);context_->PSSetShader(scenePs_.Get(),nullptr,0);context_->Draw(3,0);};
        ID3D11ShaderResourceView*finishedScene=sceneSrv_.Get();
        const bool mixed=c.scene!=c.previousScene&&c.transition>0&&c.transition<1&&!(surfaceWall(int(c.scene))&&surfaceWall(int(c.previousScene)));
        if(mixed){
            const float phase=liquidProgress(c.transition);
            Constants outgoing=c;outgoing.scene=outgoing.previousScene=c.previousScene;outgoing.patternSeed=c.previousPatternSeed;outgoing.transition=1;outgoing.morphPad0=surfaceWall(int(c.previousScene))?phase:0;
            worldPass(outgoing,morphSourceRtv_.Get());
            Constants incoming=c;incoming.previousScene=c.scene;incoming.transition=1;incoming.morphPad0=surfaceWall(int(c.scene))?1-phase:0;
            worldPass(incoming,sceneRtv_.Get());
            uploadConstants(c);context_->OMSetRenderTargets(1,morphRtv_.GetAddressOf(),nullptr);context_->PSSetShader(morphPs_.Get(),nullptr,0);
            context_->PSSetShaderResources(1,1,sceneSrv_.GetAddressOf());context_->PSSetShaderResources(4,1,morphSourceSrv_.GetAddressOf());context_->Draw(3,0);
            ID3D11ShaderResourceView*empty=nullptr;context_->PSSetShaderResources(1,1,&empty);context_->PSSetShaderResources(4,1,&empty);
            finishedScene=morphSrv_.Get();
        }else{
            Constants frame=c;
            if(c.transition<=0&&c.scene!=c.previousScene){frame.scene=frame.previousScene=c.previousScene;frame.patternSeed=c.previousPatternSeed;frame.transition=1;}
            worldPass(frame,sceneRtv_.Get());
        }
        uploadConstants(c);context_->RSSetViewports(1,&bloomViewport);context_->OMSetRenderTargets(1,bloomRtv_.GetAddressOf(),nullptr);
        context_->PSSetShader(bloomPs_.Get(),nullptr,0);context_->PSSetShaderResources(1,1,&finishedScene);context_->Draw(3,0);
        c.resolution[0]=float(width_);c.resolution[1]=float(height_);uploadConstants(c);
        ID3D11ShaderResourceView*postViews[]{finishedScene,bloomSrv_.Get()};context_->RSSetViewports(1,&outputViewport);context_->OMSetRenderTargets(1,rtv_.GetAddressOf(),nullptr);
        context_->PSSetShader(compositePs_.Get(),nullptr,0);context_->PSSetShaderResources(1,2,postViews);context_->Draw(3,0);
        if(blackout_){const float black[4]{};context_->ClearRenderTargetView(rtv_.Get(),black);}
        ID3D11ShaderResourceView*nullViews[5]{};context_->PSSetShaderResources(0,5,nullViews);context_->OMSetRenderTargets(0,nullptr,nullptr);endGpuTimer();
    }
    void render(){if(displaysDirty_)refreshDisplays(true);if(outputsDirty_){displayOutputStatus_=outputs_.configure(displays_,settings_.mirrorMask,settings_.monitor)?1:-1;outputsDirty_=false;}if(!rtv_||!sceneRtv_||!bloomRtv_||!morphRtv_||!morphSourceRtv_){Sleep(10);return;}if(identityTextureName_!=settings_.djName&&!rebuildIdentityTexture())StartupLog("DJ identity mask refresh failed");pollGpuTimers();float seconds=elapsed();serviceQueuedWorld();auto raw=audio_.frame();auto af=tuned(raw);float dt=lastRenderSeconds_>0?std::clamp(seconds-lastRenderSeconds_,0.f,.10f):0.f;lastRenderSeconds_=seconds;bool reliableTempo=af.bpm>=65.f&&af.bpm<=190.f&&af.tempoConfidence>.22f;const float distance128=std::abs(af.bpm-128.f),distance130=std::abs(af.bpm-130.f);if(settings_.tempoAnchor==1)effectiveTempoAnchor_=128;else if(settings_.tempoAnchor==2)effectiveTempoAnchor_=130;else if(reliableTempo&&std::min(distance128,distance130)<4.5f)effectiveTempoAnchor_=distance130<distance128?130:128;if(reliableTempo){stableBpm_+=(af.bpm-stableBpm_)*std::clamp(dt*2.4f,0.f,1.f);lastReliableTempoAt_=seconds;tempoRecoveryConfidence_+=(1.f-tempoRecoveryConfidence_)*std::clamp(dt*2.8f,0.f,1.f);}else tempoRecoveryConfidence_*=std::exp(-dt*.055f);float tempoAge=std::max(0.f,seconds-lastReliableTempoAt_);float fallbackMix=std::clamp((tempoAge-5.f)/14.f,0.f,1.f);const bool fixedAnchor=settings_.tempoAnchor!=0;const bool commonTempo=reliableTempo&&std::min(distance128,distance130)<4.5f;float measuredTarget=fixedAnchor?float(effectiveTempoAnchor_):(commonTempo?std::lerp(af.bpm,float(effectiveTempoAnchor_),.68f):af.bpm);float targetBpm=reliableTempo?measuredTarget:std::lerp(stableBpm_,float(effectiveTempoAnchor_),fallbackMix);visualBpm_+=(targetBpm-visualBpm_)*std::clamp(dt*(.52f+std::max(af.tempoConfidence,tempoRecoveryConfidence_)*2.05f),0.f,1.f);bool beatNow=(af.transientClass==1&&af.kickEvidence>af.detectorThreshold)||af.kick>.30f||(af.beat>.32f&&af.flux>.08f);if(beatNow&&!beatLatched_){float nearest=std::round(tempoPhase_);float error=std::clamp(nearest-tempoPhase_,-.28f,.28f);phaseCorrection_=phaseCorrection_*.30f+error*.70f;++beatOrdinal_;}beatLatched_=beatNow;float baseAdvance=dt*visualBpm_/60.f;float maxSlew=baseAdvance*.30f;float slew=std::clamp(phaseCorrection_*std::clamp(dt*6.2f,0.f,1.f),-maxSlew,maxSlew);tempoPhase_=tempoPhase_+baseAdvance+slew;phaseCorrection_-=slew;if(std::abs(phaseCorrection_)<.0001f)phaseCorrection_=0;const bool followerMode=settings_.syncMode==2;receiveStageSync(seconds);if(!followerMode){updateSongSection(raw,dt,seconds);updateDirectorPrediction(raw,dt,seconds);updateDirectorDiagnostics(raw,seconds);}else{predictionArmed_=false;preparedWorld_=-1;directorReason_=syncConnected_?L"NETWORK FOLLOWER / LEADER CONTROLS SHOW":L"NETWORK FOLLOWER / HOLDING LAST STATE";}float paletteTarget=float((automatic_&&!settings_.timeline)||followerMode?directorPalette_:settings_.palette);float paletteDelta=paletteTarget-visualPalette_;if(paletteDelta>4)paletteDelta-=8;if(paletteDelta<-4)paletteDelta+=8;visualPalette_+=paletteDelta*std::clamp(dt*.72f,0.f,1.f);if(visualPalette_<0)visualPalette_+=8;if(visualPalette_>=8)visualPalette_-=8;
        cornerMotion_.update(seconds,world_==5&&predictionArmed_&&predictedSection_==DropSection&&predictedSectionConfidence_>.66f,predictionLeadBeats_*60.f/visualBpm_,dropCount_);
        servicePixelExit(raw,seconds);
        if(!followerMode)updateTimedDirector(raw,seconds);publishStageSync(seconds);
        updateWorldProgression();float transition=activeTransitionSeconds_>0?std::clamp((seconds-transitionStarted_)/activeTransitionSeconds_,0.f,1.f):1.f;float energy=directorEnergy(af);float sectionProgress=std::clamp((tempoPhase_-sectionBeatOrigin_)/32.f,0.f,1.f);publishControlState(af,energy,transition,seconds);
        Constants c{{float(renderWidth_),float(renderHeight_)},seconds,float(world_),af.bass,af.mid,af.high,af.level,af.percussion,af.snare,af.beat,af.flux,float(previousWorld_),transition,settings_.logoScale,visualPalette_,float(activeTransitionStyle_),tempoPhase_,af.tempoConfidence,energy,float(songSection_),sectionProgress,dropPulse_,af.onset,af.kick,float(beatOrdinal_),std::fmod(tempoPhase_,4.f)*.25f,std::fmod(tempoPhase_,8.f)*.125f,endingConfidence_,directorConfidence_,1.f,masteredCueDensity(),float(settings_.masteringProfile),float(settings_.stageLayout),settings_.masterBrightness,float(settings_.productionDuty),float(worldMovement_),movementProgress_,patternSeed_,musicalNovelty_,choreographyPhase_,identityWeight_,identityKind_,stanzaProgress_,previousPatternSeed_,0,float(identityEntrance_),float(identityExit_)};
        if(transition<1){c.identityWeight=handoffIdentityWeight_*(1-liquidProgress(transition));c.identityKind=handoffIdentityKind_;c.morphPad1=c.morphPad2=float(handoffIdentityStyle_);}
        c.dvdX=cornerMotion_.x;c.dvdY=cornerMotion_.y;c.dvdCorner=cornerMotion_.impact(seconds);c.dvdControlled=1;
        drawScene(c);serviceCapture();sendSpout();if(!outputs_.present(backBuffer_.Get()))displayOutputStatus_=-1;drawOverlay();swap_->Present(settings_.vsync?1:0,0);
        ++frames_;if(seconds-lastHud_>.10f){lastHud_=seconds;updateHud(raw);}if(seconds-lastTitle_>1){lastFps_=frames_/(seconds-lastTitle_);frames_=0;lastTitle_=seconds;std::wstring title=L"RareStage  |  "+std::wstring(worldName(world_))+(automatic_?L" [AUTO / "+std::wstring(sectionName(songSection_))+L"]":L" [MANUAL]")+L"  |  "+presetName(presetIndex_)+L"  |  "+audio_.name()+L"  |  "+(af.bpm>0?std::to_wstring((int)af.bpm)+L" BPM  |  ":L"")+std::to_wstring((int)lastFps_)+L" FPS";SetWindowTextW(hwnd_,title.c_str());}}
    void fullscreen(){
        if(displays_.empty())refreshDisplays(false);if(displays_.empty())return;
        if(!fullscreen_){windowPlacement_.length=sizeof(windowPlacement_);GetWindowPlacement(hwnd_,&windowPlacement_);windowStyle_=GetWindowLongPtrW(hwnd_,GWL_STYLE);fullscreen_=true;SetWindowLongPtrW(hwnd_,GWL_STYLE,WS_POPUP|(validationMode_?0:WS_VISIBLE));placeFullscreen();}
        else{fullscreen_=false;SetWindowLongPtrW(hwnd_,GWL_STYLE,windowStyle_);SetWindowPlacement(hwnd_,&windowPlacement_);SetWindowPos(hwnd_,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
    }
    static constexpr int SettingCount=16;static constexpr float HudWidth=420.f,HudTop=18.f,HudRowsTop=74.f,HudRowHeight=28.f,HudButtonLeft=248.f,HudButtonRight=376.f,HudButtonSize=24.f;
    int queuedWorld_=-1;bool queuedDirected_=false;
    DisplayOutputs outputs_;std::vector<StageDisplay> displays_;bool displaysDirty_{},outputsDirty_{true};int displayOutputStatus_{};
    WINDOWPLACEMENT windowPlacement_{sizeof(WINDOWPLACEMENT)};LONG_PTR windowStyle_{WS_OVERLAPPEDWINDOW};
    CornerChoreography cornerMotion_;float worldCueBeat_{-1};
    int pixelExitWorld_{-1};bool pixelExitDirected_{},pixelExitCommit_{};
    float queuedMotionSeconds_{-1};int queuedMotionStyle_{};
    float previousPatternSeed_{},handoffIdentityWeight_{},handoffIdentityKind_{};
    bool validationMode_=false;std::vector<float> validationGpuSamples_;bool collectValidationGpu_=false;
    int identityCycle_=-1;bool identityStanza_=true;
    int visitPattern_{};rarestage::show::SetMemory setMemory_;
    int identityEntrance_{},identityExit_{},handoffIdentityStyle_{};
    rarestage::show::Director showDirector_;rarestage::show::Plan route_;int routeSection_=-1,routeCategory_=-1,routePhrase_=-1;
    HWND hwnd_{};UINT width_=1280,height_=720,renderWidth_=1280,renderHeight_=720;bool fullscreen_=false,automatic_=true,frozen_=false,hudVisible_=true,blackout_=false,worldChangeArmed_=false,beatLatched_=false,sectionChangePending_=false,heardActiveMusic_=false,captureRequested_=false,referenceCaptureMode_=false,gpuTimerActive_=false,rehearsalActive_=false,predictionArmed_=false,syncConnected_=false;RECT windowRect_{};int world_=0,previousWorld_=0,hudSelection_=0,autoSequence_=0,timelineStep_=0,presetIndex_=0,monitorCount_=1,songSection_=OpenSection,sectionCandidate_=OpenSection,sectionRevision_=0,lastDirectorPhrase_=-1,directorPalette_=0,activeTransitionStyle_=0,beatOrdinal_=0,directorCandidateWorld_=0,directorRunnerUpWorld_=0,predictedSection_=OpenSection,preparedWorld_=-1,preparedTargetSection_=-1,preparedSectionRevision_=0,worldMovement_=0,movementRevision_=-1,patternFrom_=0,patternTo_=0,captureStatus_=0,referenceQuality_=0,referenceWorld_=0,referenceWarmup_=0,effectiveTempoAnchor_=128,loopPeriod_=64;std::array<int,6>recentWorlds_{{-1,-1,-1,-1,-1,-1}};std::array<std::array<std::uint16_t,SongSectionCount>,WorldCount>worldActUse_{};std::array<GpuTimerSlot,8>gpuTimers_{};std::size_t gpuTimerWrite_{};std::uint32_t captureCount_{},rehearsalRevision_{},setMemoryRevision_{},directorDecisionRevision_{},acceptedDirectorCueCount_{},rejectedDirectorCueCount_{},syncSequence_{},visitSeed_{},dropCount_{},internalLoopRevision_{};StageSync sync_;AudioEngine audio_;ShowSettings settings_;AudioFrame hudFrame_{};std::wstring hudDevice_=L"Starting audio capture...";std::wstring directorReason_=L"STARTUP / ESTABLISH WORLD",previousDirectorReason_,lastCapturePath_,lastDirectorDecision_=L"DIRECTOR STARTUP",lastRejectedCue_,syncStatus_=L"LOCAL / STANDALONE";
    Clock::time_point started_;float lastTitle_{},lastHud_{},lastRenderSeconds_{},lastControlPublish_{},lastFps_{},transitionStarted_{-100},nextWorldAt_{72},worldStartedAt_{},worldChangeReadyAt_{72},visualBpm_{120},stableBpm_{120},tempoPhase_{},phaseCorrection_{},visualPalette_{},activeTransitionSeconds_{1.4f},sectionShortEnergy_{},sectionPhraseEnergy_{},sectionLongEnergy_{},sectionCandidateSeconds_{},sectionStartedAt_{},sectionBeatOrigin_{},worldBeatOrigin_{},sectionLockUntil_{},dropPulse_{},lastDropAt_{-100},manualSectionOverrideUntil_{},endingConfidence_{},lowEnergySeconds_{},sectionEvidence_{},buildTension_{},dropReadiness_{},musicalNovelty_{},songArc_{},musicStartedAt_{-1},movementProgress_{},patternBlend_{},patternSeed_{},loopPhase_{},transitionCompatibility_{1},choreographyPhase_{},identityWeight_{},identityKind_{},stanzaProgress_{},directorCandidateScore_{},directorRunnerUpScore_{},directorConfidence_{},lastDirectorDiagnosticAt_{},gpuFrameMs_{},gpuPeakMs_{},rehearsalStartedAt_{},predictedSectionConfidence_{},predictionLeadBeats_{},adaptiveCueDensity_{1.f},tempoRecoveryConfidence_{},lastReliableTempoAt_{},preparedWorldAt_{},lastSyncPacketAt_{},lastSyncBroadcastAt_{},syncPhaseErrorMs_{},syncPacketAgeMs_{};int frames_{};HANDLE controlMapping_{};rarestage::control::SharedState*controlState_{};std::uint64_t controlRevision_{};std::uint32_t lastCommandSerial_{};rarestage::control::CommandStatus lastCommandStatus_{rarestage::control::CommandStatus::None};ComPtr<ID3D11Device>device_;ComPtr<ID3D11DeviceContext>context_;ComPtr<IDXGISwapChain>swap_;ComPtr<ID3D11RenderTargetView>rtv_,sceneRtv_,bloomRtv_,morphSourceRtv_,morphRtv_;ComPtr<ID3D11Texture2D>backBuffer_,sceneTexture_,bloomTexture_,morphSourceTexture_,morphTexture_;ComPtr<ID3D11ShaderResourceView>sceneSrv_,bloomSrv_,logoSrv_,identitySrv_,morphSourceSrv_,morphSrv_;ComPtr<ID3D11VertexShader>vs_;ComPtr<ID3D11PixelShader>scenePs_,bloomPs_,compositePs_,morphPs_;ComPtr<ID3D11Buffer>constants_;ComPtr<ID3D11SamplerState>logoSampler_;std::wstring identityTextureName_;
    ComPtr<ID2D1Factory1>d2dFactory_;ComPtr<ID2D1Device>d2dDevice_;ComPtr<ID2D1DeviceContext>d2dContext_;ComPtr<ID2D1Bitmap1>d2dTarget_;ComPtr<IDWriteFactory>dwriteFactory_;ComPtr<IDWriteTextFormat>hudFormat_,valueFormat_,titleFormat_,smallFormat_,centerFormat_;ComPtr<ID2D1SolidColorBrush>panelBrush_,accentBrush_,textBrush_,mutedBrush_,selectionBrush_,meterBrush_;
#ifdef RARESTAGE_SPOUT
    spoutDX spout_;bool spoutReady_=false;
#endif
};

static bool MotionTimingSelfTest() {
    std::ofstream capture("RareStage-motion-reference.csv", std::ios::binary);
    capture << "refresh_hz,frame,seconds,phase,liquid_phrase_mix,vault_depth\n";
    constexpr std::array<int,3> RefreshRates{60,120,144};
    float referenceFinal = -1.0f;
    const auto smoothStep=[](float edge0,float edge1,float value){float v=std::clamp((value-edge0)/(edge1-edge0),0.f,1.f);return v*v*(3-2*v);};
    const auto liquid=[](float value){float v=std::clamp(value,0.f,1.f);return v*v*v*(v*(v*6-15)+10);};
    for (const int refresh : RefreshRates) {
        const float dt = 1.0f / static_cast<float>(refresh);
        const int frames = refresh * 600;
        float phase = 0.0f, correction = 0.0f, previousMix = 0.0f, previousPosition = 0.0f;
        for (int frame = 0; frame <= frames; ++frame) {
            const float seconds = frame * dt;
            if (frame > 0 && frame % (refresh / 2) == 0) {
                const float phaseError = std::round(phase) - phase;
                correction += (phaseError - correction) * std::min(1.0f, dt * 9.0f);
            }
            const float velocity = 2.0f + std::clamp(correction * 4.0f, -0.32f, 0.32f);
            const float previousPhase = phase;
            if (frame > 0) phase += velocity * dt;
            correction *= std::exp(-dt * 4.0f);
            const float phrasePosition = std::fmod(phase * .125f, 1.0f);
            const float phraseMix = liquid(smoothStep(.08f,.94f,phrasePosition));
            const float vaultJourney = phase * .03125f;
            const float vaultDepth = liquid(1-std::abs(std::fmod(vaultJourney,1.f)*2-1));
            const bool phraseWrapped = phrasePosition < previousPosition;
            if (frame > 0 && (phase <= previousPhase || phase - previousPhase > .045f ||
                              (!phraseWrapped && std::abs(phraseMix - previousMix) > .022f))) return false;
            previousMix = phraseMix;
            previousPosition = phrasePosition;
            if (frame % std::max(1, refresh * 10) == 0) {
                capture << refresh << ',' << frame << ',' << seconds << ',' << phase << ',' << phraseMix << ',' << vaultDepth << '\n';
            }
        }
        if (referenceFinal < 0) referenceFinal = phase;
        else if (std::abs(phase - referenceFinal) > .035f) return false;
    }
    std::ofstream worlds("RareStage-world-reference.csv",std::ios::binary);
    worlds << "world,act,quality,render_scale,formation_beats,cue_families,cue_density,motion_contract\n";
    constexpr std::array<const char*,21> WorldNames{"Morph Wall","Prism Atrium","Orbital Garden","Stagewall","Resonance Range","Pixel Bank","Prism Cascade","Aurora Range","Chroma Blades","Kinetic Array","Kinetic Canopy","Pixel Loom","Matrix Vault","Panel Cascade","Signal Rail","Kinetic Facade","Pulse Causeway","Void Gates","Ribbon Foundry","Halo Engine","Vector Speedway"};
    constexpr std::array<int,21> CueFamilies{6,5,6,16,6,4,5,6,8,14,7,8,4,6,6,8,7,5,8,8,8};
    constexpr std::array<float,4> RenderScales{.62f,.78f,1.f,1.25f};
    constexpr std::array<const char*,5> Acts{"Open","Build","Drop","Breakdown","Drive"};
    for(size_t world=0;world<WorldNames.size();++world)for(size_t act=0;act<Acts.size();++act)for(size_t quality=0;quality<RenderScales.size();++quality){
        const int formationBeats=world==3?16:world==5||world==12?16:8;
        const float density=MasteredCueDensity(static_cast<int>(world),static_cast<int>(act),1.f);
        if(density<.55f||density>1.35f)return false;
        worlds << WorldNames[world] << ',' << Acts[act] << ',' << quality << ',' << RenderScales[quality] << ','
               << formationBeats << ',' << CueFamilies[world] << ',' << density << ",continuous_base+seeded_movement+phrase_formation+transient_light\n";
    }
    return capture.good()&&worlds.good();
}

static bool DirectorTwoSelfTest(){
    std::ofstream trace("RareStage-director2-reference.csv",std::ios::binary);
    trace<<"frame,seconds,energy,short_energy,long_energy,drop_evidence,predicted_drop,adaptive_density,tempo_bpm\n";
    constexpr float dt=1.f/120.f;float shortEnergy=.16f,longEnergy=.16f,density=.88f,stableBpm=128.f,tempoBpm=128.f;float forecastAt=-1.f,heldAtTen=0.f,afterFallback=0.f;
    for(int frame=0;frame<=3600;++frame){
        const float seconds=frame*dt;float energy=.16f;
        if(seconds>1.f&&seconds<3.f)energy=std::lerp(.16f,.66f,(seconds-1.f)*.5f);else if(seconds>=3.f&&seconds<8.f)energy=.66f;else if(seconds>=8.f)energy=.12f;
        shortEnergy+=(energy-shortEnergy)*std::clamp(dt*5.8f,0.f,1.f);longEnergy+=(energy-longEnergy)*std::clamp(dt*.11f,0.f,1.f);
        const float rise=shortEnergy-longEnergy,flux=seconds>1.f&&seconds<3.f?.32f:.04f,mid=seconds>1.f&&seconds<3.f?.45f:.18f,bass=energy*.62f;
        const float evidence=std::clamp(std::max(0.f,rise)*4.2f+flux*.62f+mid*.20f+bass*.10f+.18f,0.f,1.f);const bool predicted=evidence>.66f&&seconds<3.f;if(predicted&&forecastAt<0)forecastAt=seconds;
        float target=seconds<1.f?.88f:seconds<3.f?std::lerp(1.02f,1.12f,evidence):seconds<8.f?1.08f:.72f;const float rate=target>density?1.10f:.52f;density+=(target-density)*std::clamp(dt*rate,0.f,1.f);
        const bool reliableTempo=seconds<1.f;const float tempoAge=std::max(0.f,seconds-1.f);const float fallbackMix=reliableTempo?0.f:std::clamp((tempoAge-5.f)/14.f,0.f,1.f);const float targetBpm=reliableTempo?128.f:std::lerp(stableBpm,128.f,fallbackMix);tempoBpm+=(targetBpm-tempoBpm)*std::clamp(dt*.72f,0.f,1.f);if(std::abs(seconds-10.f)<dt)heldAtTen=tempoBpm;if(std::abs(seconds-29.f)<dt)afterFallback=tempoBpm;
        if(frame%12==0)trace<<frame<<','<<seconds<<','<<energy<<','<<shortEnergy<<','<<longEnergy<<','<<evidence<<','<<(predicted?1:0)<<','<<density<<','<<tempoBpm<<'\n';
        if(density<.70f||density>1.14f||(frame>0&&tempoBpm<=0))return false;
    }
    std::array<std::uint16_t,18> actUse{};actUse.fill(3);actUse[9]=0;actUse[13]=1;int selected=0;float best=-1000.f;const auto minimum=*std::min_element(actUse.begin(),actUse.end());for(int world=0;world<18;++world){float score=1.f-float(actUse[world]-minimum)*.28f;if(score>best){best=score;selected=world;}}
    return trace.good()&&forecastAt>1.f&&forecastAt<3.f&&std::abs(heldAtTen-128.f)<.2f&&std::abs(afterFallback-128.f)<.2f&&selected==9;
}

static bool ProductionPackSelfTest(){
    std::ofstream contract("RareStage-production-pack-reference.csv",std::ios::binary);
    contract<<"duty,mastering,layout,brightness,cue_density,dwell_seconds,transition_seconds,visible_area,peak_limit,sync_packet_version\n";
    constexpr std::array<const char*,5>Duties{"Open","Support","Peak","Breakdown","Closing"};
    constexpr std::array<const char*,4>Masters{"Studio","LED Safe","Arena","Broadcast"};
    constexpr std::array<const char*,4>Layouts{"Full Frame","Ultrawide","Triptych","LED Facade"};
    constexpr std::array<float,5>Brightness{.84f,.88f,.98f,.82f,.80f};
    constexpr std::array<float,5>Density{.76f,.90f,1.14f,.72f,.80f};
    constexpr std::array<float,5>Dwell{96,84,56,104,112};
    constexpr std::array<float,5>Transition{1.8f,1.55f,1.05f,2.2f,2.4f};
    constexpr std::array<float,4>VisibleArea{1.f,.744f,.956f,.665f};
    constexpr std::array<float,4>PeakLimit{1.f,.86f,1.f,.9227f};
    for(std::size_t duty=0;duty<Duties.size();++duty)for(std::size_t master=0;master<Masters.size();++master)for(std::size_t layout=0;layout<Layouts.size();++layout){
        if(Brightness[duty]<.65f||Brightness[duty]>1.f||Density[duty]<.65f||Density[duty]>1.25f||VisibleArea[layout]<.5f||VisibleArea[layout]>1.f)return false;
        contract<<Duties[duty]<<','<<Masters[master]<<','<<Layouts[layout]<<','<<Brightness[duty]<<','<<Density[duty]<<','<<Dwell[duty]<<','<<Transition[duty]<<','<<VisibleArea[layout]<<','<<PeakLimit[master]<<",1\n";
    }
    StageSyncPacket source{};source.sequence=77;source.groupId=42;source.world=9;source.section=2;source.palette=3;source.productionDuty=2;source.masteringProfile=1;source.stageLayout=2;source.reactionProfile=2;source.transitionStyle=3;source.tempoPhase=31.5f;source.bpm=128;source.cueDensity=1.14f;source.masterBrightness=.88f;source.transitionSeconds=1.05f;std::array<std::byte,sizeof(StageSyncPacket)>bytes{};std::memcpy(bytes.data(),&source,sizeof(source));StageSyncPacket copy{};std::memcpy(&copy,bytes.data(),sizeof(copy));
    StageSyncPacket invalid=copy;invalid.masterBrightness=std::nanf("");
    return contract.good()&&ValidStageSyncPacket(copy,42)&&!ValidStageSyncPacket(copy,41)&&!ValidStageSyncPacket(invalid,42)&&copy.sequence==77&&copy.groupId==42&&copy.world==9&&copy.section==2&&copy.reactionProfile==2&&copy.transitionStyle==3&&copy.tempoPhase==31.5f&&copy.masterBrightness==.88f&&copy.transitionSeconds==1.05f;
}

static bool ClubLoopSelfTest(){
    const auto liquid=[](float value){value=std::clamp(value,0.f,1.f);return value*value*value*(value*(value*6.f-15.f)+10.f);};
    constexpr std::array<int,18> Families{0,3,3,0,1,0,1,3,1,0,0,0,2,0,2,0,3,2};
    constexpr std::array<const char*,4> CategoryNames{"PanelPixel","RibbonGlass","DepthTransport","Atmosphere"};
    constexpr std::array<const char*,18> SceneScopes{"stage_system","environment","environment","stage_system","stage_system","stage_system","stage_system","environment","stage_system","stage_system","stage_system","stage_system","depth_world","stage_system","depth_world","stage_system","environment","depth_world"};
    std::ofstream contract("RareStage-club-loop-reference.csv",std::ios::binary);
    contract<<"world,category,loop_beats,pattern_banks,tempo_anchor,autonomous_motion,identity_mode,scene_scope,choreography\n";
    int environmentCount=0;
    for(int world=0;world<18;++world){
        const int family=Families[world];const int beats=family==1||family==3?96:64;
        const char*identity=family==0?"gpu_mask_formation":family==1?"world_carrier":family==2?"architectural_mask":"environmental_mask";
        if(std::string_view(SceneScopes[world])=="environment")++environmentCount;
        constexpr std::array<int,2> Anchors{128,130};
        for(const int anchor:Anchors)contract<<world+1<<','<<CategoryNames[family]<<','<<beats<<','<<16<<','<<anchor<<",continuous,"<<identity<<','<<SceneScopes[world]<<",establish>roll>resolve>release\n";
        if(beats!=64&&beats!=96)return false;
    }
    const std::wstring sanitized=SanitizeDjName(L"  RARE CLUB  ");
    const std::wstring bounded=SanitizeDjName(std::wstring(80,L'X'));
    std::ofstream choreography("RareStage-choreography-reference.csv",std::ios::binary);choreography<<"refresh_hz,frame,stanza_progress,formation_weight,stage\n";for(const int refresh:std::array<int,3>{60,120,144}){const float duration=16.f*60.f/130.f;const int frames=int(std::ceil(duration*refresh));float previous=0,peak=0;for(int frame=0;frame<=frames;++frame){const float stanza=std::min(1.f,frame/float(frames));const float assemble=liquid(std::clamp((stanza-.16f)/.30f,0.f,1.f));const float release=1.f-liquid(std::clamp((stanza-.72f)/.24f,0.f,1.f));const float weight=assemble*release;const int stage=stanza<.16f?0:stanza<.46f?1:stanza<.72f?2:3;if(frame>0&&std::abs(weight-previous)>.018f)return false;previous=weight;peak=std::max(peak,weight);if(frame%(refresh/12)==0||frame==frames)choreography<<refresh<<','<<frame<<','<<stanza<<','<<weight<<','<<stage<<'\n';}if(previous>.001f||peak<.98f)return false;}
    return contract.good()&&choreography.good()&&environmentCount>=4&&sanitized==L"RARE CLUB"&&bounded.size()==24;
}

int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int){
    SetUnhandledExceptionFilter(CrashLog);
    {std::ofstream clear(StartupLogPath());clear << "RareStage startup beginning\n";}
    if(std::wcsstr(GetCommandLineW(),L"--shader-cache-test"))return ShaderCacheSelfTest()?0:23;
    if(std::wcsstr(GetCommandLineW(),L"--performance-self-test"))return PerformanceSelfTest()?0:20;
    if(std::wcsstr(GetCommandLineW(),L"--audio-self-test")){
        const bool passed=AudioEngine::classifierSelfTest();
        std::ofstream log("RareStage-audio-self-test.log",std::ios::binary);
        log << (passed?"sample-rate-aware kick/snare/hat classifier passed\n":"kick/snare/hat classifier separation failed\n");
        return passed?0:6;
    }
    if(std::wcsstr(GetCommandLineW(),L"--motion-self-test")){
        return MotionTimingSelfTest()?0:7;
    }
    if(std::wcsstr(GetCommandLineW(),L"--director-self-test")){
        return DirectorTwoSelfTest()?0:8;
    }
    if(std::wcsstr(GetCommandLineW(),L"--production-self-test")){
        return ProductionPackSelfTest()?0:9;
    }
    if(std::wcsstr(GetCommandLineW(),L"--club-loop-self-test")){
        return ClubLoopSelfTest()?0:10;
    }
    if(std::wcsstr(GetCommandLineW(),L"--self-test")||std::wcsstr(GetCommandLineW(),L"--prepare-shaders")){
        ComPtr<ID3DBlob> blob,error;
        if(FAILED(CompileStageShader("vs","vs_5_0",&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 2;}
        blob.Reset();error.Reset();
        if(FAILED(CompileStageShader("ps","ps_5_0",&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 3;}
        blob.Reset();error.Reset();
        if(FAILED(CompileStageShader("psBloom","ps_5_0",&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 4;}
        blob.Reset();error.Reset();
        if(FAILED(CompileStageShader("psComposite","ps_5_0",&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 5;}
        blob.Reset();error.Reset();
        if(FAILED(CompileStageShader("psMorph","ps_5_0",&blob,&error))){if(error){std::ofstream log("RareStage-self-test.log",std::ios::binary);log.write((const char*)error->GetBufferPointer(),error->GetBufferSize());}return 13;}
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);App app;
    if(!app.init(h)){StartupLog("Startup failed");if(SUCCEEDED(com))CoUninitialize();return 1;}
    if(std::wcsstr(GetCommandLineW(),L"--release-validation")){
        std::ofstream log("RareStage-release-validation.log");int result=app.runPerformanceValidation();log<<"performance "<<result<<std::endl;
        if(!result){result=app.runStageLightingValidation();log<<"lighting "<<result<<std::endl;}
        if(!result){result=app.runProfessionalValidation();log<<"professional "<<result<<std::endl;}
        if(!result){result=app.runVisualValidation(false);log<<"artifacts "<<result<<std::endl;}
        if(!result){result=app.runChoreographyValidation(false);log<<"continuity "<<result<<std::endl;}
        if(!result){result=app.runVisualValidation(true);log<<"world_gpu "<<result<<std::endl;}
        if(!result){result=app.runChoreographyValidation(true);log<<"scene_gpu "<<result<<std::endl;}
        app.requestClose();app.run();return result;
    }
    if(std::wcsstr(GetCommandLineW(),L"--performance-test")){const int result=app.runPerformanceValidation();app.requestClose();app.run();return result;}
    if(std::wcsstr(GetCommandLineW(),L"--professional-test")){const int result=app.runProfessionalValidation();app.requestClose();app.run();return result;}
    if(std::wcsstr(GetCommandLineW(),L"--stage-lighting-test")){const int result=app.runStageLightingValidation();app.requestClose();app.run();return result;}
    if(std::wcsstr(GetCommandLineW(),L"--choreography-test")||std::wcsstr(GetCommandLineW(),L"--choreography-benchmark")){const int result=app.runChoreographyValidation(std::wcsstr(GetCommandLineW(),L"--choreography-benchmark")!=nullptr);app.requestClose();app.run();return result;}
    if(std::wcsstr(GetCommandLineW(),L"--visual-regression-test")||std::wcsstr(GetCommandLineW(),L"--benchmark-4k")){const int validationResult=app.runVisualValidation(std::wcsstr(GetCommandLineW(),L"--benchmark-4k")!=nullptr);app.requestClose();app.run();return validationResult;}
    if(std::wcsstr(GetCommandLineW(),L"--reference-capture-test"))app.enableReferenceCaptureTest();
    else if(std::wcsstr(GetCommandLineW(),L"--startup-test"))app.requestClose();
    int result=app.run();StartupLog("RareStage exited normally");if(SUCCEEDED(com))CoUninitialize();return result;
}

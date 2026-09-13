#pragma once
#include "show_sequence.h"
#include <cstdint>
#include <optional>

namespace rarestage::show {
enum class MotionAct {Setup,Develop,Payoff,Departure};
inline constexpr const wchar_t* MotionActNames[]{L"ESTABLISHING",L"DEVELOPING",L"FINISHING THE FORMATION",L"READY TO HAND OVER"};
struct ActionContract {float establish{16},development{8},payoff{4},exitStride{8},exitOffset{};};
inline ActionContract PatternContract(int scene,int program){
 const auto gesture=ProgramGesture(scene,program);ActionContract c;
 if(gesture==Gesture::Turn||gesture==Gesture::Fold)c.development=16;
 if(gesture==Gesture::Gather||gesture==Gesture::Fan)c.payoff=8;
 if(gesture==Gesture::Counterflow)c.exitOffset=4;
 return c;
}
inline float PreferredExit(int scene,int program,float earliest,float establishedAt,float settledAt,float confidence,bool confirmedChapter){
 const auto c=PatternContract(scene,program);
 float ready=std::max({earliest,establishedAt+c.establish,settledAt+c.payoff});
 // Uncertain music permits one longer continuation. Recompute only when arming
 // or invalidating a cue, never every frame (which would move the deadline).
 if(!confirmedChapter&&confidence<.25f)ready+=c.development;
 return std::ceil((ready-c.exitOffset)/c.exitStride)*c.exitStride+c.exitOffset;
}
inline MotionAct CurrentAct(int scene,int program,float beat,float establishedAt,float settledAt,bool departing){
 const auto c=PatternContract(scene,program);
 if(beat<establishedAt+c.establish)return MotionAct::Setup;
 if(beat<settledAt+c.payoff)return MotionAct::Payoff;
 return departing?MotionAct::Departure:MotionAct::Develop;
}
struct Composition {
 float luma{},coverage{},centerX{.5f},centerY{.5f},clipped{};
 std::array<float,16> tiles{};
};
inline Composition MeasureComposition(const std::uint8_t* pixels,unsigned width,unsigned height,unsigned pitch){
 Composition r;float energy=0;unsigned n=0;std::array<unsigned,16>counts{};
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto* p=pixels+size_t(y)*pitch+x*4;
  const float l=(p[2]*.2126f+p[1]*.7152f+p[0]*.0722f)/255.f;
  const int tile=int(y*4/height)*4+int(x*4/width);r.tiles[tile]+=l;++counts[tile];
  r.luma+=l;r.coverage+=l>.06f?1.f:0.f;r.clipped+=std::max({p[0],p[1],p[2]})>=250?1.f:0.f;
  const float weight=std::max(0.f,l-.025f);energy+=weight;r.centerX+=(float(x)+.5f)/width*weight;r.centerY+=(float(y)+.5f)/height*weight;++n;
 }
 if(n){r.luma/=n;r.coverage/=n;r.clipped/=n;}
 if(energy>.001f){r.centerX=(r.centerX-.5f)/energy;r.centerY=(r.centerY-.5f)/energy;}else r.centerX=r.centerY=.5f;
 for(size_t i=0;i<16;++i)if(counts[i])r.tiles[i]/=counts[i];return r;
}
struct CompositionJudgment {float penalty{},continuity{1},minimumCoverage{},darkLoss{};};
inline CompositionJudgment JudgeComposition(const std::array<Composition,5>& frames){
 CompositionJudgment r;r.minimumCoverage=1;float largestLumaChange=0,travel=0,tileTravel=0;
 const float intendedFloor=std::min(frames.front().luma,frames.back().luma);
 const float intendedCoverage=std::min(frames.front().coverage,frames.back().coverage);
 for(size_t i=0;i<5;++i){const auto&f=frames[i];r.minimumCoverage=std::min(r.minimumCoverage,f.coverage);
  // Endpoint-relative intent: a sparse scene or quiet opening is legitimate.
  if(i>0&&i<4){r.darkLoss=std::max(r.darkLoss,std::max(0.f,intendedFloor*.60f-f.luma));r.penalty+=std::max(0.f,intendedCoverage*.45f-f.coverage)*.20f;}
  if(i){const auto&old=frames[i-1];largestLumaChange=std::max(largestLumaChange,std::abs(f.luma-old.luma));
   travel+=std::hypot(f.centerX-old.centerX,f.centerY-old.centerY);
   for(size_t tile=0;tile<16;++tile)tileTravel+=std::abs(f.tiles[tile]-old.tiles[tile])/16;
  }
 }
 float necessaryTravel=std::hypot(frames.back().centerX-frames.front().centerX,frames.back().centerY-frames.front().centerY);
 const float necessaryLuma=std::abs(frames.back().luma-frames.front().luma);
 float necessaryTileTravel=0;for(size_t tile=0;tile<16;++tile)necessaryTileTravel+=std::abs(frames.back().tiles[tile]-frames.front().tiles[tile])/16;
 r.penalty+=r.darkLoss*2+std::max(0.f,largestLumaChange-.10f-necessaryLuma*.6f)*.6f+std::max(0.f,travel-necessaryTravel-.6f)*.08f+std::max(0.f,tileTravel-necessaryTileTravel-.12f)*.25f;
 r.penalty=std::clamp(r.penalty,0.f,.45f);r.continuity=1-r.penalty/.45f;return r;
}
struct RehearsalContext {
 int source{},program{},section{},revision{},category{},palette{},theme{},layout{},width{},height{},brightness{},mastering{},exitBeat{};unsigned visit{};
 bool operator==(const RehearsalContext&)const=default;
};
struct VisualCandidate {int scene{-1},program{};std::array<Composition,5> frames{};unsigned samples{};CompositionJudgment judgment{};};
class VisualRehearsal {
 RehearsalContext context_{};std::array<VisualCandidate,2> candidates_{};unsigned epoch_{};float started_{-1000};bool valid_{};
public:
 unsigned discarded{},completed{},submitted{},budgetSkips{};
 void cancel(){if(valid_){if(!done())++discarded;++epoch_;}valid_=false;}
 unsigned epoch()const{return epoch_;}
 bool valid(const RehearsalContext& c,float beat)const{return valid_&&context_==c&&beat>=started_&&beat-started_<=32;}
 void begin(const RehearsalContext& c,float beat,int first,int second,int firstProgram,int secondProgram){cancel();context_=c;started_=beat;valid_=true;++epoch_;candidates_={VisualCandidate{first,firstProgram},VisualCandidate{second,secondProgram}};}
 const std::array<VisualCandidate,2>& candidates()const{return candidates_;}
 bool done()const{return candidates_[0].samples==5&&candidates_[1].samples==5;}
 std::optional<std::array<int,2>> next()const{if(!valid_)return {};for(int i=0;i<2;++i)if(candidates_[i].samples<5)return std::array<int,2>{i,int(candidates_[i].samples)};return {};}
 bool accept(unsigned epoch,int candidate,int sample,const Composition& image){
  if(!valid_||epoch!=epoch_||candidate<0||candidate>1||sample<0||sample>4||candidates_[candidate].samples!=unsigned(sample))return false;
  auto&c=candidates_[candidate];c.frames[sample]=image;++c.samples;if(c.samples==5){c.judgment=JudgeComposition(c.frames);++completed;}return true;
 }
 float adjustment(int scene,const RehearsalContext& c,float beat)const{if(!valid(c,beat))return 0;for(const auto&candidate:candidates_)if(candidate.scene==scene&&candidate.samples==5)return -candidate.judgment.penalty;return 0;}
 int entry(int scene,const RehearsalContext& c,float beat)const{if(!valid(c,beat))return -1;for(const auto&candidate:candidates_)if(candidate.scene==scene&&candidate.samples==5)return candidate.program;return -1;}
};
inline bool RehearsalBudget(float liveMs,float peakMs,float budgetMs,float lastRehearsalMs,float secondsSinceSubmit){
 return std::isfinite(liveMs+peakMs+budgetMs+lastRehearsalMs)&&liveMs>0&&budgetMs>0&&liveMs<budgetMs*.65f&&peakMs<budgetMs*.85f&&lastRehearsalMs<budgetMs*.08f&&secondsSinceSubmit>=.25f;
}
}

#pragma once
#include "world_spec.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
// Decisions about generated worlds, kept free of the GPU so they are tested offline. A compiled shader is a
// candidate, not a result: it is rendered offscreen at reduced size, timed on the GPU, scaled to the output
// resolution and checked against the budget for the display's refresh rate before the Director may use it.
namespace rarestage::gen {

// Provisional GPU budgets for the generated pass alone, interpolated by refresh rate. They are starting
// points to be tuned against the real frame, not guarantees for any particular GPU.
inline float BudgetForFps(float fps){
 struct P{float fps,ms;};static constexpr P table[]{{60.f,5.0f},{90.f,3.0f},{120.f,2.0f}};
 if(!std::isfinite(fps)||fps<=table[0].fps)return table[0].ms;if(fps>=table[2].fps)return table[2].ms;
 for(int i=0;i<2;++i)if(fps<=table[i+1].fps){const float t=(fps-table[i].fps)/(table[i+1].fps-table[i].fps);return table[i].ms+(table[i+1].ms-table[i].ms)*t;}
 return table[2].ms;
}

enum class Verdict:std::uint8_t{Accept,AcceptHeavy,RejectCost,RejectBlack,RejectBlown,RejectNonFinite,RejectNoTiming};
inline const char* VerdictName(Verdict v){switch(v){case Verdict::Accept:return "accepted";case Verdict::AcceptHeavy:return "accepted (heavy)";case Verdict::RejectCost:return "over GPU budget";case Verdict::RejectBlack:return "renders black";case Verdict::RejectBlown:return "renders blown out";case Verdict::RejectNonFinite:return "non-finite output";default:return "no timing";}}
inline bool Accepted(Verdict v){return v==Verdict::Accept||v==Verdict::AcceptHeavy;}

struct Measurement{
 float avgMs{0},maxMs{0};          // GPU time of the test render
 float testPixels{0},fullPixels{0};
 float meanLuma{0},nonBlack{0};bool finite{true};bool timingValid{true};
};
inline float EstimatedFullMs(const Measurement&m){return m.testPixels>0?m.avgMs*(m.fullPixels/m.testPixels):m.avgMs;}

// Soft limit: usable but expensive, so the generator is asked for a cheaper sibling. Hard limit: rejected.
inline constexpr float SoftFraction=.7f;
inline Verdict Decide(const Measurement&m,float budgetMs){
 if(!m.timingValid||!std::isfinite(m.avgMs)||m.avgMs<0)return Verdict::RejectNoTiming;
 if(!m.finite)return Verdict::RejectNonFinite;
 if(m.meanLuma<.004f||m.nonBlack<.03f)return Verdict::RejectBlack;
 if(m.meanLuma>.97f)return Verdict::RejectBlown;
 const float est=EstimatedFullMs(m);
 if(est>budgetMs)return Verdict::RejectCost;
 return est>budgetMs*SoftFraction?Verdict::AcceptHeavy:Verdict::Accept;
}

// Candidate performance history keyed by shader hash. A composition that keeps failing is blacklisted so the
// generator does not spend GPU time re-qualifying it.
class QualificationHistory{
public:
 struct Entry{std::uint32_t measured{0},rejected{0};float avgMs{0};Verdict last{Verdict::Accept};};
 static constexpr std::uint32_t BlacklistAfter=2;
 void record(std::uint64_t hash,Verdict v,float fullMs){
  Entry&e=map_[hash];e.last=v;if(std::isfinite(fullMs)&&v!=Verdict::RejectNoTiming){e.avgMs=e.measured?e.avgMs+(fullMs-e.avgMs)/float(e.measured+1):fullMs;++e.measured;}
  if(!Accepted(v)&&v!=Verdict::RejectNoTiming)++e.rejected;
 }
 bool blacklisted(std::uint64_t hash)const{const auto it=map_.find(hash);return it!=map_.end()&&it->second.rejected>=BlacklistAfter;}
 const Entry*find(std::uint64_t hash)const{const auto it=map_.find(hash);return it==map_.end()?nullptr:&it->second;}
 size_t size()const{return map_.size();}
 size_t blacklistCount()const{size_t n=0;for(const auto&kv:map_)n+=kv.second.rejected>=BlacklistAfter;return n;}
private:
 std::unordered_map<std::uint64_t,Entry> map_;
};

// A world that was too expensive gets one cheaper sibling (same seed and family, one quality tier down).
inline bool CheaperSibling(const WorldSpec&w,WorldSpec&out){
 if(w.budget.quality==0)return false;out=Generate(w.seed,w.family,int(w.budget.quality)-1);return true;
}
}

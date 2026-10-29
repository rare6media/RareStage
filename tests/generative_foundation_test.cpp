#include "modulation.h"
#include "procedural_palette.h"
#include "generative_clock.h"
#include <cstdio>
#include <string>
using namespace rarestage::show;
static int failures=0;
static void check(const char*name,bool ok){if(!ok){std::printf("FAIL %s\n",name);++failures;}}
static ModSnapshot snapshot(ModSource s,float v,int section=0){ModSnapshot m;m.value[size_t(s)]=v;m.section=section;return m;}
static void settle(ModulationMatrix&m,const ModSnapshot&s,int frames=600,float dt=1.f/60){for(int i=0;i<frames;++i)m.update(s,dt);}
static float shaderValue(int p,float norm){const auto&d=ModDestinations[size_t(p)];return d.lo+(d.hi-d.lo)*norm;}
static float normOf(int p){const auto&d=ModDestinations[size_t(p)];return (d.def-d.lo)/(d.hi-d.lo);}

int main(){
 // Schema is sane: defaults sit inside ranges.
 {bool ok=true;for(const auto&d:ModDestinations)ok&=d.hi>d.lo&&d.def>=d.lo&&d.def<=d.hi;check("schema_defaults_in_range",ok);}

 // Disabled matrix yields neutral values regardless of routes.
 {ModulationMatrix m;ModBank b=CohesivePatch();m.load(b);m.setEnabled(false);ModSnapshot s;s.value.fill(1.f);settle(m,s);
  bool neutral=true;for(int i=0;i<ModParamCount;++i)neutral&=std::abs(m.output()[size_t(i)]-ModDestinations[size_t(i)].def)<1e-5f;check("disabled_is_neutral",neutral);}

 // A single Add route lands on the schema-converted value.
 {ModulationMatrix m;ModBank b;ModRoute r=Route(ModSource::Energy,PBloom,.25f,0,0);b.add(r);m.load(b);m.setEnabled(true);settle(m,snapshot(ModSource::Energy,1.f));
  check("add_route_value",std::abs(m.output()[PBloom]-shaderValue(PBloom,normOf(PBloom)+.25f))<1e-4f);}

 // Combination modes, evaluated in route order.
 {auto run=[](std::initializer_list<ModRoute> routes){ModulationMatrix m;ModBank b;for(const auto&r:routes)b.add(r);m.load(b);m.setEnabled(true);settle(m,snapshot(ModSource::Constant,1.f));return m.output()[PExposure];};
  ModRoute add=Route(ModSource::Constant,PExposure,.5f,0,0),rep=Route(ModSource::Constant,PExposure,.2f,0,0,ModMode::Replace);
  ModRoute mul=Route(ModSource::Constant,PExposure,.5f,0,0,ModMode::Multiply),mx=Route(ModSource::Constant,PExposure,.9f,0,0,ModMode::Maximum);
  check("mode_replace_wins_when_last",std::abs(run({add,rep})-shaderValue(PExposure,.2f))<1e-4f);
  check("mode_add_wins_when_last",std::abs(run({rep,add})-shaderValue(PExposure,.7f))<1e-4f);
  check("mode_multiply",std::abs(run({add,mul})-shaderValue(PExposure,(normOf(PExposure)+.5f)*.5f))<1e-4f);
  check("mode_maximum",std::abs(run({add,mx})-shaderValue(PExposure,.9f))<1e-4f);
  check("order_is_deterministic",run({add,rep})==run({add,rep}));}

 // Attack and release shape the envelope: no jump, monotone rise, monotone fall.
 {ModulationMatrix m;ModBank b;b.add(Route(ModSource::KickOnset,PBloom,.5f,.1f,.4f));m.load(b);m.setEnabled(true);
  settle(m,snapshot(ModSource::KickOnset,0.f),120);const float base=m.output()[PBloom];
  float prev=base;bool rising=true,first=true;float firstStep=0;for(int i=0;i<30;++i){m.update(snapshot(ModSource::KickOnset,1.f),1.f/60);const float v=m.output()[PBloom];rising&=v>=prev-1e-6f;if(first){firstStep=v-base;first=false;}prev=v;}
  const float peak=prev;check("attack_rises_monotonically",rising&&peak>base);
  check("attack_is_not_instant",firstStep<.5f*(peak-base)+1e-6f);
  bool falling=true;for(int i=0;i<60;++i){m.update(snapshot(ModSource::KickOnset,0.f),1.f/60);const float v=m.output()[PBloom];falling&=v<=prev+1e-6f;prev=v;}
  check("release_falls_monotonically",falling&&prev<peak);}

 // Section mask keeps a route silent outside its sections.
 {ModulationMatrix m;ModBank b;b.add(Route(ModSource::Constant,PContrast,.4f,0,0,ModMode::Add,SectionBit(2)));m.load(b);m.setEnabled(true);
  settle(m,snapshot(ModSource::Constant,1.f,0));const float quiet=m.output()[PContrast];settle(m,snapshot(ModSource::Constant,1.f,2));const float active=m.output()[PContrast];
  check("section_mask_silences",std::abs(quiet-ModDestinations[PContrast].def)<1e-4f&&active>quiet+.05f);}

 // Non-finite sources are treated as silence.
 {ModulationMatrix m;m.load(CohesivePatch());m.setEnabled(true);ModSnapshot s;s.value.fill(std::nanf(""));settle(m,s,10);bool finite=true;for(float v:m.output())finite&=std::isfinite(v);check("nan_source_sanitised",finite);}

 // Outputs always stay inside the schema, even for hostile input.
 {ModulationMatrix m;m.load(CohesivePatch());m.setEnabled(true);std::uint32_t seed=7;bool inRange=true;
  for(int f=0;f<2000;++f){ModSnapshot s;for(auto&v:s.value)v=PaletteUnit(seed,std::uint32_t(f*31+&v-&s.value[0]))*3.f-1.f;s.section=f%5;m.update(s,1.f/60);
   for(int i=0;i<ModParamCount;++i){const auto&d=ModDestinations[size_t(i)];inRange&=m.output()[size_t(i)]>=d.lo-1e-4f&&m.output()[size_t(i)]<=d.hi+1e-4f;}}
  check("outputs_within_schema",inRange);}

 // Patch change: the crossfade is continuous and carries envelopes of shared routes.
 {ModulationMatrix m;ModBank a;a.add(Route(ModSource::Energy,PBloom,.4f,.05f,.5f));m.load(a);m.setEnabled(true);settle(m,snapshot(ModSource::Energy,1.f));
  const float before=m.output()[PBloom];ModBank b;b.add(Route(ModSource::Energy,PBloom,.4f,.05f,.5f));b.add(Route(ModSource::LfoSlow,PContrast,.2f,.5f,.5f));
  m.stage(b);m.commit(2.f);m.update(snapshot(ModSource::Energy,1.f),1.f/60);
  check("stage_carries_envelope",std::abs(m.output()[PBloom]-before)<.02f);
  float prev=m.output()[PBloom],maxStep=0;for(int i=0;i<200;++i){m.update(snapshot(ModSource::Energy,1.f),1.f/60);maxStep=std::max(maxStep,std::abs(m.output()[PBloom]-prev));prev=m.output()[PBloom];}
  check("crossfade_is_smooth",maxStep<.02f);check("crossfade_completes",m.crossfade()==1.f&&m.selectedBank()==1);}

 // Disabling fades instead of snapping.
 {ModulationMatrix m;ModBank b;b.add(Route(ModSource::Constant,PBloom,.5f,0,0));m.load(b);m.setEnabled(true);settle(m,snapshot(ModSource::Constant,1.f));
  const float on=m.output()[PBloom];m.setEnabled(false);m.update(snapshot(ModSource::Constant,1.f),1.f/60);const float step=std::abs(m.output()[PBloom]-on);
  check("disable_fades",step<.5f*std::abs(on-ModDestinations[PBloom].def)+1e-6f&&m.output()[PBloom]!=ModDestinations[PBloom].def);}

 // Palette evaluation stays displayable and finite.
 {bool ok=true;for(unsigned seed=1;seed<64;++seed){const auto p=MakePalette(PaletteUnit(seed,1),seed);for(int i=0;i<=64;++i){const auto c=p.eval(i/64.f);for(float v:c)ok&=std::isfinite(v)&&v>=0&&v<=1;}}check("palette_in_gamut",ok);}
 // t is hue: red, green and blue palettes peak in the matching channel, so the ring colour the Director picked is the colour shown.
 {bool ok=true;for(unsigned seed=1;seed<32;++seed){const auto p=MakePalette(1.f,seed);const auto r=p.eval(0.f),g=p.eval(1.f/3),b=p.eval(2.f/3);
   ok&=r[0]>r[1]&&r[0]>r[2]&&g[1]>g[0]&&g[1]>g[2]&&b[2]>b[0]&&b[2]>b[1];}check("palette_t_is_hue",ok);}
 {const auto a=MakePalette(.6f,42),b=MakePalette(.6f,42);check("palette_deterministic",a.d==b.d&&a.c==b.c);}

 // A hue change inside one harmony slides the short way across 0/360 and never visits the far side.
 {ProceduralPalette p;p.setEnabled(true);PaletteSpec s;s.hueDegrees=350;s.section=0;p.retarget(s,2.f);p.update(.016f);
  s.hueDegrees=10;p.retarget(s,2.f);bool shortWay=true;for(int i=0;i<150;++i){p.update(1.f/60);const float t0=p.gpu().b[3];shortWay&=t0>.9f||t0<.1f;}
  check("hue_slide_takes_short_path",shortWay);}

 // A harmony change bridges between evaluated colours: no jump at the retarget, then arrival.
 {ProceduralPalette p;p.setEnabled(true);PaletteSpec s;s.hueDegrees=200;s.section=0;p.retarget(s,3.f);for(int i=0;i<30;++i)p.update(1.f/60);
  const auto before=p.currentKeys();s.section=2;p.retarget(s,3.f);const auto after=p.currentKeys();
  float jump=0;for(int i=0;i<3;++i)for(int j=0;j<3;++j)jump=std::max(jump,std::abs(before.c[size_t(i)][size_t(j)]-after.c[size_t(i)][size_t(j)]));
  check("harmony_change_has_no_jump",jump<1e-4f);
  auto last=after;float maxStep=0;for(int i=0;i<240;++i){p.update(1.f/60);const auto k=p.currentKeys();for(int a=0;a<3;++a)for(int b=0;b<3;++b)maxStep=std::max(maxStep,std::abs(k.c[size_t(a)][size_t(b)]-last.c[size_t(a)][size_t(b)]));last=k;}
  check("bridge_steps_are_small",maxStep<.05f);check("bridge_arrives",p.gpu().previous[0][3]==1.f);}

 // Retargeting mid-bridge keeps colour continuous too.
 {ProceduralPalette p;p.setEnabled(true);PaletteSpec s;s.hueDegrees=30;s.section=0;p.retarget(s,4.f);for(int i=0;i<10;++i)p.update(1.f/60);
  s.section=2;p.retarget(s,4.f);for(int i=0;i<60;++i)p.update(1.f/60);const auto mid=p.currentKeys();s.section=4;p.retarget(s,4.f);const auto now=p.currentKeys();
  float jump=0;for(int i=0;i<3;++i)for(int j=0;j<3;++j)jump=std::max(jump,std::abs(mid.c[size_t(i)][size_t(j)]-now.c[size_t(i)][size_t(j)]));check("mid_bridge_retarget_continuous",jump<1e-4f);}

 // The procedural palette fades in and out rather than switching.
 {ProceduralPalette p;PaletteSpec s;p.retarget(s,2.f);p.setEnabled(true);p.update(.1f);check("palette_weight_fades_in",p.weight()>0&&p.weight()<.5f);
  for(int i=0;i<200;++i)p.update(.05f);check("palette_weight_reaches_one",p.weight()==1.f);p.setEnabled(false);p.update(.1f);check("palette_weight_fades_out",p.weight()<1.f&&p.weight()>.5f);}

 // GPU packing: coefficients arrive in xyz, extras in w.
 {ProceduralPalette p;p.setEnabled(true);PaletteSpec s;s.hueDegrees=120;p.retarget(s,2.f);for(int i=0;i<200;++i)p.update(.05f);const auto g=p.gpu();
  check("gpu_pack_weight",g.a[3]==1.f);check("gpu_pack_t0",std::abs(g.b[3]-120.f/360.f)<1e-4f);check("gpu_pack_offsets",g.c[3]>0&&g.d[3]>g.c[3]);}

 // Motion clocks: a sudden change in the music bends the speed but never makes the position jump.
 {GenerativeClock c;const float dt=1.f/60;float tempo=10.f;for(int i=0;i<120;++i){tempo+=dt*2.1f;c.update(dt,.2f,0.f,0.f,tempo,1.f);}
  const float t0=c.travel,w0=c.warpTravel,p0=c.pulse;float maxTravel=0,maxWarp=0,maxPulse=0,lastT=c.travel,lastW=c.warpTravel,lastP=c.pulse;
  for(int i=0;i<240;++i){tempo+=dt*2.1f;c.update(dt,1.f,1.f,1.f,tempo,1.f);maxTravel=std::max(maxTravel,c.travel-lastT);maxWarp=std::max(maxWarp,c.warpTravel-lastW);maxPulse=std::max(maxPulse,c.pulse-lastP);lastT=c.travel;lastW=c.warpTravel;lastP=c.pulse;}
  check("clock_travel_never_jumps",maxTravel<=dt*1.1f);check("clock_warp_never_jumps",maxWarp<=dt*2.2f);check("clock_pulse_follows_tempo",maxPulse<=dt*2.1f*1.5f+1e-4f&&c.pulse>p0);
  check("clock_keeps_advancing",c.travel>t0&&c.warpTravel>w0);}
 {GenerativeClock c;for(int i=0;i<6;++i)c.update(1.f/60,1.f,1.f,1.f,50.f,1.f);check("slow_energy_does_not_snap",c.energySlow<.12f&&c.energySlow>0.f);
  for(int i=0;i<600;++i)c.update(1.f/60,1.f,0.f,0.f,50.f+i*.03f,1.f);check("slow_energy_arrives",c.energySlow>.95f);check("slow_build_releases",c.buildSlow<.05f);}
 {GenerativeClock c;c.update(1.f/60,.5f,0.f,0.f,100.f,1.f);const float p=c.pulse;c.update(1.f/60,.5f,0.f,0.f,3.f,1.f);check("tempo_jump_adds_nothing",c.pulse==p);
  const float d=c.drift;c.update(1.f/60,.5f,0.f,0.f,3.1f,0.f);check("drift_param_zero_stops_drift",c.drift==d);}
 if(failures==0)std::printf("generative foundation: all checks passed\n");
 return failures?1:0;
}

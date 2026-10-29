#pragma once
#include <algorithm>
#include <cmath>
// Motion clocks for generated worlds. A shader that multiplies absolute time (or tempo) by something that
// changes with the music makes its picture jump whenever that something changes, because the product of a
// large number and a small change is a large change. Speed has to be integrated instead: the clock
// accumulates speed * dt, so a change in speed bends the motion smoothly and position never jumps.
// The slow followers give the structural controls (how far the infection has spread, how tight the warp is)
// a response that follows the section rather than every hit; hits are left to the shader's own fast values.
namespace rarestage::show {
struct GenerativeClock{
 float travel{0},pulse{0},drift{0},warpTravel{0};   // integrated clocks, handed to the shader as g_Clock
 float energySlow{0},buildSlow{0},dropSlow{0};      // slow followers, handed to the shader as g_Clock2
 float lastTempo{-1};
 static float Follow(float v,float target,float dt,float up,float down){const float tau=std::max(target>v?up:down,.01f);return v+(target-v)*(1.f-std::exp(-dt/tau));}
 // driftParam is the CameraDrift pool value (1 when the modulation matrix is off).
 void update(float dt,float energy,float build,float drop,float tempoPhase,float driftParam){
  dt=std::clamp(dt,0.f,.1f);
  energySlow=Follow(energySlow,std::clamp(energy,0.f,1.f),dt,1.2f,2.4f);
  buildSlow=Follow(buildSlow,std::clamp(build,0.f,1.f),dt,.9f,.6f);
  dropSlow=Follow(dropSlow,std::clamp(drop,0.f,1.f),dt,.15f,1.6f);
  travel+=dt*(.6f+.45f*energySlow);
  drift+=dt*std::clamp(driftParam,0.f,2.5f);
  // Beats come from the tempo clock itself. A reset or a jump (new song, anchor change) adds nothing.
  const float beats=lastTempo<0?0.f:tempoPhase-lastTempo;lastTempo=tempoPhase;
  pulse+=(beats>0.f&&beats<1.f?beats:0.f)*(1.f+.5f*buildSlow);
  const float warp=std::clamp(buildSlow*.9f+dropSlow*.9f,0.f,1.f);
  warpTravel+=dt*(.5f+1.6f*warp);
 }
};
}

#pragma once
#include <array>
#include <algorithm>
namespace rarestage::show {
struct Scene {const char* name;const wchar_t* wideName;int key,category,motion;float energy,density,anchorX,anchorY,pace,scale;int bridge;float relativeCost;};
#define RS_WIDE_IMPL(s) L##s
#define RS_WIDE(s) RS_WIDE_IMPL(s)
inline constexpr auto Scenes=std::array{
#define RS_SCENE(id,name,key,category,motion,energy,density,ax,ay,pace,scale,bridge,cost) Scene{name,RS_WIDE(name),key,category,motion,energy##f,density##f,float(ax),float(ay),pace##f,float(scale),bridge,cost##f},
#include "scene_catalog.inc"
#undef RS_SCENE
};
#undef RS_WIDE
#undef RS_WIDE_IMPL
inline constexpr int SceneCount=int(Scenes.size());
// Retired scenes keep their ids and shader code but are unselectable and never scheduled.
inline constexpr bool Retired(int id){return id==14||id==17||id==20||id==23||id==28||id==30||id==31||id==32||id==34||id==37||id==38;}
inline constexpr int ActiveSceneCount(){int n=0;for(int i=0;i<SceneCount;++i)n+=Retired(i)?0:1;return n;}
inline constexpr bool AutoScene(int id){return id>=0&&id<SceneCount&&!Retired(id)&&!(id>=28&&id<=34);}
inline int SceneForKey(int key){for(int i=0;i<SceneCount;++i)if(key!=0&&!Retired(i)&&Scenes[i].key==key)return i;return -1;}
inline bool SurfaceWall(int id){return id>=0&&id<SceneCount&&Scenes[id].bridge==1;}
inline bool NativeBridge(int a,int b){if((a==11&&b>=24&&b<=25)||(b==11&&a>=24&&a<=25))return false;return a>=0&&b>=0&&a<SceneCount&&b<SceneCount&&Scenes[a].bridge!=0&&Scenes[a].bridge==Scenes[b].bridge;}
}

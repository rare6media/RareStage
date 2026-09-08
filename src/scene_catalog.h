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
inline int SceneForKey(int key){for(int i=0;i<SceneCount;++i)if(Scenes[i].key==key)return i;return -1;}
inline bool SurfaceWall(int id){return id>=0&&id<SceneCount&&Scenes[id].bridge==1;}
inline bool NativeBridge(int a,int b){return a>=0&&b>=0&&a<SceneCount&&b<SceneCount&&Scenes[a].bridge!=0&&Scenes[a].bridge==Scenes[b].bridge;}
}

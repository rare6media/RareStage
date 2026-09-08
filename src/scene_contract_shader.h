// Generated from the same scene contract used by the CPU planner.
#define RS_TEXT_IMPL(...) #__VA_ARGS__
#define RS_TEXT(...) RS_TEXT_IMPL(__VA_ARGS__)
"static const float4 scenePose[]={"
#define RS_SCENE(id,name,key,category,motion,energy,density,ax,ay,pace,scale,bridge,cost) "float4(" RS_TEXT(ax,ay,pace,scale) "),"
#include "scene_catalog.inc"
#undef RS_SCENE
"};static const float4 sceneRig[]={"
#define RS_SCENE(id,name,key,category,motion,energy,density,ax,ay,pace,scale,bridge,cost) "float4(" RS_TEXT(motion,bridge,density,energy) "),"
#include "scene_catalog.inc"
#undef RS_SCENE
"};"
#undef RS_TEXT
#undef RS_TEXT_IMPL

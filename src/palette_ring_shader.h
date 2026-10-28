// Generated from palette_ring.inc, the table the Console and Director also use.
#define RS_TEXT_IMPL(...) #__VA_ARGS__
#define RS_TEXT(...) RS_TEXT_IMPL(__VA_ARGS__)
"static const float3 paletteRing[]={"
#define RS_PALETTE(id,name,p,a,s) "float3(" RS_TEXT(p,a,s) "),"
#include "palette_ring.inc"
#undef RS_PALETTE
"};static const float paletteRingCount=24;"
#undef RS_TEXT
#undef RS_TEXT_IMPL

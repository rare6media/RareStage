#pragma once
#include "phrase_conductor.h"
#include <cwctype>
#include <vector>
namespace rarestage::show {
inline std::wstring CatalogLower(std::wstring s){for(auto&c:s)c=wchar_t(std::towlower(c));return s;}
inline std::vector<int> CatalogSearch(const std::wstring& query,int category,bool onlyFavorites,const std::array<bool,SceneCount>& favorites){
 std::vector<int> result;const auto needle=CatalogLower(query);
 for(int scene=0;scene<SceneCount;++scene){if((category>0&&Scenes[scene].category!=category-1)||(onlyFavorites&&!favorites[scene]))continue;
  bool match=CatalogLower(Scenes[scene].wideName).find(needle)!=std::wstring::npos;
  if(PhraseFamily(scene)>=0)for(int mode=0;mode<ProgramCount(scene)&&!match;++mode)match=CatalogLower(PhraseName(scene,mode)).find(needle)!=std::wstring::npos;
  if(match)result.push_back(scene);
 }return result;
}
inline int NamedProgramCount(){int result=0;for(int scene=0;scene<SceneCount;++scene)if(PhraseFamily(scene)>=0)result+=ProgramCount(scene);return result;}
}

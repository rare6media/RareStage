#pragma once
// Source/profile keyed DXBC. Packaged bytecode avoids shader compilation at show startup.
// A missing, outdated or damaged cache is rejected and rebuilt from embedded source.
#include "shader_blob_cache.h"
static std::uint64_t ShaderKey(const char*entry,const char*profile){static const auto source=ShaderHash(Shader,strlen(Shader));auto h=ShaderHash(entry,strlen(entry),source);h=ShaderHash(profile,strlen(profile),h);const UINT flags=D3DCOMPILE_OPTIMIZATION_LEVEL3;return ShaderHash(&flags,sizeof(flags),h);}
static HRESULT CompileStageShader(const char*entry,const char*profile,ID3DBlob**blob,ID3DBlob**error){
 const auto key=ShaderKey(entry,profile);std::ostringstream name;name<<entry<<'-'<<std::hex<<key<<".rsc";
 const auto packaged=StartupLogPath().parent_path()/L"assets"/L"shaders"/name.str();
 const auto cached=SettingsPath().parent_path()/L"ShaderCache"/name.str();
 const bool prepare=std::wcsstr(GetCommandLineW(),L"--prepare-shaders")!=nullptr;
 const bool force=std::wcsstr(GetCommandLineW(),L"--self-test")!=nullptr;
 HRESULT result=S_OK;bool loaded=false;
 if(!force)loaded=ReadShaderCache(packaged,key,blob)||ReadShaderCache(cached,key,blob);
 if(!loaded){result=D3DCompile(Shader,strlen(Shader),nullptr,nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,blob,error);if(SUCCEEDED(result))WriteShaderCache(cached,key,*blob);}
 if(prepare&&SUCCEEDED(result))WriteShaderCache(packaged,key,*blob);
 const std::string message=std::string(loaded?"Loaded shader bytecode: ":"Compiled shader: ")+entry;StartupLog(message.c_str());
 return result;
}
static bool ShaderCacheSelfTest(){
 const auto dir=std::filesystem::temp_directory_path()/(L"RareStage-cache-check-"+std::to_wstring(GetCurrentProcessId()));const auto file=dir/L"fixture.rsc";
 ComPtr<ID3DBlob>source,result;D3DCreateBlob(8,&source);std::memcpy(source->GetBufferPointer(),"DXBCtest",8);WriteShaderCache(file,123,source.Get());
 bool ok=ReadShaderCache(file,123,&result);result.Reset();ok=ok&&!ReadShaderCache(file,124,&result);
 {std::fstream corrupt(file,std::ios::binary|std::ios::in|std::ios::out);corrupt.seekp(36);corrupt.put('!');}
 ok=ok&&!ReadShaderCache(file,123,&result);std::error_code error;std::filesystem::remove(file,error);std::filesystem::remove(dir,error);return ok;
}

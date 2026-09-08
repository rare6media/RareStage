#pragma once
// Source/profile keyed DXBC. Packaged bytecode avoids shader compilation at show startup.
// A missing, outdated or damaged cache is rejected and rebuilt from embedded source.
static std::uint64_t ShaderHash(const void*data,size_t size,std::uint64_t h=14695981039346656037ull){const auto*p=static_cast<const unsigned char*>(data);for(size_t i=0;i<size;++i){h^=p[i];h*=1099511628211ull;}return h;}
static std::uint64_t ShaderKey(const char*entry,const char*profile){static const auto source=ShaderHash(Shader,strlen(Shader));auto h=ShaderHash(entry,strlen(entry),source);h=ShaderHash(profile,strlen(profile),h);const UINT flags=D3DCOMPILE_OPTIMIZATION_LEVEL3;return ShaderHash(&flags,sizeof(flags),h);}
static bool ReadShaderCache(const std::filesystem::path&path,std::uint64_t key,ID3DBlob**output){
 std::ifstream input(path,std::ios::binary);std::uint64_t header[4]{};if(!input.read(reinterpret_cast<char*>(header),sizeof(header)))return false;
 if(header[0]!=0x3143584447525352ull||header[1]!=key||header[3]<4||header[3]>16*1024*1024)return false;
 ComPtr<ID3DBlob>blob;if(FAILED(D3DCreateBlob(size_t(header[3]),&blob)))return false;
 if(!input.read(static_cast<char*>(blob->GetBufferPointer()),std::streamsize(header[3]))||input.peek()!=std::char_traits<char>::eof())return false;
 if(std::memcmp(blob->GetBufferPointer(),"DXBC",4)||ShaderHash(blob->GetBufferPointer(),blob->GetBufferSize())!=header[2])return false;
 *output=blob.Detach();return true;
}
static void WriteShaderCache(const std::filesystem::path&path,std::uint64_t key,ID3DBlob*blob){
 std::error_code error;std::filesystem::create_directories(path.parent_path(),error);if(error)return;
 const std::uint64_t header[]{0x3143584447525352ull,key,ShaderHash(blob->GetBufferPointer(),blob->GetBufferSize()),blob->GetBufferSize()};
 auto temporary=path;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
 {std::ofstream output(temporary,std::ios::binary);output.write(reinterpret_cast<const char*>(header),sizeof(header));output.write(static_cast<const char*>(blob->GetBufferPointer()),std::streamsize(blob->GetBufferSize()));if(!output.good()){output.close();std::filesystem::remove(temporary,error);return;}}
 // Atomic replacement; a concurrent reader sees either complete version.
 if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))std::filesystem::remove(temporary,error);
}
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

#pragma once
#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
using Microsoft::WRL::ComPtr;
// Checksummed DXBC blobs on disk. Shared by the stage shader cache and the generated-world cache.
// A missing, outdated or damaged file is rejected; writes are atomic so a reader sees a whole file or none.
inline std::uint64_t ShaderHash(const void*data,size_t size,std::uint64_t h=14695981039346656037ull){const auto*p=static_cast<const unsigned char*>(data);for(size_t i=0;i<size;++i){h^=p[i];h*=1099511628211ull;}return h;}
inline bool ReadShaderCache(const std::filesystem::path&path,std::uint64_t key,ID3DBlob**output){
 std::ifstream input(path,std::ios::binary);std::uint64_t header[4]{};if(!input.read(reinterpret_cast<char*>(header),sizeof(header)))return false;
 if(header[0]!=0x3143584447525352ull||header[1]!=key||header[3]<4||header[3]>16*1024*1024)return false;
 ComPtr<ID3DBlob>blob;if(FAILED(D3DCreateBlob(size_t(header[3]),&blob)))return false;
 if(!input.read(static_cast<char*>(blob->GetBufferPointer()),std::streamsize(header[3]))||input.peek()!=std::char_traits<char>::eof())return false;
 if(std::memcmp(blob->GetBufferPointer(),"DXBC",4)||ShaderHash(blob->GetBufferPointer(),blob->GetBufferSize())!=header[2])return false;
 *output=blob.Detach();return true;
}
inline void WriteShaderCache(const std::filesystem::path&path,std::uint64_t key,ID3DBlob*blob){
 std::error_code error;std::filesystem::create_directories(path.parent_path(),error);if(error)return;
 const std::uint64_t header[]{0x3143584447525352ull,key,ShaderHash(blob->GetBufferPointer(),blob->GetBufferSize()),blob->GetBufferSize()};
 auto temporary=path;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
 {std::ofstream output(temporary,std::ios::binary);output.write(reinterpret_cast<const char*>(header),sizeof(header));output.write(static_cast<const char*>(blob->GetBufferPointer()),std::streamsize(blob->GetBufferSize()));if(!output.good()){output.close();std::filesystem::remove(temporary,error);return;}}
 // Atomic replacement; a concurrent reader sees either complete version.
 if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))std::filesystem::remove(temporary,error);
}

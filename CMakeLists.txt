cmake_minimum_required(VERSION 3.24)
project(RareStage VERSION 0.1.0 LANGUAGES CXX)

add_executable(RareStage WIN32 src/main.cpp)
target_compile_features(RareStage PRIVATE cxx_std_20)
target_compile_definitions(RareStage PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
  target_compile_options(RareStage PRIVATE /W4 /permissive- /EHsc)
endif()
target_link_libraries(RareStage PRIVATE d3d11 dxgi d3dcompiler ole32 avrt uuid)

install(TARGETS RareStage RUNTIME DESTINATION .)

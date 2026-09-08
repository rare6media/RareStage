cmake_minimum_required(VERSION 3.24)
project(RareStage VERSION 0.17.0 LANGUAGES CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(RareStage WIN32 src/main.cpp src/rarestage.rc)
target_compile_features(RareStage PRIVATE cxx_std_20)
target_compile_definitions(RareStage PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
  target_compile_options(RareStage PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(RareStage PRIVATE /MANIFEST:NO)
endif()
target_link_libraries(RareStage PRIVATE d3d11 dxgi d3dcompiler d2d1 dwrite ole32 avrt uuid windowscodecs gdi32 ws2_32)

add_executable(RareStageConsole WIN32 src/console.cpp src/rarestage_console.rc)
target_compile_features(RareStageConsole PRIVATE cxx_std_20)
target_compile_definitions(RareStageConsole PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(RareStageConsole PRIVATE dwmapi gdi32 user32 shell32)
if(MSVC)
  target_compile_options(RareStageConsole PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(RareStageConsole PRIVATE /MANIFEST:NO)
endif()

include(CTest)
add_executable(RareStageProtocolTests tests/control_protocol_test.cpp)
target_compile_features(RareStageProtocolTests PRIVATE cxx_std_20)
target_compile_definitions(RareStageProtocolTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(RareStageProtocolTests PRIVATE src)
if(MSVC)
  target_compile_options(RareStageProtocolTests PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
add_test(NAME RareStageControlProtocol COMMAND RareStageProtocolTests)

option(RARESTAGE_ENABLE_SPOUT "Build the clean Spout2 stage-output sender" ON)
if(RARESTAGE_ENABLE_SPOUT)
  include(FetchContent)
  set(SPOUT_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SPOUT_BUILD_SPOUTDX ON CACHE BOOL "" FORCE)
  set(SPOUT_BUILD_CMT ON CACHE BOOL "" FORCE)
  set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    spout2
    GIT_REPOSITORY https://github.com/leadedge/Spout2.git
    GIT_TAG c2bcc12147711d12ace7d5f08e869d774d840f8a
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(spout2)
  target_link_libraries(RareStage PRIVATE SpoutDX_static)
  target_compile_definitions(RareStage PRIVATE RARESTAGE_SPOUT SPOUT_BUILD_STATIC)
endif()

install(TARGETS RareStage RareStageConsole RUNTIME DESTINATION .)
install(FILES THIRD_PARTY_NOTICES.md README.md DESTINATION .)
install(FILES assets/rare-logo.png DESTINATION assets)

cmake_minimum_required(VERSION 3.24)
project(RareStage VERSION 0.31.0 LANGUAGES CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(RareStage WIN32 src/main.cpp src/rarestage.rc)
target_compile_features(RareStage PRIVATE cxx_std_20)
target_compile_definitions(RareStage PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
  target_compile_options(RareStage PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(RareStage PRIVATE /MANIFEST:NO)
endif()
target_link_libraries(RareStage PRIVATE d3d11 dxgi d3dcompiler d2d1 dwrite ole32 avrt uuid windowscodecs gdi32 ws2_32)

# The GDI console stays available as RareStageConsoleClassic until every legacy tab has an ImGui equivalent.
add_executable(RareStageConsoleClassic WIN32 src/console.cpp src/rarestage_console.rc)
target_compile_features(RareStageConsoleClassic PRIVATE cxx_std_20)
target_compile_definitions(RareStageConsoleClassic PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(RareStageConsoleClassic PRIVATE dwmapi gdi32 user32 shell32)
if(MSVC)
  target_compile_options(RareStageConsoleClassic PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(RareStageConsoleClassic PRIVATE /MANIFEST:NO)
endif()

include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG 97428e8ac99e339ce05eee531cf55b77b29ea709 # v1.91.9
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)
add_library(RareStageImGui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(RareStageImGui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(RareStageImGui PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(RareStageImGui PUBLIC d3d11 dxgi d3dcompiler dwmapi)

add_executable(RareStageConsole WIN32 src/console_imgui.cpp src/rarestage_console.rc)
target_compile_features(RareStageConsole PRIVATE cxx_std_20)
target_compile_definitions(RareStageConsole PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(RareStageConsole PRIVATE RARESTAGE_VERSION="${PROJECT_VERSION}")
target_link_libraries(RareStageConsole PRIVATE RareStageImGui user32 gdi32 shell32 windowscodecs ole32)
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

add_executable(RareStagePhraseTests tests/phrase_conductor_test.cpp)
target_compile_features(RareStagePhraseTests PRIVATE cxx_std_20)
target_include_directories(RareStagePhraseTests PRIVATE src)
if(MSVC)
  target_compile_options(RareStagePhraseTests PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
add_test(NAME RareStagePhraseConductor COMMAND RareStagePhraseTests)

add_executable(RareStageMotionTests tests/motion_worlds_test.cpp)
target_compile_features(RareStageMotionTests PRIVATE cxx_std_20)
target_include_directories(RareStageMotionTests PRIVATE src)
add_test(NAME RareStageMotionWorlds COMMAND RareStageMotionTests)

add_executable(RareStageActionTests tests/action_director_test.cpp)
target_compile_features(RareStageActionTests PRIVATE cxx_std_20)
target_include_directories(RareStageActionTests PRIVATE src)
add_test(NAME RareStageActionDirector COMMAND RareStageActionTests)

add_executable(RareStageColorTests tests/color_director_test.cpp)
target_compile_features(RareStageColorTests PRIVATE cxx_std_20)
target_include_directories(RareStageColorTests PRIVATE src)
if(MSVC)
  target_compile_options(RareStageColorTests PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
add_test(NAME RareStageColorDirector COMMAND RareStageColorTests)

add_executable(RareStageGenerativeTests tests/generative_foundation_test.cpp)
target_compile_features(RareStageGenerativeTests PRIVATE cxx_std_20)
target_include_directories(RareStageGenerativeTests PRIVATE src)
if(MSVC)
  target_compile_options(RareStageGenerativeTests PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
add_test(NAME RareStageGenerativeFoundation COMMAND RareStageGenerativeTests)

add_executable(RareStageGenDirectorTests tests/generative_director_test.cpp)
target_compile_features(RareStageGenDirectorTests PRIVATE cxx_std_20)
target_include_directories(RareStageGenDirectorTests PRIVATE src)
if(MSVC)
  target_compile_options(RareStageGenDirectorTests PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
add_test(NAME RareStageGenerativeDirector COMMAND RareStageGenDirectorTests)

add_executable(RareStageWorldSpecTests tests/world_spec_test.cpp)
target_compile_features(RareStageWorldSpecTests PRIVATE cxx_std_20)
target_compile_definitions(RareStageWorldSpecTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(RareStageWorldSpecTests PRIVATE src)
target_link_libraries(RareStageWorldSpecTests PRIVATE d3d11 d3dcompiler dxguid windowscodecs ole32)
if(MSVC)
  target_compile_options(RareStageWorldSpecTests PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
add_test(NAME RareStageWorldSpec COMMAND RareStageWorldSpecTests)

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

install(TARGETS RareStage RareStageConsole RareStageConsoleClassic RUNTIME DESTINATION .)
install(FILES THIRD_PARTY_NOTICES.md README.md DESTINATION .)
install(FILES assets/rare-logo.png DESTINATION assets)

install(DIRECTORY "${CMAKE_BINARY_DIR}/Release/assets/shaders/" DESTINATION assets/shaders OPTIONAL)

install(DIRECTORY assets/catalog/ DESTINATION assets/catalog OPTIONAL)

// RareStage Console (Dear ImGui). Talks to RareStage.exe through the same shared-memory telemetry
// and window-message commands as the classic GDI console; the renderer is unchanged by the UI.
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "rarestage_control.h"
#include "world_spec.h"
#include "world_qualifier.h"
#include "console_splash.h"

#ifndef RARESTAGE_VERSION
#define RARESTAGE_VERSION "0.31.0"
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

using rarestage::control::Command;
using rarestage::control::SharedState;

std::string Utf8(const wchar_t* text) {
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) return {};
    std::string result(static_cast<size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), bytes, nullptr, nullptr);
    return result;
}

constexpr std::array<const char*, 5> SectionNames{"OPEN", "BUILD", "DROP", "BREAKDOWN", "DRIVE"};
constexpr std::array<const char*, 4> ProfileNames{"CALM", "BALANCED", "STAGE", "AGGRESSIVE"};
constexpr std::array<const char*, 4> PresetNames{"CLEAN OPEN", "MELODIC BLOOM", "RARE DROP", "GLITCH FINALE"};
constexpr std::array<const char*, 4> TransitionNames{"ROW FLOW", "RIBBON FLOW", "ORBIT FLOW", "DEPTH FOLD"};
constexpr std::array<const char*, 4> QualityNames{"PERFORMANCE", "BALANCED", "NATIVE HD", "ULTRA 4K"};
constexpr std::array<const char*, 4> AudioProfileNames{"AUTO", "EDM", "BASS MUSIC", "MELODIC"};
constexpr std::array<const char*, 4> MasteringNames{"STUDIO", "LED SAFE", "ARENA", "BROADCAST"};
constexpr std::array<const char*, 4> StageLayoutNames{"FULL FRAME", "ULTRAWIDE", "TRIPTYCH", "LED FACADE"};
constexpr std::array<const char*, 5> DutyNames{"OPEN", "SUPPORT", "PEAK", "BREAKDOWN", "CLOSING"};
constexpr std::array<const char*, 3> SyncNames{"LOCAL", "LEADER", "FOLLOWER"};
constexpr std::array<const char*, 3> TempoAnchorNames{"FOLLOW MUSIC", "LOCK 128", "LOCK 130"};
constexpr std::array<const char*, 5> CategoryNames{"ALL", "PANELS", "RIBBONS", "DEPTH", "ATMOS"};

struct BandInfo {const char* name; const char* range; ImU32 color;};
const std::array<BandInfo, 7> Bands{{
    {"SUB", "25-80 Hz", IM_COL32(120, 90, 255, 255)},
    {"KICK", "45-145 Hz", IM_COL32(190, 80, 255, 255)},
    {"BASS", "70-230 Hz", IM_COL32(255, 80, 170, 255)},
    {"LOW MID", "230-700 Hz", IM_COL32(255, 150, 70, 255)},
    {"MID", "0.7-2.8 kHz", IM_COL32(250, 210, 70, 255)},
    {"PRESENCE", "2.8-6 kHz", IM_COL32(90, 230, 150, 255)},
    {"AIR", "6 kHz+", IM_COL32(80, 210, 255, 255)},
}};

ImVec4 FamilyColor(int family) {
    switch (std::clamp(family, 0, 3)) {
        case 0: return {0.12f, 0.62f, 0.78f, 1.f};
        case 1: return {0.50f, 0.32f, 0.82f, 1.f};
        case 2: return {0.16f, 0.62f, 0.40f, 1.f};
        default: return {0.78f, 0.52f, 0.14f, 1.f};
    }
}

class Console {
public:
    bool init(HINSTANCE instance, int show, bool withSplash = false) {
        WNDCLASSW wc{};
        wc.hInstance = instance;
        wc.lpfnWndProc = WndProc;
        wc.lpszClassName = L"RareStageConsoleWindow";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(2));
        RegisterClassW(&wc);
        window_ = CreateWindowExW(0, wc.lpszClassName, L"RareStage Console", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                  CW_USEDEFAULT, 1400, 860, nullptr, nullptr, instance, this);
        if (!window_) return false;
        if (!createDevice()) return false;
        ShowWindow(window_, show);
        UpdateWindow(window_);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        const float dpi = std::max(1.f, float(GetDpiForWindow(window_)) / 96.f);
        const char* fontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
        if (std::filesystem::exists(fontPath)) io.Fonts->AddFontFromFileTTF(fontPath, 17.f * dpi);
        else io.FontGlobalScale = dpi;
        applyStyle(dpi);
        ImGui_ImplWin32_Init(window_);
        ImGui_ImplDX11_Init(device_, context_);
        if (withSplash) {
            wchar_t exe[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            const std::filesystem::path logo = std::filesystem::path(exe).parent_path() / L"assets" / L"rare-logo.png";
            splash_.setDpi(dpi);
            if (!splash_.load(device_, logo.wstring(), RARESTAGE_VERSION)) splash_ = rarestage::console::Splash{};
        }
        return true;
    }

    int run(bool startupTest) {
        MSG message{};
        int frames = 0;
        while (message.message != WM_QUIT) {
            if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
                continue;
            }
            if (IsIconic(window_)) {
                Sleep(40);
                continue;
            }
            updateConnection();
            frame();
            if (startupTest && ++frames >= 30) PostMessageW(window_, WM_CLOSE, 0, 0);
        }
        shutdown();
        return 0;
    }

private:
    // ---- window / device -------------------------------------------------------------------
    static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (m == WM_NCCREATE) {
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
        }
        auto* self = reinterpret_cast<Console*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
        if (self && m == WM_SIZE && w != SIZE_MINIMIZED && self->device_) {
            self->resize(UINT(LOWORD(l)), UINT(HIWORD(l)));
            return 0;
        }
        if (m == WM_SYSCOMMAND && (w & 0xfff0) == SC_KEYMENU) return 0;
        if (m == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(h, m, w, l);
    }

    bool createDevice() {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = window_;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
        D3D_FEATURE_LEVEL got{};
        HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                                   &sd, &swap_, &device_, &got, &context_);
        if (FAILED(hr)) {
            hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
                                               &swap_, &device_, &got, &context_);
        }
        if (FAILED(hr)) return false;
        createTarget();
        return rtv_ != nullptr;
    }
    void createTarget() {
        ID3D11Texture2D* back = nullptr;
        if (SUCCEEDED(swap_->GetBuffer(0, IID_PPV_ARGS(&back))) && back) {
            device_->CreateRenderTargetView(back, nullptr, &rtv_);
            back->Release();
        }
    }
    void releaseTarget() {
        if (rtv_) {
            rtv_->Release();
            rtv_ = nullptr;
        }
    }
    void resize(UINT width, UINT height) {
        if (!swap_ || width == 0 || height == 0) return;
        releaseTarget();
        swap_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        createTarget();
    }
    void shutdown() {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        releaseTarget();
        if (swap_) swap_->Release();
        if (context_) context_->Release();
        if (device_) device_->Release();
        if (mapped_) UnmapViewOfFile(mapped_);
        if (mapping_) CloseHandle(mapping_);
        if (rendererProcess_) CloseHandle(rendererProcess_);
    }

    static void applyStyle(float dpi) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark(&style);
        style.WindowRounding = 0;
        style.FrameRounding = 6;
        style.GrabRounding = 6;
        style.TabRounding = 6;
        style.ChildRounding = 8;
        style.FramePadding = {10, 6};
        style.ItemSpacing = {10, 8};
        style.ScrollbarSize = 14;
        style.ScaleAllSizes(dpi);
        ImVec4* c = style.Colors;
        c[ImGuiCol_WindowBg] = {0.045f, 0.050f, 0.070f, 1};
        c[ImGuiCol_ChildBg] = {0.070f, 0.078f, 0.105f, 1};
        c[ImGuiCol_FrameBg] = {0.105f, 0.115f, 0.155f, 1};
        c[ImGuiCol_FrameBgHovered] = {0.15f, 0.17f, 0.23f, 1};
        c[ImGuiCol_FrameBgActive] = {0.18f, 0.20f, 0.28f, 1};
        c[ImGuiCol_Button] = {0.13f, 0.15f, 0.21f, 1};
        c[ImGuiCol_ButtonHovered] = {0.20f, 0.23f, 0.32f, 1};
        c[ImGuiCol_ButtonActive] = {0.26f, 0.30f, 0.42f, 1};
        c[ImGuiCol_SliderGrab] = {0.30f, 0.80f, 0.95f, 1};
        c[ImGuiCol_SliderGrabActive] = {0.55f, 0.92f, 1.0f, 1};
        c[ImGuiCol_CheckMark] = {0.30f, 0.80f, 0.95f, 1};
        c[ImGuiCol_Tab] = {0.09f, 0.10f, 0.14f, 1};
        c[ImGuiCol_TabHovered] = {0.20f, 0.45f, 0.55f, 1};
        c[ImGuiCol_TabActive] = {0.14f, 0.40f, 0.50f, 1};
        c[ImGuiCol_Header] = {0.14f, 0.32f, 0.42f, 1};
        c[ImGuiCol_Border] = {0.16f, 0.18f, 0.25f, 1};
    }

    // ---- connection ------------------------------------------------------------------------
    void updateConnection() {
        if (rendererProcess_) {
            DWORD code = STILL_ACTIVE;
            if (GetExitCodeProcess(rendererProcess_, &code) && code != STILL_ACTIVE) {
                CloseHandle(rendererProcess_);
                rendererProcess_ = nullptr;
            }
        }
        if (!mapping_) {
            mapping_ = OpenFileMappingW(FILE_MAP_READ, FALSE, rarestage::control::MappingName);
            if (mapping_) {
                mapped_ = static_cast<const SharedState*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, sizeof(SharedState)));
                if (!mapped_) {
                    CloseHandle(mapping_);
                    mapping_ = nullptr;
                }
            }
        }
        SharedState next{};
        connected_ = false;
        if (!mapped_ || !rarestage::control::ReadConsistent(mapped_, next)) return;
        compatible_ = next.protocolVersion == rarestage::control::ProtocolVersion && next.structSize == sizeof(SharedState) &&
                      next.worldCount == rarestage::control::WorldCount;
        const std::uint64_t now = GetTickCount64();
        const bool alive = now < next.heartbeatTickMs + 1800;
        if (!compatible_ || !alive || !(next.flags & rarestage::control::RendererReady)) {
            if (!alive && mapped_) {
                // The renderer closed; drop the stale mapping so a restarted renderer is picked up.
                UnmapViewOfFile(mapped_);
                CloseHandle(mapping_);
                mapped_ = nullptr;
                mapping_ = nullptr;
                renderer_ = nullptr;
            }
            return;
        }
        if (!renderer_ || !IsWindow(renderer_)) {
            struct Search {DWORD process; HWND window;} search{next.rendererProcessId, nullptr};
            EnumWindows(
                [](HWND w, LPARAM data) -> BOOL {
                    auto* s = reinterpret_cast<Search*>(data);
                    DWORD process = 0;
                    GetWindowThreadProcessId(w, &process);
                    wchar_t name[64]{};
                    GetClassNameW(w, name, 64);
                    if (process == s->process && std::wcscmp(name, rarestage::control::RendererWindowClass) == 0) {
                        s->window = w;
                        return FALSE;
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&search));
            renderer_ = search.window;
        }
        if (!renderer_) return;
        state_ = next;
        connected_ = true;
        if (next.stateRevision != lastRevision_) {
            lastRevision_ = next.stateRevision;
            historyHead_ = (historyHead_ + 1) % HistoryLength;
            for (size_t band = 0; band < 7; ++band) history_[band][historyHead_] = next.bands[band];
            kickHistory_[historyHead_] = next.kick;
            snareHistory_[historyHead_] = next.snare;
            hatHistory_[historyHead_] = next.percussion;
        }
        if (next.lastCommandSerial == pendingSerial_ && pendingSerial_) {
            status_ = pendingText_ + (next.lastCommandStatus == rarestage::control::CommandStatus::Rejected ? " rejected" : " ok");
            pendingSerial_ = 0;
        }
    }

    bool flag(rarestage::control::StateFlag f) const { return (state_.flags & f) != 0; }

    void send(Command command, int value, const char* what = "") {
        if (!connected_ || !renderer_) {
            status_ = "RareStage is offline. Launch the renderer first.";
            return;
        }
        const std::uint32_t serial = ++serial_;
        if (PostMessageW(renderer_, rarestage::control::CommandMessage, static_cast<WPARAM>(command),
                         rarestage::control::PackCommand(value, serial))) {
            pendingSerial_ = serial;
            pendingText_ = what;
            if (*what) status_ = pendingText_ + " sent...";
        } else {
            status_ = "Windows rejected the control message.";
        }
    }

    void launchRenderer() {
        if (renderer_ && IsWindow(renderer_)) {
            ShowWindow(renderer_, SW_RESTORE);
            SetForegroundWindow(renderer_);
            return;
        }
        if (rendererProcess_) {
            status_ = "RareStage is still starting...";
            return;
        }
        wchar_t module[MAX_PATH]{};
        GetModuleFileNameW(nullptr, module, MAX_PATH);
        const auto directory = std::filesystem::path(module).parent_path();
        const auto executable = directory / L"RareStage.exe";
        if (!std::filesystem::exists(executable)) {
            status_ = "RareStage.exe was not found beside RareStageConsole.exe.";
            return;
        }
        SHELLEXECUTEINFOW execute{};
        execute.cbSize = sizeof(execute);
        execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        execute.lpVerb = L"open";
        execute.lpFile = executable.c_str();
        execute.lpDirectory = directory.c_str();
        execute.nShow = SW_SHOWNOACTIVATE;
        if (ShellExecuteExW(&execute) && execute.hProcess) {
            rendererProcess_ = execute.hProcess;
            status_ = "RareStage is starting.";
        } else {
            status_ = "RareStage could not be launched (Windows error " + std::to_string(GetLastError()) + ").";
        }
    }

    void applyDjName() {
        if (!connected_ || !renderer_) return;
        wchar_t wide[64]{};
        MultiByteToWideChar(CP_UTF8, 0, djName_, -1, wide, 63);
        std::wstring name = wide;
        if (name.empty()) name = L"RARE";
        COPYDATASTRUCT copy{};
        copy.dwData = rarestage::control::DjNameCopyDataId;
        copy.cbData = static_cast<DWORD>((name.size() + 1) * sizeof(wchar_t));
        copy.lpData = name.data();
        DWORD_PTR result{};
        const bool ok = SendMessageTimeoutW(renderer_, WM_COPYDATA, reinterpret_cast<WPARAM>(window_), reinterpret_cast<LPARAM>(&copy),
                                            SMTO_ABORTIFHUNG | SMTO_BLOCK, 750, &result) != 0 && result != 0;
        status_ = ok ? "DJ name updated." : "RareStage did not accept the DJ name.";
    }

    // ---- widgets ---------------------------------------------------------------------------
    // A slider that streams commands while dragging (rate limited) and always sends the final value.
    bool commandSlider(const char* label, int current, int lo, int hi, Command command, int base, const char* format,
                       int resetValue) {
        const ImGuiID id = ImGui::GetID(label);
        auto found = pending_.find(id);
        int value = found != pending_.end() ? found->second : current;
        bool changed = false;
        if (ImGui::SliderInt(label, &value, lo, hi, format)) {
            pending_[id] = value;
            const auto now = std::chrono::steady_clock::now();
            if (now - lastSend_[id] > std::chrono::milliseconds(40)) {
                send(command, base + value);
                lastSend_[id] = now;
            }
            changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            send(command, base + value, label);
            pending_.erase(id);
        } else if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            send(command, base + resetValue, label);
            pending_.erase(id);
        } else if (!ImGui::IsItemActive() && found != pending_.end() && !changed) {
            pending_.erase(id);
        }
        return changed;
    }

    static void meterBar(ImDrawList* draw, ImVec2 origin, ImVec2 size, float level, float onset, ImU32 color) {
        draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(20, 23, 32, 255), 5.f);
        const float h = size.y * std::clamp(level, 0.f, 1.f);
        if (h > 1.f) {
            draw->AddRectFilledMultiColor({origin.x, origin.y + size.y - h}, {origin.x + size.x, origin.y + size.y},
                                          color, color, (color & 0x00ffffff) | 0x66000000, (color & 0x00ffffff) | 0x66000000);
        }
        const float cap = origin.y + size.y - size.y * std::clamp(std::max(level, onset), 0.f, 1.f);
        draw->AddRectFilled({origin.x, cap - 2.f}, {origin.x + size.x, cap + 2.f},
                            onset > 0.15f ? IM_COL32(255, 255, 255, 255) : color, 2.f);
    }

    static bool toggleButton(const char* label, bool active, ImVec4 onColor) {
        ImGui::PushStyleColor(ImGuiCol_Button, active ? onColor : ImGui::GetStyle().Colors[ImGuiCol_Button]);
        const bool pressed = ImGui::Button(label);
        ImGui::PopStyleColor();
        return pressed;
    }

    template <size_t N>
    void choiceRow(const char* label, const std::array<const char*, N>& names, int current, Command command) {
        ImGui::TextDisabled("%s", label);
        for (size_t i = 0; i < N; ++i) {
            if (i) ImGui::SameLine();
            if (toggleButton((std::string(names[i]) + "##" + label).c_str(), int(i) == current, {0.12f, 0.50f, 0.62f, 1.f}))
                send(command, int(i), names[i]);
        }
    }

    void checkRow(const char* label, bool value, Command command) {
        bool v = value;
        if (ImGui::Checkbox(label, &v)) send(command, v ? 1 : 0, label);
    }

    // ---- frame -----------------------------------------------------------------------------
    void frame() {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (splash_.active()) {
            // The launch animation owns the window until it finishes; a click or key skips it.
            if (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1) || ImGui::IsKeyPressed(ImGuiKey_Space) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape)) splash_.skip();
            const ImGuiViewport* vp = ImGui::GetMainViewport();
            splash_.draw(ImGui::GetBackgroundDrawList(), vp->Pos, vp->Size, ImGui::GetIO().DeltaTime);
            renderFrame();
            return;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::Begin("##root", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);
        header();
        if (ImGui::BeginTabBar("tabs")) {
            if (ImGui::BeginTabItem("LIVE")) { tabLive(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("MIX")) { tabMix(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("AUDIO LAB")) { tabAudioLab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("DIRECTOR")) { tabDirector(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("OUTPUT")) { tabOutput(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("PRODUCTION")) { tabProduction(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        ImGui::End();

        renderFrame();
    }

    void renderFrame() {
        ImGui::Render();
        const float clear[4]{0.045f, 0.050f, 0.070f, 1.f};
        context_->OMSetRenderTargets(1, &rtv_, nullptr);
        context_->ClearRenderTargetView(rtv_, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swap_->Present(1, 0);
    }

    void header() {
        const auto& s = state_;
        if (!connected_) {
            ImGui::TextColored({1.f, 0.72f, 0.25f, 1.f}, "RARESTAGE OFFLINE");
            ImGui::SameLine();
            if (mapped_ && !compatible_) ImGui::TextDisabled("  version mismatch: run the matching Stage and Console pair");
            if (ImGui::Button("Launch RareStage")) launchRenderer();
            ImGui::Separator();
            return;
        }
        std::string worldName = Utf8(s.worldName);
        if (s.standbyState != 0) worldName = s.standbyState == 1 ? "LAUNCH SEQUENCE" : "STANDBY  -  pick a scene or press AUTO to start the show";
        else if (s.previewState == 2 && s.previewFamily >= 0 && s.previewFamily < rarestage::gen::FamilyCount)
            worldName = std::string(rarestage::gen::FamilyNames[s.previewFamily]) + "  (preview)";
        else if (s.genLiveState != 0 && s.genLiveMix > 0.5f && s.genLiveFamily >= 0 && s.genLiveFamily < rarestage::gen::FamilyCount)
            worldName = std::string(rarestage::gen::FamilyNames[s.genLiveFamily]) + "  (generated)";
        ImGui::PushStyleColor(ImGuiCol_Text, {0.42f, 0.90f, 1.f, 1.f});
        ImGui::Text("ON AIR  %s", worldName.empty() ? "-" : worldName.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("   %s  |  %s  |  %.0f BPM  |  %.0f FPS  |  GPU %.1f / %.1f ms",
                            SectionNames[size_t(std::clamp(s.songSection, 0, 4))], flag(rarestage::control::Automatic) ? "AUTO" : "MANUAL",
                            s.bpm, s.fps, s.gpuFrameMs, s.gpuBudgetMs);
        ImGui::Separator();
    }

    void footer() {
        ImGui::Separator();
        ImGui::TextDisabled("%s", status_.c_str());
    }

    // ---- LIVE ------------------------------------------------------------------------------
    void tabLive() {
        const auto& s = state_;
        const bool automatic = flag(rarestage::control::Automatic);
        if (toggleButton("AUTO", automatic, {0.12f, 0.58f, 0.36f, 1.f})) send(Command::SetAutomatic, automatic ? 0 : 1, "Auto");
        ImGui::SameLine();
        const bool frozen = flag(rarestage::control::Frozen);
        if (toggleButton("FREEZE", frozen, {0.20f, 0.45f, 0.85f, 1.f})) send(Command::SetFrozen, frozen ? 0 : 1, "Freeze");
        ImGui::SameLine();
        const bool blackout = flag(rarestage::control::Blackout);
        if (toggleButton("BLACKOUT", blackout, {0.75f, 0.18f, 0.18f, 1.f})) send(Command::SetBlackout, blackout ? 0 : 1, "Blackout");
        ImGui::SameLine();
        if (ImGui::Button("CLEAN OPEN")) send(Command::CleanOpen, 0, "Clean open");
        ImGui::SameLine();
        if (ImGui::Button("FALLBACK")) send(Command::Fallback, 0, "Fallback");
        ImGui::SameLine();
        if (ImGui::Button("< PREV")) send(Command::PreviousWorld, 0, "Previous");
        ImGui::SameLine();
        if (ImGui::Button("NEXT >")) send(Command::NextWorld, 0, "Next");
        ImGui::SameLine();
        if (ImGui::Button("CAPTURE")) send(Command::CaptureFrame, 0, "Capture");

        ImGui::TextDisabled("SECTION");
        for (int i = 0; i < 5; ++i) {
            ImGui::SameLine();
            if (toggleButton((std::string(SectionNames[size_t(i)]) + "##section").c_str(), s.songSection == i, {0.55f, 0.25f, 0.80f, 1.f}))
                send(Command::ForceSection, i, SectionNames[size_t(i)]);
        }
        ImGui::SameLine();
        if (ImGui::Button("RELEASE##section")) send(Command::ClearSectionOverride, 0, "Section release");

        // Palette
        bool autoColor = flag(rarestage::control::AutoColor);
        if (ImGui::Checkbox("AUTO COLOR", &autoColor)) send(Command::SetAutoColor, autoColor ? 1 : 0, "Auto color");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(260.f);
        const int paletteIndex = std::clamp(autoColor ? s.directorPalette : s.fallbackPalette, 0, int(rarestage::show::PaletteCount) - 1);
        const std::string paletteName = Utf8(rarestage::show::Palettes[size_t(paletteIndex)].name);
        if (ImGui::BeginCombo("##palette", paletteName.c_str())) {
            for (int i = 0; i < rarestage::show::PaletteCount; ++i) {
                const auto rgb = rarestage::show::PaletteColor(i, 0);
                ImGui::ColorButton(("##sw" + std::to_string(i)).c_str(), {rgb[0], rgb[1], rgb[2], 1.f},
                                   ImGuiColorEditFlags_NoTooltip, {16, 16});
                ImGui::SameLine();
                if (ImGui::Selectable(Utf8(rarestage::show::Palettes[size_t(i)].name).c_str(), i == paletteIndex))
                    send(Command::SetPalette, i, "Palette");
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("HOLD THEME")) send(Command::HoldTheme, s.themeHeld ? 0 : 1, "Hold theme");
        ImGui::SameLine();
        if (ImGui::Button("NEXT THEME")) send(Command::NextTheme, 0, "Next theme");

        // Scene filter
        ImGui::SetNextItemWidth(260.f);
        ImGui::InputTextWithHint("##search", "search scenes", search_, sizeof(search_));
        ImGui::SameLine();
        for (int i = 0; i < 5; ++i) {
            if (i) ImGui::SameLine();
            if (toggleButton((std::string(CategoryNames[size_t(i)]) + "##cat").c_str(), category_ == i, FamilyColor(i - 1)))
                category_ = i;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("  director pool:");
        for (int i = 0; i < 5; ++i) {
            ImGui::SameLine();
            if (toggleButton((std::string(CategoryNames[size_t(i)]) + "##pool").c_str(), s.sceneCategory == i, {0.12f, 0.50f, 0.62f, 1.f}))
                send(Command::SetSceneCategory, i, "Director pool");
        }

        // Scene grid
        ImGui::BeginChild("scenes", {0, -ImGui::GetFrameHeightWithSpacing() - 6.f}, true);
        std::string needle = search_;
        std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        const float cell = 190.f * ImGui::GetIO().FontGlobalScale * (ImGui::GetFontSize() / 17.f);
        const int columns = std::max(1, int(ImGui::GetContentRegionAvail().x / (cell + ImGui::GetStyle().ItemSpacing.x)));
        int column = 0;
        for (int id = 0; id < rarestage::show::SceneCount; ++id) {
            if (rarestage::show::Retired(id)) continue;
            const auto& scene = rarestage::show::Scenes[size_t(id)];
            if (category_ > 0 && scene.category != category_ - 1) continue;
            std::string lower = scene.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (!needle.empty() && lower.find(needle) == std::string::npos) continue;
            if (column % columns) ImGui::SameLine();
            ++column;
            const bool live = connected_ && id == s.world;
            const bool next = connected_ && id == s.preparedWorld;
            ImVec4 base = FamilyColor(scene.category);
            ImGui::PushStyleColor(ImGuiCol_Button, live ? base : ImVec4(base.x * 0.30f, base.y * 0.30f, base.z * 0.30f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(base.x * 0.65f, base.y * 0.65f, base.z * 0.65f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, base);
            if (next && !live) ImGui::PushStyleColor(ImGuiCol_Border, {1.f, 0.85f, 0.3f, 1.f});
            else ImGui::PushStyleColor(ImGuiCol_Border, {0, 0, 0, 0});
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, next && !live ? 2.f : 0.f);
            if (ImGui::Button((std::string(scene.name) + "##scene" + std::to_string(id)).c_str(), {cell, 56.f})) send(Command::SelectWorld, id, scene.name);
            const ImVec2 min = ImGui::GetItemRectMin();
            if (scene.key) {
                char key[2]{char(scene.key), 0};
                ImGui::GetWindowDrawList()->AddText({min.x + 8.f, min.y + 4.f}, IM_COL32(255, 255, 255, 150), key);
            }
            if (live) ImGui::GetWindowDrawList()->AddText({min.x + 8.f, min.y + 34.f}, IM_COL32(255, 255, 255, 220), "ON AIR");
            else if (next) ImGui::GetWindowDrawList()->AddText({min.x + 8.f, min.y + 34.f}, IM_COL32(255, 217, 77, 230), "NEXT");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }
        ImGui::EndChild();
        footer();
    }

    // ---- MIX -------------------------------------------------------------------------------
    void tabMix() {
        const auto& s = state_;
        ImGui::BeginChild("mix-left", {ImGui::GetContentRegionAvail().x * 0.5f, -ImGui::GetFrameHeightWithSpacing() - 6.f}, true);
        ImGui::SeparatorText("RESPONSE CHARACTER");
        ImGui::TextWrapped("Reactivity sets how the analysis envelopes move: low glides through the music, high snaps to every hit.");
        commandSlider("Reactivity", int(std::lround(s.reactivity * 100)), 0, 100, Command::SetReactivityPercent, 0, "%d%%", 50);
        checkRow("MODULATION MATRIX (lane patch bay drives look)", s.modulationOn != 0, Command::SetModulation);
        checkRow("PROCEDURAL PALETTE (harmony from section)", s.proceduralPaletteOn != 0, Command::SetProceduralPalette);
        commandSlider("Sensitivity", int(std::lround(s.sensitivity * 100)), 50, 150, Command::SetSensitivityPercent, 0, "%d%%", 100);
        commandSlider("Beat / cues", int(std::lround(s.beatGain * 100)), 50, 150, Command::SetBeatGainPercent, 0, "%d%%", 100);
        choiceRow("REACTION PROFILE", ProfileNames, s.reactionProfile, Command::SetProfile);
        choiceRow("DETECTOR PROFILE", AudioProfileNames, s.audioProfile, Command::SetAudioProfile);
        ImGui::SeparatorText("LEGACY MIX (3 BAND)");
        commandSlider("Bass / kick", int(std::lround(s.bassGain * 100)), 50, 150, Command::SetBassGainPercent, 0, "%d%%", 100);
        commandSlider("Mids / snare", int(std::lround(s.midGain * 100)), 50, 150, Command::SetMidGainPercent, 0, "%d%%", 100);
        commandSlider("Highs / hats", int(std::lround(s.highGain * 100)), 50, 150, Command::SetHighGainPercent, 0, "%d%%", 100);
        ImGui::SeparatorText("LOOK");
        commandSlider("Logo scale", int(std::lround(s.logoScale * 100)), 75, 125, Command::SetLogoScalePercent, 0, "%d%%", 100);
        commandSlider("Cue density", int(std::lround(s.cueDensity * 100)), 65, 125, Command::SetCueDensityPercent, 0, "%d%%", 100);
        commandSlider("Brightness", int(std::lround(s.masterBrightness * 100)), 65, 100, Command::SetMasterBrightnessPercent, 0, "%d%%", 92);
        ImGui::SeparatorText("SHOW PACING");
        commandSlider("Scene seconds", int(std::lround(s.worldDwellSeconds)), 40, 120, Command::SetWorldDwellSeconds, 0, "%d s", 72);
        commandSlider("Transition", int(std::lround(s.transitionSeconds * 10)), 2, 30, Command::SetTransitionTenths, 0, "%d / 10 s", 14);
        choiceRow("TRANSITION STYLE", TransitionNames, s.transitionStyle, Command::SetTransitionStyle);
        ImGui::SeparatorText("PRESETS");
        for (int i = 0; i < 4; ++i) {
            if (i) ImGui::SameLine();
            if (toggleButton((std::string(PresetNames[size_t(i)]) + "##preset").c_str(), s.preset == i, {0.12f, 0.50f, 0.62f, 1.f}))
                send(Command::LoadPreset, i, PresetNames[size_t(i)]);
        }
        ImGui::TextDisabled("SAVE CURRENT TO");
        for (int i = 0; i < 4; ++i) {
            ImGui::SameLine();
            if (ImGui::Button(("SAVE " + std::to_string(i + 1)).c_str())) send(Command::SavePreset, i, "Preset saved");
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("mix-right", {0, -ImGui::GetFrameHeightWithSpacing() - 6.f}, true);
        ImGui::SeparatorText("INSTRUMENT LANES");
        ImGui::TextWrapped("Each lane has its own gain. Scenes that read the lanes respond to their instrument alone; the meter shows what the renderer sees after gain.");
        for (size_t band = 0; band < Bands.size(); ++band) {
            const float meterWidth = 120.f;
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float height = ImGui::GetFrameHeight();
            meterBar(ImGui::GetWindowDrawList(), origin, {meterWidth, height}, 0.f, 0.f, Bands[band].color);
            const float level = s.bands[band];
            ImGui::GetWindowDrawList()->AddRectFilled(origin, {origin.x + meterWidth * std::clamp(level, 0.f, 1.f), origin.y + height},
                                                      Bands[band].color, 5.f);
            if (s.bandOnsets[band] > 0.2f)
                ImGui::GetWindowDrawList()->AddRect(origin, {origin.x + meterWidth, origin.y + height}, IM_COL32(255, 255, 255, 220), 5.f, 0, 2.f);
            ImGui::Dummy({meterWidth, height});
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-130.f);
            const std::string label = std::string(Bands[band].name) + "  " + Bands[band].range + "##band" + std::to_string(band);
            commandSlider(label.c_str(), int(std::lround(s.bandGains[band] * 100)), 0, 200, Command::SetBandGainPercent, int(band) * 1000, "%d%%", 100);
        }
        ImGui::TextDisabled("Double-click a slider to reset it.");
        ImGui::EndChild();
        footer();
    }

    // ---- AUDIO LAB -------------------------------------------------------------------------
    void tabAudioLab() {
        const auto& s = state_;
        ImGui::BeginChild("lab-top", {0, ImGui::GetContentRegionAvail().y * 0.48f}, true);
        ImGui::SeparatorText("INSTRUMENT LANES (LIVE)");
        const ImVec2 area = ImGui::GetContentRegionAvail();
        const float laneWidth = std::min(110.f, (area.x - 10.f * 6.f) / 7.f);
        const float height = area.y - ImGui::GetTextLineHeightWithSpacing() * 2.f;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        auto* draw = ImGui::GetWindowDrawList();
        for (size_t band = 0; band < Bands.size(); ++band) {
            const ImVec2 p{origin.x + float(band) * (laneWidth + 10.f), origin.y};
            meterBar(draw, p, {laneWidth, height}, s.bands[band], s.bandOnsets[band], Bands[band].color);
            draw->AddText({p.x + 2.f, p.y + height + 2.f}, IM_COL32(220, 225, 240, 255), Bands[band].name);
            draw->AddText({p.x + 2.f, p.y + height + 2.f + ImGui::GetTextLineHeight()}, IM_COL32(130, 140, 165, 255), Bands[band].range);
        }
        // Detector panel to the right of the lanes.
        const float panelX = origin.x + 7.f * (laneWidth + 10.f) + 20.f;
        ImGui::SetCursorScreenPos({panelX, origin.y});
        ImGui::BeginGroup();
        detectorBar("KICK", s.kick, s.kickEvidence, s.detectorThreshold, s.kickConfidence);
        detectorBar("SNARE", s.snare, s.snareEvidence, s.detectorThreshold, s.snareConfidence);
        detectorBar("HATS", s.percussion, s.hatEvidence, s.detectorThreshold, s.hatConfidence);
        ImGui::Text("BPM %.1f   confidence %.0f%%", s.bpm, s.tempoConfidence * 100.f);
        ImGui::Text("calibration %.0f%%   latency %.0f ms   %.0f Hz", s.calibrationProgress * 100.f, s.analysisLatencyMs, s.sampleRate);
        ImGui::Text("accepted K %u  S %u  H %u   rejected %u", s.acceptedKickCount, s.acceptedSnareCount, s.acceptedHatCount, s.rejectedTransientCount);
        ImGui::TextDisabled("detector profile in use: %s", AudioProfileNames[size_t(std::clamp(s.resolvedAudioProfile, 0, 3))]);
        ImGui::EndGroup();
        ImGui::EndChild();

        ImGui::BeginChild("lab-bottom", {0, -ImGui::GetFrameHeightWithSpacing() - 6.f}, true);
        ImGui::SeparatorText("SPECTRAL HISTORY (OLDEST LEFT)");
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float rowHeight = std::max(8.f, (avail.y - ImGui::GetTextLineHeightWithSpacing() * 2.f) / 7.f);
        const float column = avail.x / float(HistoryLength);
        for (size_t band = 0; band < Bands.size(); ++band) {
            const ImU32 c = Bands[band].color;
            for (size_t i = 0; i < HistoryLength; ++i) {
                const float v = history_[band][(historyHead_ + 1 + i) % HistoryLength];
                if (v < 0.02f) continue;
                const int alpha = int(255.f * std::clamp(v, 0.f, 1.f));
                draw->AddRectFilled({p0.x + float(i) * column, p0.y + float(band) * rowHeight},
                                    {p0.x + float(i + 1) * column + 0.5f, p0.y + float(band + 1) * rowHeight - 1.f},
                                    (c & 0x00ffffff) | (ImU32(alpha) << 24));
            }
            draw->AddText({p0.x + 4.f, p0.y + float(band) * rowHeight}, IM_COL32(255, 255, 255, 140), Bands[band].name);
        }
        ImGui::Dummy({avail.x, rowHeight * 7.f});
        ImGui::TextDisabled("Output device: %s", Utf8(s.playbackDevice).c_str());
        ImGui::SameLine();
        if (ImGui::Button("NEXT DEVICE")) send(Command::CycleAudioDevice, 0, "Audio device");
        ImGui::EndChild();
        footer();
    }

    static void detectorBar(const char* name, float level, float evidence, float threshold, float confidence) {
        ImGui::Text("%s", name);
        ImGui::SameLine(70.f);
        const ImVec2 size{260.f, ImGui::GetFrameHeight() * 0.8f};
        const ImVec2 o = ImGui::GetCursorScreenPos();
        auto* d = ImGui::GetWindowDrawList();
        d->AddRectFilled(o, {o.x + size.x, o.y + size.y}, IM_COL32(20, 23, 32, 255), 4.f);
        d->AddRectFilled(o, {o.x + size.x * std::clamp(level, 0.f, 1.f), o.y + size.y}, IM_COL32(255, 90, 160, 255), 4.f);
        d->AddRectFilled({o.x, o.y + size.y * 0.7f}, {o.x + size.x * std::clamp(evidence, 0.f, 1.f), o.y + size.y}, IM_COL32(80, 210, 255, 200));
        const float tx = o.x + size.x * std::clamp(threshold, 0.f, 1.f);
        d->AddLine({tx, o.y}, {tx, o.y + size.y}, IM_COL32(255, 255, 255, 200), 2.f);
        ImGui::Dummy(size);
        ImGui::SameLine();
        ImGui::TextDisabled("%.0f%%", confidence * 100.f);
    }

    // ---- DIRECTOR --------------------------------------------------------------------------
    void tabDirector() {
        const auto& s = state_;
        auto sceneName = [](int id) -> const char* {
            return id >= 0 && id < rarestage::show::SceneCount ? rarestage::show::Scenes[size_t(id)].name : "-";
        };
        ImGui::BeginChild("director", {0, -ImGui::GetFrameHeightWithSpacing() - 6.f}, true);
        ImGui::SeparatorText("DECISION");
        ImGui::Text("Reason: %s", Utf8(s.directorReason).c_str());
        ImGui::Text("Last decision: %s", Utf8(s.lastDirectorDecision).c_str());
        ImGui::Text("Candidate: %s  (%.2f)    runner-up: %s  (%.2f)", sceneName(s.directorCandidateWorld), s.directorCandidateScore,
                    sceneName(s.directorRunnerUpWorld), s.directorRunnerUpScore);
        ImGui::Text("Prepared next: %s    predicted section: %s (%.0f%%, %.1f beats ahead)", sceneName(s.preparedWorld),
                    SectionNames[size_t(std::clamp(s.predictedSection, 0, 4))], s.predictedSectionConfidence * 100.f, s.predictionLeadBeats);
        ImGui::Text("Route: continue %s / drop %s / break %s", sceneName(s.composerContinue), sceneName(s.composerDrop), sceneName(s.composerBreak));
        ImGui::SeparatorText("GENERATIVE WORLDS");
        checkRow("GENERATIVE WORLDS: build candidates and let the Director bring them on at drops and phrase boundaries", s.generativeOn != 0, Command::SetGenerative);
        if (s.generativeOn) {
            ImGui::Text("Queue: %u pending / %u ready      submitted %u  compiled %u  failed %u  rejected %u", s.genPending, s.genReady, s.genSubmitted, s.genCompiled, s.genFailed, s.genRejected);
            ImGui::Text("Cache: %u hits / %u misses      stale %u  dropped %u", s.genCacheHits, s.genCacheMisses, s.genStale, s.genDropped);
            if (s.genLastFamily >= 0 && s.genLastFamily < rarestage::gen::FamilyCount)
                ImGui::Text("Last: %s  seed %u  hash %016llx  %.0f ms", rarestage::gen::FamilyNames[s.genLastFamily], s.genLastSeed, (unsigned long long)s.genLastHash, s.genLastCompileMs);
            ImGui::Text("Qualification: %u ready  accepted %u  over budget %u  blacklisted %u      budget %.1f ms", s.genQualified, s.genAccepted, s.genRejectedCost, s.genBlacklisted, s.genBudgetMs);
            if (s.genLastVerdict >= 0 && s.genLastVerdict <= int(rarestage::gen::Verdict::RejectNoTiming))
                ImGui::Text("Last qualified: %.2f ms at output size -> %s", s.genLastGpuMs, rarestage::gen::VerdictName(rarestage::gen::Verdict(s.genLastVerdict)));
            if (s.genLastError[0]) ImGui::TextColored({1.f, .55f, .3f, 1.f}, "Last error: %s", s.genLastError);
        }
        if (s.generativeOn) {
            static const char* guest[4] = {"waiting for a drop or phrase boundary", "arriving", "ON STAGE", "leaving"};
            ImGui::Text("Stage guest: %s%s%s  (%u visits)", guest[std::clamp(s.genLiveState, 0, 3)], s.genLiveReason[0] ? " - " : "", s.genLiveReason, s.genActivations);
            if (ImGui::Button("TAKE NEXT QUALIFIED WORLD NOW##gt")) send(Command::GenerativeTake, 1, "Take generated world");
            ImGui::SameLine();
            if (ImGui::Button("RELEASE GENERATED WORLD##gr")) send(Command::GenerativeRelease, 1, "Release generated world");
        }
        if (ImGui::Button("REPLAY LAUNCH SEQUENCE / RETURN TO STANDBY##sb")) send(Command::EnterStandby, 1, "Standby");
        ImGui::SeparatorText("PREVIEW A WORLD ON THE OUTPUT");
        ImGui::TextWrapped("Plays one generated world on the live output, driven by the music. This is an unqualified preview: it steps back by itself if it fails to compile or the GPU struggles.");
        {
            static const char* labels[4] = {"COSMIC SPACE", "CIRCUIT BOARD", "VIRUS SPREAD", "ERROR HORIZON"};
            const int active = s.previewState != 0 && s.previewFamily >= rarestage::gen::FirstFieldFamily ? s.previewFamily - rarestage::gen::FirstFieldFamily : -1;
            auto request = [&](int family) { send(Command::SetPreview, int((previewSeed_ << 3) | std::uint32_t(family + 1)), "Preview world"); };
            if (toggleButton("PREVIEW OFF##pv", s.previewState == 0, {0.35f, 0.35f, 0.40f, 1.f})) send(Command::SetPreview, 0, "Preview off");
            for (int k = 0; k < 4; ++k) {
                ImGui::SameLine();
                if (toggleButton((std::string(labels[k]) + "##pv" + std::to_string(k)).c_str(), active == k, {0.12f, 0.50f, 0.62f, 1.f})) request(k);
            }
            ImGui::Text("Seed %u", previewSeed_);
            ImGui::SameLine();
            if (ImGui::SmallButton("<##pvseed") && previewSeed_ > 1) { --previewSeed_; if (active >= 0) request(active); }
            ImGui::SameLine();
            if (ImGui::SmallButton(">##pvseed") && previewSeed_ < 1000000u) { ++previewSeed_; if (active >= 0) request(active); }
            ImGui::SameLine();
            if (ImGui::SmallButton("RANDOM##pvseed")) { previewSeed_ = 1u + (std::uint32_t(GetTickCount64()) * 2654435761u) % 100000u; if (active >= 0) request(active); }
            static const char* states[4] = {"off", "compiling...", "LIVE (unqualified preview)", "stepped back (failed or GPU too slow)"};
            ImGui::Text("State: %s%s", states[std::clamp(s.previewState, 0, 3)], s.previewState == 2 ? "" : "");
            if (s.previewState == 2) ImGui::Text("Compiled in %.0f ms%s", s.previewCompileMs, s.previewGpuMs >= 0.f ? "" : "  (measuring GPU cost...)");
            if (s.previewState == 2 && s.previewGpuMs >= 0.f) ImGui::Text("GPU cost at output size: %.2f ms  (budget %.1f ms)", s.previewGpuMs, s.genBudgetMs);
        }
        ImGui::SeparatorText("MUSICAL READ");
        ImGui::ProgressBar(s.buildTension, {320, 0}, "build tension");
        ImGui::ProgressBar(s.dropReadiness, {320, 0}, "drop readiness");
        ImGui::ProgressBar(s.musicalNovelty, {320, 0}, "novelty");
        ImGui::ProgressBar(s.songArc, {320, 0}, "song arc");
        ImGui::ProgressBar(s.sectionEvidence, {320, 0}, "section evidence");
        ImGui::ProgressBar(s.endingConfidence, {320, 0}, "ending confidence");
        ImGui::SeparatorText("CHOREOGRAPHY");
        ImGui::Text("Theme: %s    movement: %s    pattern: %s", Utf8(s.themeName).c_str(), Utf8(s.movementName).c_str(), Utf8(s.patternName).c_str());
        ImGui::Text("Intention: %s", Utf8(s.motionIntention).c_str());
        ImGui::Text("Choreography: %s    identity: %s", Utf8(s.choreographyName).c_str(), Utf8(s.identityTarget).c_str());
        ImGui::Text("Cue density %.2f (world %.2f, adaptive %.2f)    transition compatibility %.0f%%", s.cueDensity, s.worldCueDensity,
                    s.adaptiveCueDensity, s.transitionCompatibility * 100.f);
        ImGui::Text("Cues accepted %u / rejected %u    last rejected: %s", s.acceptedDirectorCueCount, s.rejectedDirectorCueCount,
                    Utf8(s.lastRejectedCue).c_str());
        ImGui::EndChild();
        footer();
    }

    // ---- OUTPUT ----------------------------------------------------------------------------
    void tabOutput() {
        const auto& s = state_;
        ImGui::BeginChild("output", {0, -ImGui::GetFrameHeightWithSpacing() - 6.f}, true);
        choiceRow("QUALITY", QualityNames, s.quality, Command::SetQuality);
        choiceRow("MASTERING", MasteringNames, s.masteringProfile, Command::SetMasteringProfile);
        choiceRow("STAGE LAYOUT", StageLayoutNames, s.stageLayout, Command::SetStageLayout);
        ImGui::Spacing();
        checkRow("VSync", flag(rarestage::control::VsyncEnabled), Command::SetVsync);
        ImGui::SameLine();
        checkRow("Spout clean output", flag(rarestage::control::SpoutEnabled), Command::SetSpout);
        ImGui::SameLine();
        checkRow("Fullscreen", flag(rarestage::control::Fullscreen), Command::SetFullscreen);
        ImGui::SameLine();
        checkRow("Stage overlay", flag(rarestage::control::StageOverlayVisible), Command::SetStageOverlay);
        ImGui::SameLine();
        checkRow("Show timeline", flag(rarestage::control::Timeline), Command::SetTimeline);
        ImGui::SameLine();
        checkRow("DJ identity", flag(rarestage::control::DjIdentityVisible), Command::SetDjIdentityVisible);
        ImGui::Text("Spout: %s", flag(rarestage::control::SpoutReady) ? "sender ready" : "not running");
        ImGui::SeparatorText("SCREENS");
        ImGui::Text("Stage on display %d of %d", s.monitor + 1, s.monitorCount);
        for (int i = 0; i < std::min(s.monitorCount, 8); ++i) {
            if (i) ImGui::SameLine();
            if (toggleButton((Utf8(s.displayNames[i]) + "##display" + std::to_string(i)).c_str(), s.monitor == i, {0.12f, 0.50f, 0.62f, 1.f}))
                send(Command::SelectMonitor, i, "Stage display");
        }
        ImGui::TextDisabled("MIRROR TO");
        for (int i = 0; i < std::min(s.monitorCount, 8); ++i) {
            ImGui::SameLine();
            bool mirrored = (s.mirrorMask >> i) & 1u;
            if (ImGui::Checkbox(("D" + std::to_string(i + 1) + "##mirror").c_str(), &mirrored))
                send(Command::SetMirrorMask, int((s.mirrorMask & ~(1u << i)) | (mirrored ? (1u << i) : 0u)), "Mirror");
        }
        ImGui::Text("Output status: %s", s.displayOutputStatus > 0 ? "OK" : s.displayOutputStatus < 0 ? "FAILED" : "idle");
        ImGui::SeparatorText("CAPTURE");
        if (ImGui::Button("CAPTURE FRAME")) send(Command::CaptureFrame, 0, "Capture");
        ImGui::SameLine();
        ImGui::Text("%u captured. Last: %s", s.captureCount, Utf8(s.lastCapturePath).c_str());
        ImGui::SeparatorText("RENDERER");
        if (ImGui::Button("Focus RareStage")) launchRenderer();
        ImGui::SameLine();
        if (ImGui::Button("Quit RareStage")) ImGui::OpenPopup("quit?");
        if (ImGui::BeginPopupModal("quit?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Close the renderer and stop the stage output?");
            if (ImGui::Button("Quit RareStage")) { send(Command::QuitRenderer, 0, "Quit"); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::EndChild();
        footer();
    }

    // ---- PRODUCTION ------------------------------------------------------------------------
    void tabProduction() {
        const auto& s = state_;
        ImGui::BeginChild("production", {0, -ImGui::GetFrameHeightWithSpacing() - 6.f}, true);
        choiceRow("DUTY", DutyNames, s.productionDuty, Command::ApplyProductionDuty);
        choiceRow("TEMPO", TempoAnchorNames, s.tempoAnchor, Command::SetTempoAnchor);
        ImGui::Text("Effective tempo anchor: %d", s.effectiveTempoAnchor);
        ImGui::SeparatorText("NETWORK SYNC");
        choiceRow("MODE", SyncNames, s.syncMode, Command::SetSyncMode);
        commandSlider("Sync group", s.syncGroup, 1, 99, Command::SetSyncGroup, 0, "%d", 1);
        ImGui::Text("%s", Utf8(s.syncStatus).c_str());
        ImGui::Text("Phase error %.1f ms    packet age %.0f ms    sequence %u", s.syncPhaseErrorMs, s.syncPacketAgeMs, s.syncSequence);
        ImGui::SeparatorText("DJ IDENTITY");
        ImGui::SetNextItemWidth(260.f);
        if (!djLoaded_ && connected_) {
            std::snprintf(djName_, sizeof(djName_), "%s", Utf8(s.djName).c_str());
            djLoaded_ = true;
        }
        ImGui::InputText("##dj", djName_, 25);
        ImGui::SameLine();
        if (ImGui::Button("APPLY NAME")) applyDjName();
        ImGui::EndChild();
        footer();
    }

    static constexpr size_t HistoryLength = 360;

    HWND window_{};
    ID3D11Device* device_{};
    ID3D11DeviceContext* context_{};
    IDXGISwapChain* swap_{};
    ID3D11RenderTargetView* rtv_{};

    HANDLE mapping_{};
    const SharedState* mapped_{};
    SharedState state_{};
    HWND renderer_{};
    HANDLE rendererProcess_{};
    bool connected_{}, compatible_{}, djLoaded_{};
    std::uint32_t serial_{}, pendingSerial_{}, previewSeed_{1};
    rarestage::console::Splash splash_;
    std::uint64_t lastRevision_{};
    std::string status_ = "Waiting for RareStage...";
    std::string pendingText_;

    std::unordered_map<ImGuiID, int> pending_;
    std::unordered_map<ImGuiID, std::chrono::steady_clock::time_point> lastSend_;
    char search_[64]{};
    char djName_[32]{};
    int category_{};

    std::array<std::array<float, HistoryLength>, 7> history_{};
    std::array<float, HistoryLength> kickHistory_{}, snareHistory_{}, hatHistory_{};
    size_t historyHead_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int show) {
    const bool startupTest = commandLine && std::wcsstr(commandLine, L"--startup-test") != nullptr;
    Console console;
    const bool withSplash = !startupTest && !(commandLine && std::wcsstr(commandLine, L"--no-splash"));
    if (!console.init(instance, startupTest ? SW_HIDE : show, withSplash)) {
        MessageBoxW(nullptr, L"RareStage Console could not initialize DirectX.", L"RareStage Console", MB_OK | MB_ICONERROR);
        return 1;
    }
    return console.run(startupTest);
}

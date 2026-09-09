#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>

#include "rarestage_control.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

using rarestage::control::Command;
using rarestage::control::CommandStatus;
using rarestage::control::SharedState;

constexpr COLORREF Background = RGB(3, 8, 15);
constexpr COLORREF Panel = RGB(8, 18, 29);
constexpr COLORREF PanelRaised = RGB(12, 27, 42);
constexpr COLORREF Line = RGB(27, 60, 79);
constexpr COLORREF Text = RGB(232, 249, 255);
constexpr COLORREF Muted = RGB(105, 139, 155);
constexpr COLORREF Cyan = RGB(21, 222, 244);
constexpr COLORREF Green = RGB(108, 246, 163);
constexpr COLORREF Purple = RGB(157, 101, 255);
constexpr COLORREF Amber = RGB(255, 188, 65);
constexpr COLORREF Red = RGB(255, 55, 91);

COLORREF blendColor(COLORREF from, COLORREF to, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    const auto channel = [amount](BYTE a, BYTE b) {
        return static_cast<BYTE>(std::lround(a + (b - a) * amount));
    };
    return RGB(channel(GetRValue(from), GetRValue(to)),
               channel(GetGValue(from), GetGValue(to)),
               channel(GetBValue(from), GetBValue(to)));
}

const auto WorldNames=[](){std::array<const wchar_t*,rarestage::control::WorldCount> names{};for(size_t i=0;i<names.size();++i)names[i]=rarestage::show::Scenes[i].wideName;return names;}();
const auto WorldKeys=[](){std::array<std::wstring,rarestage::control::WorldCount> keys{};for(size_t i=0;i<keys.size();++i)keys[i]=rarestage::show::Scenes[i].key?std::wstring(1,wchar_t(rarestage::show::Scenes[i].key)):L"";return keys;}();
constexpr std::array<const wchar_t*, rarestage::control::SectionCount> SectionNames{
    L"OPEN", L"BUILD", L"DROP", L"BREAKDOWN", L"DRIVE"
};
constexpr std::array<const wchar_t*, rarestage::control::ProfileCount> ProfileNames{
    L"CALM", L"BALANCED", L"STAGE", L"AGGRESSIVE"
};
constexpr std::array<const wchar_t*, rarestage::control::PaletteCount> PaletteNames{
    L"RARE CYAN", L"NEBULA", L"SAKURA", L"SOLAR", L"LASER LIME", L"CORAL ICE", L"ULTRAVIOLET", L"ELECTRIC GOLD"
};
constexpr std::array<const wchar_t*, rarestage::control::PresetCount> PresetNames{
    L"CLEAN OPEN", L"MELODIC BLOOM", L"RARE DROP", L"GLITCH FINALE"
};
constexpr std::array<const wchar_t*, rarestage::control::TransitionStyleCount> TransitionNames{
    L"ROW FLOW", L"RIBBON FLOW", L"ORBIT FLOW", L"DEPTH FOLD"
};
constexpr std::array<const wchar_t*, rarestage::control::QualityCount> QualityNames{
    L"PERFORMANCE", L"BALANCED", L"NATIVE HD", L"ULTRA 4K"
};
constexpr std::array<const wchar_t*, rarestage::control::AudioProfileCount> AudioProfileNames{
    L"AUTO", L"EDM", L"BASS MUSIC", L"MELODIC"
};
constexpr std::array<const wchar_t*, rarestage::control::MasteringProfileCount> MasteringNames{
    L"STUDIO", L"LED SAFE", L"ARENA", L"BROADCAST"
};
constexpr std::array<const wchar_t*, rarestage::control::StageLayoutCount> StageLayoutNames{
    L"FULL FRAME", L"ULTRAWIDE", L"TRIPTYCH", L"LED FACADE"
};
constexpr std::array<const wchar_t*, rarestage::control::ProductionDutyCount> ProductionDutyNames{
    L"OPEN", L"SUPPORT", L"PEAK", L"BREAKDOWN", L"CLOSING"
};
constexpr std::array<const wchar_t*, rarestage::control::SyncModeCount> SyncModeNames{
    L"LOCAL", L"LEADER", L"FOLLOWER"
};
constexpr std::array<const wchar_t*, rarestage::control::SceneCategoryCount> SceneCategoryNames{
    L"AUTO", L"PANEL / PIXEL", L"RIBBON / GLASS", L"DEPTH", L"ATMOSPHERE"
};
constexpr std::array<const wchar_t*, rarestage::control::TempoAnchorCount> TempoAnchorNames{
    L"AUTO CLUB", L"128 BPM", L"130 BPM"
};

enum class Page { Show, Mix, Audio, Output, Production, Rehearsal, Screens };

enum class Setting {
    Sensitivity,
    Bass,
    Mid,
    High,
    Beat,
    Logo,
    Dwell,
    Transition,
    CueDensity,
    MasterBrightness,
    SyncGroup
};

enum class Action {
    None,
    Launch,
    Page,
    World,
    Section,
    ClearSection,
    Previous,
    Next,
    Automatic,
    Freeze,
    HoldTheme,
    NextTheme,
    Blackout,
    CleanOpen,
    Fallback,
    LoadPreset,
    SavePreset,
    StageOverlay,
    Profile,
    Palette,
    TransitionStyle,
    Quality,
    Adjust,
    Timeline,
    Vsync,
    Spout,
    AudioProfile,
    ExportTrace,
    LabelTrace,
    CaptureFrame,
    ToggleRehearsal,
    ToggleSmartCapture,
    ReviewPrevious,
    ReviewNext,
    ReviewApprove,
    ReviewRework,
    ReviewTag,
    ExportSession,
    AudioDevice,
    Monitor,
    SelectDisplay,
    ToggleMirror,
    MirrorAll,
    MirrorOff,
    Fullscreen,
    MasteringProfile,
    StageLayout,
    ProductionDuty,
    SyncMode,
    EditDjName,
    SceneCategory,
    TempoAnchor,
    DjIdentity,
    QuitStage
};

struct HitTarget {
    RECT rect{};
    Action action{Action::None};
    int value{};
};

struct TraceSample {
    float bass{}, mid{}, high{}, kick{}, snare{}, hats{}, onset{};
    float kickEvidence{}, snareEvidence{}, hatEvidence{}, ambiguity{}, threshold{}, stability{};
    float lowFluxShare{}, midFluxShare{}, highFluxShare{}, lowPowerShare{}, midPowerShare{}, highPowerShare{};
    float bpm{}, tempoConfidence{}, latencyMs{}, sampleRate{};
    std::uint32_t transientClass{}, acceptedKicks{}, acceptedSnares{}, acceptedHats{}, rejected{};
    int requestedProfile{}, resolvedProfile{}, world{}, section{};
    int expectedClass{-1};
};

struct RehearsalSample {
    std::uint64_t timeMs{};
    int world{}, section{}, predictedSection{}, preparedWorld{}, masteringProfile{}, stageLayout{}, productionDuty{}, syncMode{}, syncGroup{}, palette{}, quality{};
    std::uint32_t transientClass{}, flags{}, directorDecisionRevision{};
    float bpm{}, tempoConfidence{}, bass{}, mid{}, high{}, kick{}, snare{}, hats{}, onset{}, energy{};
    float gpuMs{}, gpuBudgetMs{}, cueDensity{}, worldCueDensity{}, predictionConfidence{}, predictionLeadBeats{}, adaptiveDensity{}, tempoRecovery{}, masterBrightness{}, syncPacketAge{};
};

struct RehearsalEvent {
    std::uint64_t timeMs{};
    std::wstring kind;
    std::wstring detail;
};

struct ReviewItem {
    std::uint64_t timeMs{};
    std::wstring path;
    std::wstring reason;
    std::wstring tag{L"UNTAGGED"};
    int world{}, section{}, status{};
};

class ConsoleApp {
public:
    ~ConsoleApp() {
        if (rehearsalActive_) finishRehearsal(false);
        disconnect();
        closeRendererProcessHandle();
        destroyFonts();
    }

    bool initialize(HINSTANCE instance) {
        WNDCLASSW windowClass{};
        windowClass.hInstance = instance;
        windowClass.lpfnWndProc = WindowProc;
        windowClass.lpszClassName = L"RareStageConsoleWindow";
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = nullptr;
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

        window_ = CreateWindowExW(
            0, windowClass.lpszClassName, L"RareStage Console 0.23.0",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, 1380, 900,
            nullptr, nullptr, instance, this);
        if (!window_) return false;

        const BOOL dark = TRUE;
        DwmSetWindowAttribute(window_, 20, &dark, sizeof(dark));
        const DWORD roundedCorners = 2;
        const DWORD backdrop = 2;
        DwmSetWindowAttribute(window_, 33, &roundedCorners, sizeof(roundedCorners));
        DwmSetWindowAttribute(window_, 38, &backdrop, sizeof(backdrop));
        dpi_ = GetDpiForWindow(window_);
        createFonts();
        SetTimer(window_, 1, 33, nullptr);
        if(!std::wcsstr(GetCommandLineW(),L"--screen-ui-test"))ShowWindow(window_, SW_SHOW);
        UpdateWindow(window_);
        return true;
    }

    int run() {
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

    void requestClose() {
        PostMessageW(window_, WM_CLOSE, 0, 0);
    }

    void beginConnectedSoakTest() {
        soakTest_ = true;
        soakStartedAt_ = GetTickCount64();
    }

    void beginLaunchStageSoakTest() {
        beginConnectedSoakTest();
        closeStageAfterSoak_ = true;
        launchRenderer();
    }

    bool captureScreenUiTest(){
        const UINT oldDpi=dpi_;destroyFonts();dpi_=96;createFonts();connected_=true;state_.monitorCount=8;state_.monitor=0;state_.mirrorMask=6;state_.mirrorCount=2;state_.displayOutputStatus=1;
        for(int n=0;n<8;++n)swprintf_s(state_.displayNames[n],L"DISPLAY %d  /  %d x %d",n+1,n==1?1080:1920,n==1?1920:1080);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1280;info.bmiHeader.biHeight=-640;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void*pixels=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bitmap||!pixels){if(bitmap)DeleteObject(bitmap);DeleteDC(dc);return false;}
        const auto old=SelectObject(dc,bitmap);fillRect(dc,RECT{0,0,1280,640},Background);targets_.clear();paintScreens(dc,RECT{12,12,1268,628});GdiFlush();
        bool ok=true;for(const auto&target:targets_)if(target.rect.left<12||target.rect.right>1268||target.rect.top<12||target.rect.bottom>628||target.rect.right<=target.rect.left||target.rect.bottom<=target.rect.top)ok=false;
        BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+1280*640*4;
        std::ofstream output("RareStage-screens-ui.bmp",std::ios::binary);output.write(reinterpret_cast<const char*>(&file),sizeof(file));output.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));output.write(static_cast<const char*>(pixels),1280*640*4);ok=ok&&output.good();
        fillRect(dc,RECT{0,0,1280,640},Background);targets_.clear();paintShow(dc,RECT{12,12,1268,628});GdiFlush();
        int worldButtons=0;for(const auto&target:targets_)if(target.action==Action::World){++worldButtons;if(target.value<0||target.value>=int(WorldNames.size())||target.rect.bottom>628||target.rect.top<12||target.rect.right>1268||target.rect.bottom<=target.rect.top)ok=false;}
        ok=ok&&worldButtons==int(WorldNames.size());std::ofstream showOutput("RareStage-worlds-ui.bmp",std::ios::binary);showOutput.write(reinterpret_cast<const char*>(&file),sizeof(file));showOutput.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));showOutput.write(static_cast<const char*>(pixels),1280*640*4);ok=ok&&showOutput.good();
        SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);connected_=false;destroyFonts();dpi_=oldDpi;createFonts();return ok;
    }

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        ConsoleApp* app = reinterpret_cast<ConsoleApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            app = reinterpret_cast<ConsoleApp*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        return app ? app->handleMessage(window, message, wParam, lParam)
                   : DefWindowProcW(window, message, wParam, lParam);
    }

    LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
            case WM_TIMER:
                updateAnimation();
                updateConnection();
                InvalidateRect(window, nullptr, FALSE);
                updateSoakTest();
                return 0;
            case WM_PAINT:
                paint();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_LBUTTONUP:
                activate(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
                return 0;
            case WM_MOUSEMOVE: {
                cursor_ = POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (!trackingMouse_) {
                    TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0};
                    TrackMouseEvent(&tracking);
                    trackingMouse_ = true;
                }
                return 0;
            }
            case WM_MOUSELEAVE:
                cursor_ = POINT{-10000, -10000};
                trackingMouse_ = false;
                return 0;
            case WM_SYSKEYDOWN:
                if(rarestage::control::IsFullscreenShortcut(message,wParam,lParam)){send(Command::SetFullscreen,flag(rarestage::control::Fullscreen)?0:1,L"Stage fullscreen");return 0;}
                break;
            case WM_SYSCHAR:if(wParam==VK_RETURN)return 0;break;
            case WM_KEYDOWN:
                if(rarestage::control::IsFullscreenShortcut(message,wParam,lParam)){send(Command::SetFullscreen,flag(rarestage::control::Fullscreen)?0:1,L"Stage fullscreen");return 0;}
                handleKey(wParam);
                return 0;
            case WM_CHAR:
                handleCharacter(static_cast<wchar_t>(wParam));
                return 0;
            case WM_DPICHANGED: {
                dpi_ = HIWORD(wParam);
                destroyFonts();
                createFonts();
                const auto* suggested = reinterpret_cast<RECT*>(lParam);
                SetWindowPos(window, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left, suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
            case WM_GETMINMAXINFO: {
                auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
                info->ptMinTrackSize.x = scale(1120);
                info->ptMinTrackSize.y = scale(860);
                return 0;
            }
            case WM_SETCURSOR: {
                POINT point{};
                GetCursorPos(&point);
                ScreenToClient(window, &point);
                for (const auto& target : targets_) {
                    if (PtInRect(&target.rect, point)) {
                        SetCursor(LoadCursorW(nullptr, IDC_HAND));
                        return TRUE;
                    }
                }
                break;
            }
            case WM_DESTROY:
                KillTimer(window, 1);
                disconnect();
                PostQuitMessage(exitCode_);
                return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    int scale(int value) const {
        return MulDiv(value, static_cast<int>(dpi_), 96);
    }

    void updateAnimation() {
        const std::uint64_t now = GetTickCount64();
        if (!animationTick_) animationTick_ = now;
        const float dt = std::clamp(static_cast<float>(now - animationTick_) * .001f, 0.0f, .10f);
        animationTick_ = now;
        animationTime_ += dt;
        pageTransition_ = std::min(1.0f, pageTransition_ + dt * 5.2f);
        const float connectedTarget = connected_ ? 1.0f : 0.0f;
        connectionEase_ += (connectedTarget - connectionEase_) * std::min(1.0f, dt * 6.0f);
    }

    void selectPage(Page next) {
        if (next == page_) return;
        page_ = next;
        pageTransition_ = 0.0f;
    }

    HFONT makeFont(int points, int weight) const {
        return CreateFontW(-MulDiv(points, static_cast<int>(dpi_), 72), 0, 0, 0, weight,
                           FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }

    void createFonts() {
        titleFont_ = makeFont(24, FW_BOLD);
        headingFont_ = makeFont(11, FW_BOLD);
        bodyFont_ = makeFont(9, FW_SEMIBOLD);
        smallFont_ = makeFont(8, FW_SEMIBOLD);
    }

    void destroyFonts() {
        for (HFONT* font : {&titleFont_, &headingFont_, &bodyFont_, &smallFont_}) {
            if (*font) DeleteObject(*font);
            *font = nullptr;
        }
    }

    void disconnect() {
        connected_ = false;
        compatible_ = false;
        rendererWindow_ = nullptr;
        if (mappedState_) {
            UnmapViewOfFile(mappedState_);
            mappedState_ = nullptr;
        }
        if (mapping_) {
            CloseHandle(mapping_);
            mapping_ = nullptr;
        }
        state_ = SharedState{};
    }

    void closeRendererProcessHandle() {
        if (rendererProcess_) {
            CloseHandle(rendererProcess_);
            rendererProcess_ = nullptr;
        }
    }

    void pollRendererProcess() {
        if (!rendererProcess_) return;
        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(rendererProcess_, &exitCode) || exitCode == STILL_ACTIVE) return;
        closeRendererProcessHandle();
        launchFocusPending_ = false;
        if (!rendererWindow_) {
            statusText_ = L"RareStage exited with code " + std::to_wstring(exitCode) + L".";
        }
    }

    void updateConnection() {
        pollRendererProcess();
        if (rendererWindow_ && !IsWindow(rendererWindow_)) disconnect();

        if (!mapping_) {
            mapping_ = OpenFileMappingW(FILE_MAP_READ, FALSE, rarestage::control::MappingName);
            if (mapping_) {
                mappedState_ = static_cast<const SharedState*>(
                    MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, sizeof(SharedState)));
                if (!mappedState_) {
                    CloseHandle(mapping_);
                    mapping_ = nullptr;
                }
            }
        }

        SharedState next{};
        if (!mappedState_ || !rarestage::control::ReadConsistent(mappedState_, next)) {
            connected_ = false;
            if (rendererWindow_ && !mapping_) {
                statusText_ = L"Launch the matching v0.18 Stage and Console pair.";
            }
            return;
        }

        // Bind commands to the process owning this telemetry, even when an older Stage is open.
        struct Search {DWORD process;HWND window;};Search search{next.rendererProcessId,nullptr};
        EnumWindows([](HWND window,LPARAM data)->BOOL{auto*search=reinterpret_cast<Search*>(data);DWORD process=0;GetWindowThreadProcessId(window,&process);wchar_t name[64]{};GetClassNameW(window,name,64);if(process==search->process&&std::wcscmp(name,rarestage::control::RendererWindowClass)==0){search->window=window;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&search));
        rendererWindow_=search.window;if(!rendererWindow_){connected_=false;return;}
        const std::uint64_t now = GetTickCount64();
        const auto age = now >= next.heartbeatTickMs ? now - next.heartbeatTickMs : 0;
        if (age > 1800) {
            disconnect();
            statusText_ = L"Renderer heartbeat lost. Waiting for RareStage to restart.";
            return;
        }

        state_ = next;
        compatible_ = next.protocolVersion == rarestage::control::ProtocolVersion &&
                      next.structSize == sizeof(SharedState) &&
                      next.worldCount == rarestage::control::WorldCount &&
                      next.sectionCount == rarestage::control::SectionCount;
        connected_ = compatible_ && (next.flags & rarestage::control::RendererReady);
        if (connected_ && next.stateRevision != lastTraceRevision_) {
            lastTraceRevision_ = next.stateRevision;
            trace_.push_back({next.bass, next.mid, next.high, next.kick, next.snare, next.percussion, next.onset,
                              next.kickEvidence, next.snareEvidence, next.hatEvidence, next.transientAmbiguity,
                              next.detectorThreshold, next.calibrationStability, next.lowFluxShare, next.midFluxShare,
                              next.highFluxShare, next.lowPowerShare, next.midPowerShare, next.highPowerShare,
                              next.bpm, next.tempoConfidence,
                              next.analysisLatencyMs, next.sampleRate, next.transientClass, next.acceptedKickCount,
                              next.acceptedSnareCount, next.acceptedHatCount, next.rejectedTransientCount,
                              next.audioProfile, next.resolvedAudioProfile, next.world, next.songSection, -1});
            constexpr std::size_t MaxTraceSamples = 1200;
            if (trace_.size() > MaxTraceSamples) {
                trace_.erase(trace_.begin(), trace_.begin() + (trace_.size() - MaxTraceSamples));
            }
            updateRehearsal(next, now);
        }
        if (connected_ && !everConnected_) {
            everConnected_ = true;
            connectedAt_ = now;
        }
        if (connected_ && launchFocusPending_) {
            // RareStage is a stage-output surface; Console is the operator
            // surface. Put the operator back in control after the child has
            // finished creating its window and telemetry endpoint.
            ShowWindow(window_, SW_RESTORE);
            BringWindowToTop(window_);
            SetForegroundWindow(window_);
            launchFocusPending_ = false;
        }

        if (!compatible_) {
            statusText_ = L"Protocol mismatch. Install RareStage and Console from the same package.";
        } else if (pendingSerial_ && next.lastCommandSerial == pendingSerial_) {
            statusText_ = pendingDescription_ +
                (next.lastCommandStatus == CommandStatus::Accepted ? L" acknowledged." : L" rejected.");
            pendingSerial_ = 0;
        } else if (pendingSerial_ && now - pendingSentAt_ > 1500) {
            statusText_ = pendingDescription_ + L" timed out.";
            pendingSerial_ = 0;
        } else if (connected_ && statusText_.empty()) {
            statusText_ = L"Console connected. All program output remains isolated from this window.";
        }
    }

    void updateSoakTest() {
        if (!soakTest_) return;
        const std::uint64_t now = GetTickCount64();
        if (everConnected_ && paintCount_ >= 20 && now - connectedAt_ >= 2500) {
            if (closeStageAfterSoak_ && rendererWindow_) {
                PostMessageW(rendererWindow_, rarestage::control::CommandMessage,
                             static_cast<WPARAM>(Command::QuitRenderer),
                             rarestage::control::PackCommand(0, ++commandSerial_));
            }
            requestClose();
            soakTest_ = false;
        } else if (now - soakStartedAt_ >= 15000) {
            exitCode_ = 2;
            requestClose();
            soakTest_ = false;
        }
    }

    bool flag(rarestage::control::StateFlag value) const {
        return (state_.flags & value) != 0;
    }

    void send(Command command, int value, const std::wstring& description) {
        if (!connected_ || !rendererWindow_) {
            statusText_ = L"RareStage is offline. Launch the renderer before sending cues.";
            return;
        }
        const std::uint32_t serial = ++commandSerial_;
        if (!PostMessageW(rendererWindow_, rarestage::control::CommandMessage,
                          static_cast<WPARAM>(command), rarestage::control::PackCommand(value, serial))) {
            statusText_ = L"Windows rejected the control message.";
            return;
        }
        pendingSerial_ = serial;
        pendingSentAt_ = GetTickCount64();
        pendingDescription_ = description;
        statusText_ = pendingDescription_ + L" sent...";
    }

    void launchRenderer() {
        if (rendererWindow_ && !IsWindow(rendererWindow_)) disconnect();
        if (rendererWindow_ && IsWindow(rendererWindow_)) {
            ShowWindow(rendererWindow_, SW_RESTORE);
            SetForegroundWindow(rendererWindow_);
            statusText_ = L"RareStage focused.";
            return;
        }

        if (rendererProcess_) {
            DWORD exitCode = STILL_ACTIVE;
            if (GetExitCodeProcess(rendererProcess_, &exitCode) && exitCode == STILL_ACTIVE) {
                statusText_ = L"RareStage is still starting...";
                return;
            }
            closeRendererProcessHandle();
        }

        wchar_t module[MAX_PATH]{};
        GetModuleFileNameW(nullptr, module, MAX_PATH);
        const auto directory = std::filesystem::path(module).parent_path();
        const auto executable = directory / L"RareStage.exe";
        std::error_code fileError;
        if (!std::filesystem::exists(executable, fileError) || fileError) {
            statusText_ = L"RareStage.exe was not found beside RareStageConsole.exe.";
            return;
        }

        SHELLEXECUTEINFOW execute{};
        execute.cbSize = sizeof(execute);
        execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
        execute.hwnd = window_;
        execute.lpVerb = L"open";
        execute.lpFile = executable.c_str();
        execute.lpDirectory = directory.c_str();
        execute.nShow = SW_SHOWNOACTIVATE;
        if (ShellExecuteExW(&execute) && execute.hProcess) {
            rendererProcess_ = execute.hProcess;
            launchFocusPending_ = true;
            statusText_ = L"RareStage is starting. Console will remain in operator focus.";
        } else {
            statusText_ = L"RareStage could not be launched. Windows error " +
                          std::to_wstring(GetLastError()) + L".";
        }
    }

    void exportTrace() {
        if (trace_.empty()) {
            statusText_ = L"No live signal history is available yet.";
            return;
        }
        wchar_t profilePath[MAX_PATH]{};
        std::filesystem::path directory;
        if (GetEnvironmentVariableW(L"USERPROFILE", profilePath, MAX_PATH) > 0) {
            directory = std::filesystem::path(profilePath) / L"Documents" / L"RareStage";
        } else {
            wchar_t module[MAX_PATH]{};
            GetModuleFileNameW(nullptr, module, MAX_PATH);
            directory = std::filesystem::path(module).parent_path();
        }
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        SYSTEMTIME time{};
        GetLocalTime(&time);
        wchar_t filename[96]{};
        swprintf_s(filename, L"RareStage-calibration-%04u%02u%02u-%02u%02u%02u.csv",
                   time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
        const auto path = directory / filename;
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            statusText_ = L"Could not write the calibration trace.";
            return;
        }
        file << "sample,time_ms,requested_profile,resolved_profile,sample_rate,world,section,transient_class,expected_class,"
                "bass,mids,highs,kick,snare,hats,onset,kick_evidence,snare_evidence,hat_evidence,"
                "ambiguity,acceptance_threshold,calibration_stability,low_flux_share,mid_flux_share,high_flux_share,"
                "low_power_share,mid_power_share,high_power_share,bpm,tempo_confidence,analysis_latency_ms,"
                "accepted_kicks,accepted_snares,accepted_hats,rejected_transients\n";
        for (std::size_t index = 0; index < trace_.size(); ++index) {
            const auto& sample = trace_[index];
            file << index << ',' << index * 25 << ',' << sample.requestedProfile << ',' << sample.resolvedProfile << ','
                 << sample.sampleRate << ',' << sample.world << ',' << sample.section << ',' << sample.transientClass << ','
                 << sample.expectedClass << ','
                 << sample.bass << ',' << sample.mid << ',' << sample.high << ',' << sample.kick << ','
                 << sample.snare << ',' << sample.hats << ',' << sample.onset << ',' << sample.kickEvidence << ','
                 << sample.snareEvidence << ',' << sample.hatEvidence << ',' << sample.ambiguity << ','
                 << sample.threshold << ',' << sample.stability << ',' << sample.lowFluxShare << ','
                 << sample.midFluxShare << ',' << sample.highFluxShare << ',' << sample.lowPowerShare << ','
                 << sample.midPowerShare << ',' << sample.highPowerShare << ',' << sample.bpm << ',' << sample.tempoConfidence << ','
                 << sample.latencyMs << ',' << sample.acceptedKicks << ',' << sample.acceptedSnares << ','
                 << sample.acceptedHats << ',' << sample.rejected << '\n';
        }
        statusText_ = L"Calibration trace saved to " + path.wstring();
    }

    void labelTrace(int expectedClass) {
        if (trace_.empty()) {
            statusText_ = L"No live transient is available to label.";
            return;
        }
        trace_.back().expectedClass = std::clamp(expectedClass, 0, 3);
        static constexpr std::array<const wchar_t*,4> Labels{L"NONE", L"KICK", L"SNARE", L"HI-HAT"};
        statusText_ = L"Latest detector sample labeled " + std::wstring(Labels[trace_.back().expectedClass]) + L" for replay analysis.";
    }

    static std::string narrow(const std::wstring& value) {
        return std::string(value.begin(), value.end());
    }

    static std::string csv(const std::wstring& value) {
        std::string result = narrow(value);
        std::string escaped;
        for (char character : result) {
            if (character == '"') escaped += "\"\"";
            else escaped += character;
        }
        return '"' + escaped + '"';
    }

    static std::filesystem::path sessionDirectory() {
        wchar_t profilePath[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"USERPROFILE", profilePath, MAX_PATH) > 0) {
            return std::filesystem::path(profilePath) / L"Documents" / L"RareStage" / L"Sessions";
        }
        wchar_t module[MAX_PATH]{};
        GetModuleFileNameW(nullptr, module, MAX_PATH);
        return std::filesystem::path(module).parent_path() / L"Sessions";
    }

    static std::wstring clockText(std::uint64_t milliseconds) {
        const auto totalSeconds = milliseconds / 1000;
        wchar_t text[32]{};
        swprintf_s(text, L"%02llu:%02llu", totalSeconds / 60, totalSeconds % 60);
        return text;
    }

    void addRehearsalEvent(const std::wstring& kind, const std::wstring& detail) {
        if (!rehearsalActive_) return;
        rehearsalEvents_.push_back({GetTickCount64() - rehearsalStartedAt_, kind, detail});
    }

    void startRehearsal() {
        if (!connected_) {
            statusText_ = L"Launch RareStage before starting a rehearsal session.";
            return;
        }
        rehearsalSamples_.clear();
        rehearsalEvents_.clear();
        reviewItems_.clear();
        reviewIndex_ = 0;
        rehearsalStartedAt_ = GetTickCount64();
        rehearsalActive_ = true;
        lastRehearsalWorld_ = state_.world;
        lastRehearsalSection_ = state_.songSection;
        lastRehearsalCaptureCount_ = state_.captureCount;
        lastDirectorDecisionRevision_ = state_.directorDecisionRevision;
        lastAcceptedDirectorCueCount_ = state_.acceptedDirectorCueCount;
        lastRejectedDirectorCueCount_ = state_.rejectedDirectorCueCount;
        lastGpuUtilization_ = state_.gpuBudgetUtilization;
        smartCaptureDue_ = smartCaptureDeadline_ = 0;
        captureReasonInFlight_.clear();
        SYSTEMTIME time{};
        GetLocalTime(&time);
        wchar_t stamp[40]{};
        swprintf_s(stamp, L"%04u%02u%02u-%02u%02u%02u", time.wYear, time.wMonth, time.wDay,
                   time.wHour, time.wMinute, time.wSecond);
        sessionStamp_ = stamp;
        addRehearsalEvent(L"SESSION", L"Recording started");
        send(Command::SetRehearsalActive, 1, L"Rehearsal recorder");
    }

    void scheduleSmartCapture(const std::wstring& reason, std::uint64_t now, std::uint64_t delayMs = 900) {
        if (!rehearsalActive_ || !smartCapture_) return;
        if (now - lastSmartCaptureAt_ < 3500) return;
        if (!smartCaptureDue_) {
            smartCaptureDue_ = now + delayMs;
            smartCaptureDeadline_ = now + delayMs + 3000;
            pendingCaptureReason_ = reason;
        }
    }

    void updateRehearsal(const SharedState& next, std::uint64_t now) {
        if (!rehearsalActive_) return;
        const std::uint64_t timeMs = now - rehearsalStartedAt_;
        rehearsalSamples_.push_back({timeMs, next.world, next.songSection, next.predictedSection, next.preparedWorld, next.masteringProfile, next.stageLayout, next.productionDuty, next.syncMode, next.syncGroup, next.directorPalette, next.quality,
            next.transientClass, next.flags, next.directorDecisionRevision, next.bpm, next.tempoConfidence, next.bass, next.mid, next.high,
            next.kick, next.snare, next.percussion, next.onset, next.energy, next.gpuFrameMs, next.gpuBudgetMs,
            next.cueDensity, next.worldCueDensity, next.predictedSectionConfidence, next.predictionLeadBeats,
            next.adaptiveCueDensity, next.tempoRecoveryConfidence, next.masterBrightness, next.syncPacketAgeMs});
        constexpr std::size_t MaxSessionSamples = 144000;
        if (rehearsalSamples_.size() > MaxSessionSamples) rehearsalSamples_.erase(rehearsalSamples_.begin(), rehearsalSamples_.begin() + 1200);

        if (next.world != lastRehearsalWorld_) {
            addRehearsalEvent(L"WORLD", std::wstring(WorldNames[std::clamp(next.world, 0, int(rarestage::control::WorldCount)-1)]) + L" entered");
            scheduleSmartCapture(L"world change", now, 1100);
            lastRehearsalWorld_ = next.world;
        }
        if (next.songSection != lastRehearsalSection_) {
            addRehearsalEvent(L"ACT", std::wstring(SectionNames[std::clamp(next.songSection, 0, 4)]) + L" detected");
            scheduleSmartCapture(L"act change", now, 850);
            lastRehearsalSection_ = next.songSection;
        }
        if (next.directorDecisionRevision != lastDirectorDecisionRevision_) {
            const bool accepted = next.acceptedDirectorCueCount != lastAcceptedDirectorCueCount_ ||
                                  (next.lastDirectorDecision[0] && next.rejectedDirectorCueCount == lastRejectedDirectorCueCount_);
            addRehearsalEvent(accepted ? L"DIRECTOR" : L"REJECT", accepted ? std::wstring(next.lastDirectorDecision) : std::wstring(next.lastRejectedCue));
            lastDirectorDecisionRevision_ = next.directorDecisionRevision;
            lastAcceptedDirectorCueCount_ = next.acceptedDirectorCueCount;
            lastRejectedDirectorCueCount_ = next.rejectedDirectorCueCount;
        }
        if (next.gpuBudgetUtilization > 1.05f && lastGpuUtilization_ <= 1.05f && now - lastGpuCaptureAt_ > 15000) {
            addRehearsalEvent(L"GPU", L"Frame budget exceeded");
            scheduleSmartCapture(L"GPU budget spike", now, 1400);
            lastGpuCaptureAt_ = now;
        }
        lastGpuUtilization_ = next.gpuBudgetUtilization;

        if (next.captureCount != lastRehearsalCaptureCount_) {
            if (next.lastCapturePath[0]) {
                ReviewItem item{timeMs, next.lastCapturePath,
                    captureReasonInFlight_.empty() ? L"manual capture" : captureReasonInFlight_, L"UNTAGGED",
                    next.world, next.songSection, 0};
                reviewItems_.push_back(std::move(item));
                reviewIndex_ = reviewItems_.size() - 1;
                addRehearsalEvent(L"CAPTURE", reviewItems_.back().reason);
            }
            lastRehearsalCaptureCount_ = next.captureCount;
            captureReasonInFlight_.clear();
        }

        if (smartCaptureDue_ && now >= smartCaptureDue_ && now - lastSmartCaptureAt_ >= 3500 &&
            (next.gpuBudgetUtilization < .90f || now >= smartCaptureDeadline_)) {
            captureReasonInFlight_ = pendingCaptureReason_;
            send(Command::CaptureFrame, 0, L"Rehearsal smart capture");
            lastSmartCaptureAt_ = now;
            smartCaptureDue_ = smartCaptureDeadline_ = 0;
            pendingCaptureReason_.clear();
        }
    }

    bool saveRehearsalSession() {
        if (rehearsalSamples_.empty()) return false;
        const auto directory = sessionDirectory();
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) return false;
        const auto base = directory / (L"RareStage-session-" + sessionStamp_);
        const auto timelinePath = std::filesystem::path(base.wstring() + L"-timeline.csv");
        const auto reviewPath = std::filesystem::path(base.wstring() + L"-review.csv");
        const auto summaryPath = std::filesystem::path(base.wstring() + L"-summary.md");
        std::ofstream timeline(timelinePath, std::ios::binary);
        std::ofstream review(reviewPath, std::ios::binary);
        std::ofstream summary(summaryPath, std::ios::binary);
        if (!timeline || !review || !summary) return false;
        timeline << "time_ms,world,act,predicted_act,prepared_world,mastering_profile,stage_layout,production_duty,sync_mode,sync_group,palette,quality,transient_class,director_revision,bpm,tempo_confidence,bass,mids,highs,kick,snare,hats,onset,energy,gpu_ms,gpu_budget_ms,cue_density,world_cue_density,prediction_confidence,prediction_lead_beats,adaptive_density,tempo_recovery,master_brightness,sync_packet_age_ms,flags\n";
        std::array<bool, rarestage::control::WorldCount> worlds{};
        double gpuTotal = 0.0;
        float gpuPeak = 0.0f;
        for (const auto& sample : rehearsalSamples_) {
            worlds[std::clamp(sample.world, 0, int(rarestage::control::WorldCount)-1)] = true;
            gpuTotal += sample.gpuMs;
            gpuPeak = std::max(gpuPeak, sample.gpuMs);
            timeline << sample.timeMs << ',' << sample.world << ',' << sample.section << ',' << sample.predictedSection << ',' << sample.preparedWorld << ',' << sample.masteringProfile << ',' << sample.stageLayout << ',' << sample.productionDuty << ',' << sample.syncMode << ',' << sample.syncGroup << ',' << sample.palette << ',' << sample.quality << ','
                     << sample.transientClass << ',' << sample.directorDecisionRevision << ',' << sample.bpm << ',' << sample.tempoConfidence << ',' << sample.bass << ',' << sample.mid << ','
                     << sample.high << ',' << sample.kick << ',' << sample.snare << ',' << sample.hats << ',' << sample.onset << ',' << sample.energy << ','
                     << sample.gpuMs << ',' << sample.gpuBudgetMs << ',' << sample.cueDensity << ',' << sample.worldCueDensity << ','
                     << sample.predictionConfidence << ',' << sample.predictionLeadBeats << ',' << sample.adaptiveDensity << ',' << sample.tempoRecovery << ',' << sample.masterBrightness << ',' << sample.syncPacketAge << ',' << sample.flags << '\n';
        }
        review << "time_ms,world,act,reason,status,tag,file\n";
        int approved = 0, rework = 0;
        for (const auto& item : reviewItems_) {
            if (item.status == 1) ++approved;
            if (item.status == 2) ++rework;
            review << item.timeMs << ',' << item.world << ',' << item.section << ',' << csv(item.reason) << ','
                   << (item.status == 1 ? "approved" : item.status == 2 ? "rework" : "pending") << ','
                   << csv(item.tag) << ',' << csv(item.path) << '\n';
        }
        const auto duration = rehearsalSamples_.back().timeMs;
        const auto worldCount = std::count(worlds.begin(), worlds.end(), true);
        summary << "# RareStage rehearsal " << narrow(sessionStamp_) << "\n\n"
                << "- Duration: " << narrow(clockText(duration)) << "\n"
                << "- Telemetry samples: " << rehearsalSamples_.size() << "\n"
                << "- Worlds exercised: " << worldCount << " / " << worlds.size() << "\n"
                << "- Captures: " << reviewItems_.size() << " (" << approved << " approved, " << rework << " rework)\n"
                << "- Director cues: " << state_.acceptedDirectorCueCount << " accepted, " << state_.rejectedDirectorCueCount << " rejected\n"
                << "- Average GPU frame: " << gpuTotal / rehearsalSamples_.size() << " ms\n"
                << "- Peak GPU frame: " << gpuPeak << " ms\n\n"
                << "## Cue timeline\n\n";
        for (const auto& event : rehearsalEvents_) summary << "- " << narrow(clockText(event.timeMs)) << " — " << narrow(event.kind) << ": " << narrow(event.detail) << "\n";
        lastSessionSummaryPath_ = summaryPath.wstring();
        return timeline.good() && review.good() && summary.good();
    }

    void finishRehearsal(bool notifyRenderer = true) {
        if (!rehearsalActive_) return;
        addRehearsalEvent(L"SESSION", L"Recording stopped");
        rehearsalActive_ = false;
        if (notifyRenderer && connected_) send(Command::SetRehearsalActive, 0, L"Rehearsal recorder");
        if (saveRehearsalSession()) statusText_ = L"Rehearsal package saved to " + lastSessionSummaryPath_;
        else statusText_ = L"Rehearsal ended, but its session package could not be written.";
    }

    void reviewCurrent(int status) {
        if (reviewItems_.empty()) { statusText_ = L"No rehearsal captures are waiting for review."; return; }
        reviewIndex_ = std::min(reviewIndex_, reviewItems_.size() - 1);
        reviewItems_[reviewIndex_].status = status;
        statusText_ = status == 1 ? L"Capture approved." : L"Capture marked for rework.";
    }

    void tagCurrent(int tag) {
        if (reviewItems_.empty()) { statusText_ = L"No rehearsal capture is selected."; return; }
        static constexpr std::array<const wchar_t*,5> Tags{L"TOO BUSY", L"TOO DARK", L"LOGO", L"MOTION", L"GOOD"};
        reviewIndex_ = std::min(reviewIndex_, reviewItems_.size() - 1);
        reviewItems_[reviewIndex_].tag = Tags[std::clamp(tag, 0, 4)];
        statusText_ = L"Review tag set to " + reviewItems_[reviewIndex_].tag + L".";
    }

    void applyDjName() {
        if (!connected_ || !rendererWindow_) {
            statusText_ = L"Launch RareStage before applying a DJ name.";
            editingDjName_ = false;
            return;
        }
        while (!djNameBuffer_.empty() && djNameBuffer_.front() == L' ') djNameBuffer_.erase(djNameBuffer_.begin());
        while (!djNameBuffer_.empty() && djNameBuffer_.back() == L' ') djNameBuffer_.pop_back();
        if (djNameBuffer_.empty()) djNameBuffer_ = L"RARE";
        COPYDATASTRUCT copy{};
        copy.dwData = rarestage::control::DjNameCopyDataId;
        copy.cbData = static_cast<DWORD>((djNameBuffer_.size() + 1) * sizeof(wchar_t));
        copy.lpData = djNameBuffer_.data();
        DWORD_PTR result{};
        const bool delivered = SendMessageTimeoutW(rendererWindow_, WM_COPYDATA, reinterpret_cast<WPARAM>(window_),
            reinterpret_cast<LPARAM>(&copy), SMTO_ABORTIFHUNG | SMTO_BLOCK, 750, &result) != 0 && result != 0;
        statusText_ = delivered ? L"DJ formation target updated in the Showbrain." : L"RareStage did not accept the DJ name.";
        editingDjName_ = false;
    }

    void handleCharacter(wchar_t character) {
        if (!editingDjName_) return;
        if (character == L'\r') { applyDjName(); return; }
        if (character == L'\b') { if (!djNameBuffer_.empty()) djNameBuffer_.pop_back(); return; }
        if (character >= L' ' && character != 127 && djNameBuffer_.size() < 24) djNameBuffer_.push_back(character);
    }

    void handleKey(WPARAM key) {
        if (editingDjName_) {
            if (key == VK_ESCAPE) { editingDjName_ = false; statusText_ = L"DJ name edit cancelled."; }
            else if (key == VK_RETURN) applyDjName();
            return;
        }
        const int cue=rarestage::show::SceneForKey(int(key));
        if(cue>=0&&!(key==L'O'&&(GetKeyState(VK_CONTROL)&0x8000)))send(Command::SelectWorld,cue,WorldNames[cue]);
        else if (key == VK_OEM_4) send(Command::PreviousWorld, 0, L"Previous world");
        else if (key == VK_OEM_6) send(Command::NextWorld, 0, L"Next world");
        else if (key == L'A') send(Command::SetAutomatic, flag(rarestage::control::Automatic) ? 0 : 1, L"Smart Director");
        else if (key == VK_SPACE) send(Command::SetFrozen, flag(rarestage::control::Frozen) ? 0 : 1, L"Automation hold");
        else if (key == L'B') send(Command::SetBlackout, flag(rarestage::control::Blackout) ? 0 : 1, L"Blackout");
        else if (key == L'O' && (GetKeyState(VK_CONTROL) & 0x8000)) send(Command::CleanOpen, 0, L"Clean Open");
        else if (key == L'F') send(Command::Fallback, 0, L"Safe fallback");
        else if (key == L'H') send(Command::SetStageOverlay, flag(rarestage::control::StageOverlayVisible) ? 0 : 1, L"Stage HUD");
        else if (key == L'D') send(Command::CycleAudioDevice, 0, L"Audio device");
        else if (key == L'M') send(Command::CycleMonitor, 0, L"Output monitor");
        else if (key == L'T') send(Command::SetTimeline, flag(rarestage::control::Timeline) ? 0 : 1, L"Show mode");
        else if (key == L'C') send(Command::CaptureFrame, 0, L"Clean stage capture");
        else if (key == L'G') { if (rehearsalActive_) finishRehearsal(); else startRehearsal(); }
        else if (key >= VK_F1 && key <= VK_F4) {
            const int preset = static_cast<int>(key - VK_F1);
            if (GetKeyState(VK_SHIFT) & 0x8000) send(Command::SavePreset, preset, L"Save preset F" + std::to_wstring(preset + 1));
            else send(Command::LoadPreset, preset, PresetNames[preset]);
        } else if (key == VK_TAB) {
            selectPage(static_cast<Page>((static_cast<int>(page_) + 1) % 7));
        }
    }

    static int encodeAdjustment(Setting setting, int direction) {
        return static_cast<int>(setting) * 2 + (direction > 0 ? 1 : 0);
    }

    void adjust(Setting setting, int direction) {
        const int delta = direction > 0 ? 1 : -1;
        switch (setting) {
            case Setting::Sensitivity:
                send(Command::SetSensitivityPercent,
                     std::clamp(static_cast<int>(std::lround(state_.sensitivity * 100)) + delta * 5, 50, 150),
                     L"Sensitivity");
                break;
            case Setting::Bass:
                send(Command::SetBassGainPercent,
                     std::clamp(static_cast<int>(std::lround(state_.bassGain * 100)) + delta * 5, 50, 150),
                     L"Bass gain");
                break;
            case Setting::Mid:
                send(Command::SetMidGainPercent,
                     std::clamp(static_cast<int>(std::lround(state_.midGain * 100)) + delta * 5, 50, 150),
                     L"Mid gain");
                break;
            case Setting::High:
                send(Command::SetHighGainPercent,
                     std::clamp(static_cast<int>(std::lround(state_.highGain * 100)) + delta * 5, 50, 150),
                     L"High gain");
                break;
            case Setting::Beat:
                send(Command::SetBeatGainPercent,
                     std::clamp(static_cast<int>(std::lround(state_.beatGain * 100)) + delta * 5, 50, 150),
                     L"Beat gain");
                break;
            case Setting::Logo:
                send(Command::SetLogoScalePercent,
                     std::clamp(static_cast<int>(std::lround(state_.logoScale * 100)) + delta * 5, 75, 125),
                     L"Logo scale");
                break;
            case Setting::Dwell:
                send(Command::SetWorldDwellSeconds,
                     std::clamp(static_cast<int>(std::lround(state_.worldDwellSeconds)) + delta * 4, 40, 120),
                     L"World dwell");
                break;
            case Setting::Transition:
                send(Command::SetTransitionTenths,
                     std::clamp(static_cast<int>(std::lround(state_.transitionSeconds * 10)) + delta, 2, 30),
                     L"Transition time");
                break;
            case Setting::CueDensity:
                send(Command::SetCueDensityPercent,
                     std::clamp(static_cast<int>(std::lround(state_.cueDensity * 100)) + delta * 5, 65, 125),
                     L"Cue density");
                break;
            case Setting::MasterBrightness:
                send(Command::SetMasterBrightnessPercent,
                     std::clamp(static_cast<int>(std::lround(state_.masterBrightness * 100)) + delta * 5, 65, 100),
                     L"Master brightness");
                break;
            case Setting::SyncGroup:
                send(Command::SetSyncGroup,
                     std::clamp(state_.syncGroup + delta, 1, 9999),
                     L"Sync group");
                break;
        }
    }

    void activate(POINT point) {
        for (const auto& target : targets_) {
            if (!PtInRect(&target.rect, point)) continue;
            switch (target.action) {
                case Action::Launch: launchRenderer(); break;
                case Action::Page: selectPage(static_cast<Page>(target.value)); break;
                case Action::World: send(Command::SelectWorld, target.value, WorldNames[target.value]); break;
                case Action::Section: send(Command::ForceSection, target.value, SectionNames[target.value]); break;
                case Action::ClearSection: send(Command::ClearSectionOverride, 0, L"Automatic act detection"); break;
                case Action::Previous: send(Command::PreviousWorld, 0, L"Previous world"); break;
                case Action::Next: send(Command::NextWorld, 0, L"Next world"); break;
                case Action::Automatic: send(Command::SetAutomatic, flag(rarestage::control::Automatic) ? 0 : 1, L"Smart Director"); break;
                case Action::HoldTheme:send(Command::HoldTheme,state_.themeHeld?0:1,L"Hold theme");break;
                case Action::NextTheme:send(Command::NextTheme,0,L"Next theme on a musical boundary");break;
                case Action::Freeze: send(Command::SetFrozen, flag(rarestage::control::Frozen) ? 0 : 1, L"Automation hold"); break;
                case Action::Blackout: send(Command::SetBlackout, flag(rarestage::control::Blackout) ? 0 : 1, L"Blackout"); break;
                case Action::CleanOpen: send(Command::CleanOpen, 0, L"Clean Open"); break;
                case Action::Fallback: send(Command::Fallback, 0, L"Safe fallback"); break;
                case Action::LoadPreset: send(Command::LoadPreset, target.value, PresetNames[target.value]); break;
                case Action::SavePreset: send(Command::SavePreset, target.value, L"Save preset F" + std::to_wstring(target.value + 1)); break;
                case Action::StageOverlay: send(Command::SetStageOverlay, flag(rarestage::control::StageOverlayVisible) ? 0 : 1, L"Stage HUD"); break;
                case Action::Profile: send(Command::SetProfile, target.value, ProfileNames[target.value]); break;
                case Action::Palette: send(Command::SetPalette, target.value, PaletteNames[target.value]); break;
                case Action::TransitionStyle: send(Command::SetTransitionStyle, target.value, TransitionNames[target.value]); break;
                case Action::Quality: send(Command::SetQuality, target.value, QualityNames[target.value]); break;
                case Action::Adjust: {
                    const auto setting = static_cast<Setting>(target.value / 2);
                    adjust(setting, target.value % 2 ? 1 : -1);
                    break;
                }
                case Action::Timeline: send(Command::SetTimeline, flag(rarestage::control::Timeline) ? 0 : 1, L"Show mode"); break;
                case Action::Vsync: send(Command::SetVsync, flag(rarestage::control::VsyncEnabled) ? 0 : 1, L"Frame output"); break;
                case Action::Spout: send(Command::SetSpout, flag(rarestage::control::SpoutEnabled) ? 0 : 1, L"Spout output"); break;
                case Action::AudioProfile: send(Command::SetAudioProfile, target.value, AudioProfileNames[target.value]); break;
                case Action::ExportTrace: exportTrace(); break;
                case Action::LabelTrace: labelTrace(target.value); break;
                case Action::CaptureFrame: send(Command::CaptureFrame, 0, L"Clean stage capture"); break;
                case Action::ToggleRehearsal: if (rehearsalActive_) finishRehearsal(); else startRehearsal(); break;
                case Action::ToggleSmartCapture: smartCapture_ = !smartCapture_; statusText_ = smartCapture_ ? L"Smart captures armed." : L"Smart captures disabled."; break;
                case Action::ReviewPrevious: if (!reviewItems_.empty()) reviewIndex_ = (reviewIndex_ + reviewItems_.size() - 1) % reviewItems_.size(); break;
                case Action::ReviewNext: if (!reviewItems_.empty()) reviewIndex_ = (reviewIndex_ + 1) % reviewItems_.size(); break;
                case Action::ReviewApprove: reviewCurrent(1); break;
                case Action::ReviewRework: reviewCurrent(2); break;
                case Action::ReviewTag: tagCurrent(target.value); break;
                case Action::ExportSession: if (saveRehearsalSession()) statusText_ = L"Rehearsal package updated at " + lastSessionSummaryPath_; else statusText_ = L"No rehearsal data is available to export."; break;
                case Action::AudioDevice: send(Command::CycleAudioDevice, 0, L"Audio device"); break;
                case Action::Monitor: send(Command::CycleMonitor, 0, L"Output monitor"); break;
                case Action::SelectDisplay:send(Command::SelectMonitor,target.value,L"Stage display");break;
                case Action::ToggleMirror:send(Command::SetMirrorMask,int(state_.mirrorMask^(1u<<target.value)),L"Mirror display");break;
                case Action::MirrorAll:send(Command::SetMirrorMask,int(((1u<<std::clamp(state_.monitorCount,0,8))-1u)&~(1u<<state_.monitor)),L"Mirror all other displays");break;
                case Action::MirrorOff:send(Command::SetMirrorMask,0,L"Mirrors off");break;
                case Action::Fullscreen: send(Command::SetFullscreen, flag(rarestage::control::Fullscreen) ? 0 : 1, L"Fullscreen"); break;
                case Action::MasteringProfile: send(Command::SetMasteringProfile, target.value, MasteringNames[target.value]); break;
                case Action::StageLayout: send(Command::SetStageLayout, target.value, StageLayoutNames[target.value]); break;
                case Action::ProductionDuty: send(Command::ApplyProductionDuty, target.value, ProductionDutyNames[target.value] + std::wstring(L" production duty")); break;
                case Action::SyncMode: send(Command::SetSyncMode, target.value, SyncModeNames[target.value] + std::wstring(L" sync mode")); break;
                case Action::EditDjName: editingDjName_ = true; djNameBuffer_ = state_.djName[0] ? state_.djName : L"RARE"; statusText_ = L"Type the DJ name, then press Enter to apply."; SetFocus(window_); break;
                case Action::SceneCategory: send(Command::SetSceneCategory, target.value, SceneCategoryNames[target.value] + std::wstring(L" scene category")); break;
                case Action::TempoAnchor: send(Command::SetTempoAnchor, target.value, TempoAnchorNames[target.value] + std::wstring(L" tempo anchor")); break;
                case Action::DjIdentity: send(Command::SetDjIdentityVisible, flag(rarestage::control::DjIdentityVisible) ? 0 : 1, L"identity choreography"); break;
                case Action::QuitStage: send(Command::QuitRenderer, 0, L"Close stage"); break;
                default: break;
            }
            return;
        }
    }

    void fillRound(HDC dc, RECT rect, int radius, COLORREF fill, COLORREF outline = Line, int width = 1) const {
        if (fill == Panel) {
            RECT shadow = rect;
            OffsetRect(&shadow, 0, scale(3));
            HBRUSH shadowBrush = CreateSolidBrush(RGB(1, 5, 10));
            HPEN shadowPen = CreatePen(PS_NULL, 0, RGB(1, 5, 10));
            const auto oldShadowBrush = SelectObject(dc, shadowBrush);
            const auto oldShadowPen = SelectObject(dc, shadowPen);
            RoundRect(dc, shadow.left, shadow.top, shadow.right, shadow.bottom, scale(radius), scale(radius));
            SelectObject(dc, oldShadowPen);
            SelectObject(dc, oldShadowBrush);
            DeleteObject(shadowPen);
            DeleteObject(shadowBrush);
        }
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, scale(width), outline);
        const auto oldBrush = SelectObject(dc, brush);
        const auto oldPen = SelectObject(dc, pen);
        RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, scale(radius), scale(radius));
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
        if (fill == Panel || fill == PanelRaised) {
            HPEN highlight = CreatePen(PS_SOLID, 1, blendColor(outline, Text, fill == Panel ? .16f : .10f));
            const auto oldHighlight = SelectObject(dc, highlight);
            MoveToEx(dc, rect.left + scale(radius), rect.top + 1, nullptr);
            LineTo(dc, rect.right - scale(radius), rect.top + 1);
            SelectObject(dc, oldHighlight);
            DeleteObject(highlight);
        }
    }

    void fillRect(HDC dc, RECT rect, COLORREF color) const {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }

    void paintAmbientGlow(HDC dc, int centerX, int centerY, int radius, COLORREF color, float strength) const {
        constexpr int Rings = 15;
        for (int ring = Rings; ring >= 1; --ring) {
            const float normalized = static_cast<float>(ring) / Rings;
            const int extent = static_cast<int>(radius * normalized);
            const float weight = strength * std::pow(1.0f - normalized, 2.0f);
            const COLORREF shade = blendColor(Background, color, weight);
            HBRUSH brush = CreateSolidBrush(shade);
            HPEN pen = CreatePen(PS_NULL, 0, shade);
            const auto oldBrush = SelectObject(dc, brush);
            const auto oldPen = SelectObject(dc, pen);
            Ellipse(dc, centerX - extent, centerY - extent, centerX + extent, centerY + extent);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(pen);
            DeleteObject(brush);
        }
    }

    void paintAtmosphere(HDC dc, RECT client) const {
        fillRect(dc, client, Background);
        const float t = animationTime_;
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        paintAmbientGlow(dc,
                         static_cast<int>(width * (.18f + .055f * std::sin(t * .19f))),
                         static_cast<int>(height * (.26f + .045f * std::cos(t * .15f))),
                         scale(360), Cyan, .105f + connectionEase_ * .025f);
        paintAmbientGlow(dc,
                         static_cast<int>(width * (.80f + .045f * std::cos(t * .13f))),
                         static_cast<int>(height * (.72f + .055f * std::sin(t * .17f))),
                         scale(420), Purple, .080f);
        paintAmbientGlow(dc,
                         static_cast<int>(width * (.58f + .025f * std::sin(t * .11f))),
                         static_cast<int>(height * .10f), scale(250), Green, .035f + connectionEase_ * .018f);
    }

    void drawText(HDC dc, const std::wstring& value, RECT rect, HFONT font, COLORREF color,
                  UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS) const {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        const auto oldFont = SelectObject(dc, font);
        DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &rect, format);
        SelectObject(dc, oldFont);
    }

    void sectionLabel(HDC dc, const std::wstring& value, int x, int y, int width) const {
        drawText(dc, value, RECT{x, y, x + width, y + scale(20)}, headingFont_, Muted);
    }

    void addButton(HDC dc, RECT rect, const std::wstring& label, Action action, int value,
                   bool active = false, COLORREF accent = Cyan, bool danger = false) {
        const bool local = action == Action::Launch || action == Action::Page || action == Action::EditDjName || action == Action::ExportTrace || action == Action::LabelTrace ||
                           action == Action::ToggleRehearsal || action == Action::ToggleSmartCapture || action == Action::ReviewPrevious ||
                           action == Action::ReviewNext || action == Action::ReviewApprove || action == Action::ReviewRework ||
                           action == Action::ReviewTag || action == Action::ExportSession;
        const bool enabled = connected_ || local;
        const bool hovered = enabled && PtInRect(&rect, cursor_);
        COLORREF fill = enabled ? PanelRaised : Panel;
        COLORREF border = enabled ? Line : RGB(18, 35, 45);
        COLORREF text = enabled ? Text : RGB(67, 88, 98);
        if (active && enabled) {
            fill = danger ? RGB(71, 14, 29) : RGB(11, 46, 60);
            border = accent;
        }
        if (hovered) {
            const float shimmer = .075f + .025f * (.5f + .5f * std::sin(animationTime_ * 3.2f));
            fill = blendColor(fill, accent, shimmer);
            border = blendColor(border, accent, .52f);
        }
        fillRound(dc, rect, 7, fill, border, active ? 2 : 1);
        if (active && enabled) {
            RECT rail{rect.left + scale(9), rect.bottom - scale(3), rect.right - scale(9), rect.bottom - scale(2)};
            fillRect(dc, rail, blendColor(accent, Text, .14f + .08f * (.5f + .5f * std::sin(animationTime_ * 2.1f))));
        }
        drawText(dc, label, rect, bodyFont_, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (enabled) targets_.push_back({rect, action, value});
    }

    void meter(HDC dc, const wchar_t* label, float value, RECT rect, COLORREF color) const {
        value = std::clamp(value, 0.0f, 1.0f);
        drawText(dc, label, RECT{rect.left, rect.top, rect.right, rect.top + scale(16)}, smallFont_, Muted);
        drawText(dc, std::to_wstring(static_cast<int>(std::lround(value * 100))) + L"%",
                 RECT{rect.left, rect.top, rect.right, rect.top + scale(16)}, smallFont_, Text,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        RECT track{rect.left, rect.top + scale(21), rect.right, rect.top + scale(29)};
        fillRound(dc, track, 4, RGB(4, 12, 19), Line);
        if (value > 0.005f) {
            RECT level = track;
            level.right = level.left + static_cast<int>((level.right - level.left) * value);
            fillRound(dc, level, 4, color, color);
        }
    }

    void parameterRow(HDC dc, RECT rect, const std::wstring& label, const std::wstring& value, Setting setting) {
        fillRound(dc, rect, 7, PanelRaised, Line);
        drawText(dc, label, RECT{rect.left + scale(12), rect.top, rect.left + scale(185), rect.bottom}, bodyFont_, Muted);
        drawText(dc, value, RECT{rect.left + scale(185), rect.top, rect.right - scale(98), rect.bottom}, bodyFont_, Text,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        const int button = rect.bottom - rect.top - scale(8);
        RECT minus{rect.right - scale(88), rect.top + scale(4), rect.right - scale(88) + button, rect.bottom - scale(4)};
        RECT plus{rect.right - scale(8) - button, rect.top + scale(4), rect.right - scale(8), rect.bottom - scale(4)};
        addButton(dc, minus, L"-", Action::Adjust, encodeAdjustment(setting, -1));
        addButton(dc, plus, L"+", Action::Adjust, encodeAdjustment(setting, 1));
    }

    void paintHeader(HDC dc, const RECT& client) {
        const int margin = scale(20);
        RECT header{margin, scale(10), client.right - margin, scale(66)};
        drawText(dc, L"RARESTAGE", RECT{header.left, header.top, header.left + scale(210), header.bottom}, titleFont_, Text);
        drawText(dc, L"CONSOLE  /  COMPLETE SHOW CONTROL", RECT{header.left + scale(205), header.top + scale(3), header.left + scale(520), header.bottom}, headingFont_, Cyan);

        const std::wstring connection = connected_ ? L"ENGINE CONNECTED" :
            rendererWindow_ ? L"ENGINE INCOMPATIBLE" : L"ENGINE OFFLINE";
        const int pulseRadius = scale(4 + static_cast<int>(connectionEase_ * (1.0f + .5f * std::sin(animationTime_ * 2.4f))));
        const int dotX = client.right - margin - scale(374);
        const int dotY = (header.top + header.bottom) / 2;
        HBRUSH statusBrush = CreateSolidBrush(connected_ ? Green : Amber);
        HPEN statusPen = CreatePen(PS_NULL, 0, connected_ ? Green : Amber);
        const auto oldStatusBrush = SelectObject(dc, statusBrush);
        const auto oldStatusPen = SelectObject(dc, statusPen);
        Ellipse(dc, dotX - pulseRadius, dotY - pulseRadius, dotX + pulseRadius, dotY + pulseRadius);
        SelectObject(dc, oldStatusPen);
        SelectObject(dc, oldStatusBrush);
        DeleteObject(statusPen);
        DeleteObject(statusBrush);
        drawText(dc, connection,
                 RECT{client.right - margin - scale(360), header.top, client.right - margin - scale(145), header.bottom},
                 bodyFont_, connected_ ? Green : Amber, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        addButton(dc, RECT{client.right - margin - scale(132), header.top + scale(8),
                           client.right - margin, header.bottom - scale(8)},
                  connected_ ? L"FOCUS STAGE" : L"LAUNCH STAGE", Action::Launch, 0, false, Green);

        const int tabTop = scale(68);
        const int tabWidth = scale(108);
        const std::array<const wchar_t*, 7> names{L"SHOW", L"MIX + DIRECTOR", L"AUDIO LAB", L"OUTPUT", L"PRODUCTION", L"REHEARSAL", L"SCREENS"};
        for (int index = 0; index < 7; ++index) {
            addButton(dc, RECT{margin + index * (tabWidth + scale(7)), tabTop,
                               margin + index * (tabWidth + scale(7)) + tabWidth, tabTop + scale(34)},
                      names[index], Action::Page, index, static_cast<int>(page_) == index, Cyan);
        }
        drawText(dc, L"ALT+ENTER stage fullscreen",
                 RECT{margin + scale(810), tabTop, client.right - margin, tabTop + scale(34)},
                 smallFont_, Muted, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    void paintTelemetry(HDC dc, const RECT& client) {
        const int margin = scale(20);
        RECT telemetry{margin, scale(112), client.right - margin, scale(214)};
        fillRound(dc, telemetry, 11, Panel, Line);
        sectionLabel(dc, L"LIVE SIGNAL  /  RENDER HEALTH", telemetry.left + scale(14), telemetry.top + scale(8),
                     telemetry.right - telemetry.left - scale(28));

        const int meterTop = telemetry.top + scale(35);
        const int gap = scale(12);
        const int count = 8;
        const int meterWidth = (telemetry.right - telemetry.left - scale(28) - gap * (count - 1)) / count;
        const std::array<float, count> values{
            state_.bass, state_.mid, state_.high, state_.kick, state_.snare, state_.percussion, state_.beat, state_.onset
        };
        const std::array<const wchar_t*, count> names{
            L"BASS", L"MIDS", L"HIGHS", L"KICK", L"SNARE", L"HATS", L"BEAT", L"ONSET"
        };
        const std::array<COLORREF, count> colors{Cyan, Purple, Green, Red, Purple, Amber, Cyan, Purple};
        for (int index = 0; index < count; ++index) {
            const int x = telemetry.left + scale(14) + index * (meterWidth + gap);
            meter(dc, names[index], connected_ ? values[index] : 0.0f,
                  RECT{x, meterTop, x + meterWidth, meterTop + scale(34)}, colors[index]);
        }

        const std::wstring world = connected_
            ? (state_.worldName[0] ? state_.worldName : WorldNames[std::clamp(state_.world, 0, static_cast<int>(WorldNames.size()) - 1)])
            : L"Waiting for RareStage";
        const std::wstring health = connected_
            ? world + L"  |  " + SectionNames[std::clamp(state_.songSection, 0, 4)] +
              L"  |  " + std::to_wstring(static_cast<int>(std::lround(state_.fps))) + L" FPS  |  " +
              (state_.bpm > 1 ? std::to_wstring(static_cast<int>(std::lround(state_.bpm))) + L" BPM" : L"BPM --") +
              L"  |  LOCK " + std::to_wstring(static_cast<int>(std::lround(state_.tempoConfidence * 100))) + L"%  |  " +
              state_.playbackDevice
            : world;
        drawText(dc, health,
                 RECT{telemetry.left + scale(14), telemetry.bottom - scale(27), telemetry.right - scale(14), telemetry.bottom - scale(6)},
                 smallFont_, connected_ ? Muted : Amber);
    }

    void paintShow(HDC dc, RECT body) {
        const int gap = scale(12);
        const int leftWidth = static_cast<int>((body.right - body.left) * 0.63f);
        RECT worlds{body.left, body.top, body.left + leftWidth, body.bottom};
        fillRound(dc, worlds, 11, Panel, Line);
        sectionLabel(dc, L"WORLD CUES  /  THIRTY-TWO AUTHORED SCENES",
                     worlds.left + scale(14), worlds.top + scale(9), worlds.right - worlds.left - scale(28));

        const int themeY=worlds.top+scale(35);
        addButton(dc,RECT{worlds.left+scale(14),themeY,worlds.left+scale(130),themeY+scale(29)},L"HOLD THEME",Action::HoldTheme,0,state_.themeHeld!=0,Amber);
        addButton(dc,RECT{worlds.left+scale(138),themeY,worlds.left+scale(254),themeY+scale(29)},L"NEXT THEME",Action::NextTheme,0,false,Purple);
        drawText(dc,connected_&&state_.themeName[0]?state_.themeName:L"THEMES FOLLOW THE SHOW",RECT{worlds.left+scale(263),themeY,worlds.right-scale(14),themeY+scale(29)},smallFont_,Cyan);
        const int gridTop = worlds.top + scale(76);
        const int gridGap = scale(8);
        const int columns = 4;
        const int rows = (int(WorldNames.size())+columns-1)/columns;
        const int gridWidth = (worlds.right - worlds.left - scale(28) - gridGap * (columns - 1)) / columns;
        const int gridHeight = (worlds.bottom - gridTop - scale(14) - gridGap * (rows - 1)) / rows;
        for (int index = 0; index < static_cast<int>(WorldNames.size()); ++index) {
            const int column = index % columns;
            const int row = index / columns;
            RECT button{worlds.left + scale(14) + column * (gridWidth + gridGap),
                        gridTop + row * (gridHeight + gridGap),
                        worlds.left + scale(14) + column * (gridWidth + gridGap) + gridWidth,
                        gridTop + row * (gridHeight + gridGap) + gridHeight};
            const std::wstring label = (WorldKeys[index].empty()?L"":WorldKeys[index]+L" / ") + WorldNames[index];
            addButton(dc, button, label, Action::World, index, connected_ && state_.world == index, Cyan);
        }

        const int rightLeft = worlds.right + gap;
        const int rightWidth = body.right - rightLeft;
        int top = body.top;

        RECT transport{rightLeft, top, body.right, top + scale(137)};
        fillRound(dc, transport, 11, Panel, Line);
        sectionLabel(dc, L"SHOW TRANSPORT", transport.left + scale(14), transport.top + scale(8), rightWidth - scale(28));
        const int buttonGap = scale(8);
        const int buttonWidth = (rightWidth - scale(28) - buttonGap * 2) / 3;
        int y = transport.top + scale(36);
        addButton(dc, RECT{transport.left + scale(14), y, transport.left + scale(14) + buttonWidth, y + scale(38)}, L"PREVIOUS", Action::Previous, 0);
        addButton(dc, RECT{transport.left + scale(14) + buttonWidth + buttonGap, y,
                           transport.left + scale(14) + buttonWidth * 2 + buttonGap, y + scale(38)}, L"NEXT", Action::Next, 0);
        addButton(dc, RECT{transport.left + scale(14) + (buttonWidth + buttonGap) * 2, y,
                           transport.right - scale(14), y + scale(38)}, L"STAGE HUD", Action::StageOverlay, 0,
                  flag(rarestage::control::StageOverlayVisible), Cyan);
        y += scale(45);
        addButton(dc, RECT{transport.left + scale(14), y, transport.left + scale(14) + buttonWidth, y + scale(38)}, L"SMART DIRECTOR", Action::Automatic, 0,
                  flag(rarestage::control::Automatic), Green);
        addButton(dc, RECT{transport.left + scale(14) + buttonWidth + buttonGap, y,
                           transport.left + scale(14) + buttonWidth * 2 + buttonGap, y + scale(38)}, L"HOLD AUTO", Action::Freeze, 0,
                  flag(rarestage::control::Frozen), Amber);
        addButton(dc, RECT{transport.left + scale(14) + (buttonWidth + buttonGap) * 2, y,
                           transport.right - scale(14), y + scale(38)}, L"CLEAN OPEN", Action::CleanOpen, 0, false, Cyan);

        top = transport.bottom + gap;
        RECT acts{rightLeft, top, body.right, top + scale(112)};
        fillRound(dc, acts, 11, Panel, Line);
        sectionLabel(dc, L"ACT CUES  /  AUTO-RESUME", acts.left + scale(14), acts.top + scale(7), rightWidth - scale(28));
        const int actGap = scale(6);
        const int actWidth = (rightWidth - scale(28) - actGap * 4) / 5;
        y = acts.top + scale(34);
        for (int index = 0; index < 5; ++index) {
            RECT button{acts.left + scale(14) + index * (actWidth + actGap), y,
                        acts.left + scale(14) + index * (actWidth + actGap) + actWidth, y + scale(34)};
            addButton(dc, button, SectionNames[index], Action::Section, index,
                      connected_ && state_.songSection == index, index == 2 ? Red : Purple, index == 2);
        }
        addButton(dc, RECT{acts.left + scale(14), y + scale(41), acts.right - scale(14), acts.bottom - scale(8)},
                  flag(rarestage::control::ManualSectionOverride) ? L"RETURN ACT DETECTION TO SMART DIRECTOR" : L"ACT DETECTION IS AUTOMATIC",
                  Action::ClearSection, 0, flag(rarestage::control::ManualSectionOverride), Amber);

        top = acts.bottom + gap;
        RECT presets{rightLeft, top, body.right, top + scale(116)};
        fillRound(dc, presets, 11, Panel, Line);
        sectionLabel(dc, L"SHOW PRESETS", presets.left + scale(14), presets.top + scale(7), rightWidth - scale(28));
        const int presetGap = scale(6);
        const int presetWidth = (rightWidth - scale(28) - presetGap * 3) / 4;
        y = presets.top + scale(34);
        for (int index = 0; index < 4; ++index) {
            const int x = presets.left + scale(14) + index * (presetWidth + presetGap);
            addButton(dc, RECT{x, y, x + presetWidth, y + scale(32)}, L"LOAD F" + std::to_wstring(index + 1),
                      Action::LoadPreset, index, connected_ && state_.preset == index, Cyan);
            addButton(dc, RECT{x, y + scale(38), x + presetWidth, presets.bottom - scale(8)}, L"SAVE",
                      Action::SavePreset, index, false, Purple);
        }

        top = presets.bottom + gap;
        RECT safety{rightLeft, top, body.right, body.bottom};
        fillRound(dc, safety, 11, Panel, Line);
        sectionLabel(dc, L"SAFETY", safety.left + scale(14), safety.top + scale(7), rightWidth - scale(28));
        const int safetyWidth = (rightWidth - scale(36)) / 2;
        addButton(dc, RECT{safety.left + scale(14), safety.top + scale(34),
                           safety.left + scale(14) + safetyWidth, safety.bottom - scale(10)},
                  flag(rarestage::control::Blackout) ? L"RELEASE BLACKOUT" : L"BLACKOUT",
                  Action::Blackout, 0, flag(rarestage::control::Blackout), Red, true);
        addButton(dc, RECT{safety.left + scale(22) + safetyWidth, safety.top + scale(34),
                           safety.right - scale(14), safety.bottom - scale(10)},
                  L"SAFE FALLBACK", Action::Fallback, 0, false, Amber);
    }

    void paintMix(HDC dc, RECT body) {
        const int gap = scale(12);
        const int columnWidth = (body.right - body.left - gap) / 2;
        RECT left{body.left, body.top, body.left + columnWidth, body.bottom};
        RECT right{left.right + gap, body.top, body.right, body.bottom};

        fillRound(dc, left, 11, Panel, Line);
        sectionLabel(dc, L"DIRECTOR CHARACTER", left.left + scale(14), left.top + scale(9), columnWidth - scale(28));
        const int optionGap = scale(7);
        const int optionWidth = (columnWidth - scale(28) - optionGap * 3) / 4;
        int y = left.top + scale(38);
        for (int index = 0; index < 4; ++index) {
            const int x = left.left + scale(14) + index * (optionWidth + optionGap);
            addButton(dc, RECT{x, y, x + optionWidth, y + scale(36)}, ProfileNames[index], Action::Profile, index,
                      connected_ && state_.reactionProfile == index, Purple);
        }
        sectionLabel(dc, L"PALETTE DIRECTION", left.left + scale(14), y + scale(47), columnWidth - scale(28));
        y += scale(74);
        for (int index = 0; index < 8; ++index) {
            const int x = left.left + scale(14) + (index%4) * (optionWidth + optionGap),top=y+(index/4)*scale(41);
            addButton(dc, RECT{x, top, x + optionWidth, top + scale(36)}, PaletteNames[index], Action::Palette, index,
                      connected_ && state_.fallbackPalette == index, Cyan);
        }
        y+=scale(41);

        sectionLabel(dc, L"AUDIO RESPONSE", left.left + scale(14), y + scale(48), columnWidth - scale(28));
        y += scale(76);
        const int rowHeight = scale(43);
        const int rowGap = scale(7);
        const std::array<std::wstring, 6> labels{L"SENSITIVITY", L"BASS / KICK", L"MIDS / SNARE", L"HIGHS / HATS", L"BEAT / CUES", L"LOGO SCALE"};
        const std::array<std::wstring, 6> values{
            std::to_wstring(static_cast<int>(std::lround(state_.sensitivity * 100))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(state_.bassGain * 100))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(state_.midGain * 100))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(state_.highGain * 100))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(state_.beatGain * 100))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(state_.logoScale * 100))) + L"%"
        };
        const std::array<Setting, 6> settings{Setting::Sensitivity, Setting::Bass, Setting::Mid, Setting::High, Setting::Beat, Setting::Logo};
        for (int index = 0; index < 6; ++index) {
            parameterRow(dc, RECT{left.left + scale(14), y, left.right - scale(14), y + rowHeight},
                         labels[index], values[index], settings[index]);
            y += rowHeight + rowGap;
        }

        fillRound(dc, right, 11, Panel, Line);
        sectionLabel(dc, L"LIVE DIRECTOR OVERRIDES", right.left + scale(14), right.top + scale(9), columnWidth - scale(28));
        y = right.top + scale(38);
        const int actGap = scale(7);
        const int actWidth = (columnWidth - scale(28) - actGap * 4) / 5;
        for (int index = 0; index < 5; ++index) {
            const int x = right.left + scale(14) + index * (actWidth + actGap);
            addButton(dc, RECT{x, y, x + actWidth, y + scale(42)}, SectionNames[index], Action::Section, index,
                      connected_ && state_.songSection == index, index == 2 ? Red : Purple, index == 2);
        }
        y += scale(50);
        addButton(dc, RECT{right.left + scale(14), y, right.right - scale(14), y + scale(38)},
                  flag(rarestage::control::ManualSectionOverride)
                      ? L"RELEASE MANUAL ACT  /  " + std::to_wstring(static_cast<int>(std::ceil(state_.manualOverrideRemainingSeconds))) + L"S"
                      : L"ACT DETECTION RUNNING AUTOMATICALLY",
                  Action::ClearSection, 0, flag(rarestage::control::ManualSectionOverride), Amber);

        sectionLabel(dc, L"AUTOMATION", right.left + scale(14), y + scale(50), columnWidth - scale(28));
        y += scale(78);
        const int half = (columnWidth - scale(35)) / 2;
        addButton(dc, RECT{right.left + scale(14), y, right.left + scale(14) + half, y + scale(44)},
                  L"SMART DIRECTOR", Action::Automatic, 0, flag(rarestage::control::Automatic), Green);
        addButton(dc, RECT{right.left + scale(21) + half, y, right.right - scale(14), y + scale(44)},
                  L"HOLD AUTO", Action::Freeze, 0, flag(rarestage::control::Frozen), Amber);
        y += scale(53);
        addButton(dc, RECT{right.left + scale(14), y, right.left + scale(14) + half, y + scale(44)},
                  flag(rarestage::control::Timeline) ? L"SHOW TIMELINE" : L"SMART SELECTION",
                  Action::Timeline, 0, flag(rarestage::control::Timeline), Cyan);
        addButton(dc, RECT{right.left + scale(21) + half, y, right.right - scale(14), y + scale(44)},
                  L"CLEAN OPEN", Action::CleanOpen, 0, false, Cyan);

        sectionLabel(dc, L"CURRENT DECISION", right.left + scale(14), y + scale(57), columnWidth - scale(28));
        y += scale(84);
        RECT decision{right.left + scale(14), y, right.right - scale(14), y + scale(112)};
        fillRound(dc, decision, 8, PanelRaised, Line);
        const std::wstring activeWorld = connected_
            ? WorldNames[std::clamp(state_.world, 0, static_cast<int>(WorldNames.size()) - 1)]
            : L"NO ACTIVE WORLD";
        drawText(dc, L"ACTIVE  " + activeWorld + L"  /  " +
                     (connected_ ? SectionNames[std::clamp(state_.songSection, 0, 4)] : L"OFFLINE"),
                 RECT{decision.left + scale(14), decision.top + scale(6), decision.right - scale(14), decision.top + scale(31)}, headingFont_, Text);
        auto branchName=[&](int world)->std::wstring{return connected_&&world>=0&&world<int(WorldNames.size())?WorldNames[world]:L"preparing";};
        drawText(dc,L"CONTINUE  "+branchName(state_.composerContinue),RECT{decision.left+scale(14),decision.top+scale(31),decision.right-scale(14),decision.top+scale(56)},smallFont_,Cyan);
        drawText(dc,L"DROP  "+branchName(state_.composerDrop)+L"  /  BREAK  "+branchName(state_.composerBreak),RECT{decision.left+scale(14),decision.top+scale(56),decision.right-scale(14),decision.top+scale(81)},smallFont_,Purple);
        drawText(dc,connected_&&state_.directorReason[0]?state_.directorReason:L"DIRECTOR TELEMETRY STARTING",RECT{decision.left+scale(14),decision.top+scale(81),decision.right-scale(14),decision.bottom-scale(5)},smallFont_,Muted);

    }

    void traceLine(HDC dc, RECT plot, float TraceSample::* field, COLORREF color) const {
        if (trace_.size() < 2) return;
        constexpr std::size_t VisibleSamples = 900;
        const std::size_t first = trace_.size() > VisibleSamples ? trace_.size() - VisibleSamples : 0;
        const std::size_t count = trace_.size() - first;
        HPEN pen = CreatePen(PS_SOLID, scale(1), color);
        const auto oldPen = SelectObject(dc, pen);
        for (std::size_t index = 0; index < count; ++index) {
            const float x = count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) : 0.0f;
            const float value = std::clamp(trace_[first + index].*field, 0.0f, 1.0f);
            const int px = plot.left + static_cast<int>(x * (plot.right - plot.left));
            const int py = plot.bottom - static_cast<int>(value * (plot.bottom - plot.top));
            if (index == 0) MoveToEx(dc, px, py, nullptr);
            else LineTo(dc, px, py);
        }
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }

    void paintAudio(HDC dc, RECT body) {
        const int gap = scale(12);
        const int leftWidth = static_cast<int>((body.right - body.left) * .42f);
        RECT left{body.left, body.top, body.left + leftWidth, body.bottom};
        RECT right{left.right + gap, body.top, body.right, body.bottom};
        fillRound(dc, left, 11, Panel, Line);
        fillRound(dc, right, 11, Panel, Line);

        sectionLabel(dc, L"DETECTOR PROFILE  /  LIVE CALIBRATION", left.left + scale(14), left.top + scale(9), leftWidth - scale(28));
        const int profileGap = scale(7);
        const int profileWidth = (leftWidth - scale(35)) / 2;
        int y = left.top + scale(39);
        for (int index = 0; index < 4; ++index) {
            const int column = index % 2;
            const int row = index / 2;
            const int x = left.left + scale(14) + column * (profileWidth + profileGap);
            const int top = y + row * scale(43);
            addButton(dc, RECT{x, top, x + profileWidth, top + scale(36)}, AudioProfileNames[index],
                      Action::AudioProfile, index, connected_ && state_.audioProfile == index, index == 0 ? Green : Cyan);
        }

        y += scale(96);
        sectionLabel(dc, L"INSTRUMENT CONFIDENCE", left.left + scale(14), y, leftWidth - scale(28));
        y += scale(28);
        meter(dc, L"KICK EVIDENCE  /  LOW ONSET", connected_ ? state_.kickEvidence : 0.0f,
              RECT{left.left + scale(14), y, left.right - scale(14), y + scale(35)}, Red);
        y += scale(47);
        meter(dc, L"SNARE EVIDENCE  /  MID ONSET", connected_ ? state_.snareEvidence : 0.0f,
              RECT{left.left + scale(14), y, left.right - scale(14), y + scale(35)}, Purple);
        y += scale(47);
        meter(dc, L"HI-HAT EVIDENCE  /  AIR ONSET", connected_ ? state_.hatEvidence : 0.0f,
              RECT{left.left + scale(14), y, left.right - scale(14), y + scale(35)}, Amber);

        y += scale(54);
        sectionLabel(dc, L"CALIBRATION STATE", left.left + scale(14), y, leftWidth - scale(28));
        y += scale(28);
        meter(dc, L"DEVICE CALIBRATION STABILITY", connected_ ? state_.calibrationStability : 0.0f,
              RECT{left.left + scale(14), y, left.right - scale(14), y + scale(35)}, Green);
        y += scale(48);
        const int requested = std::clamp(state_.audioProfile, 0, 3);
        const int resolved = std::clamp(state_.resolvedAudioProfile, 0, 3);
        std::wstring status = L"Launch RareStage to calibrate the active playback device.";
        if (connected_) {
            status = L"REQUESTED  ";
            status += AudioProfileNames[requested];
            status += L"   /   ACTIVE  ";
            status += AudioProfileNames[resolved];
            status += L"   /   ";
            status += std::to_wstring(static_cast<int>(std::lround(state_.sampleRate)));
            status += L" HZ   /   ";
            status += std::to_wstring(static_cast<int>(std::lround(state_.analysisLatencyMs)));
            status += L" MS ANALYSIS";
        }
        drawText(dc, status, RECT{left.left + scale(14), y, left.right - scale(14), y + scale(40)}, smallFont_, connected_ ? Text : Muted);
        y += scale(35);
        const wchar_t* hitName = state_.transientClass == 1 ? L"KICK" : state_.transientClass == 2 ? L"SNARE" : state_.transientClass == 3 ? L"HI-HAT" : L"AMBIGUOUS / NONE";
        std::wstring decision = L"LAST  "; decision += hitName; decision += L"   /   ACCEPTED ";
        decision += std::to_wstring(state_.acceptedKickCount + state_.acceptedSnareCount + state_.acceptedHatCount);
        decision += L"   /   REJECTED "; decision += std::to_wstring(state_.rejectedTransientCount);
        drawText(dc, decision, RECT{left.left + scale(14), y, left.right - scale(14), y + scale(28)}, smallFont_, state_.transientClass ? Cyan : Amber);
        y += scale(28);
        drawText(dc, connected_ ? state_.playbackDevice : L"No playback loopback connected",
                 RECT{left.left + scale(14), y, left.right - scale(14), y + scale(38)}, smallFont_, connected_ ? Muted : Amber,
                 DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

        sectionLabel(dc, L"SIGNAL HISTORY  /  APPROX. 30 SECONDS", right.left + scale(14), right.top + scale(9), right.right - right.left - scale(28));
        RECT plot{right.left + scale(14), right.top + scale(42), right.right - scale(14), right.bottom - scale(176)};
        fillRound(dc, plot, 7, RGB(3, 11, 18), Line);
        HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(15, 35, 48));
        const auto oldPen = SelectObject(dc, gridPen);
        for (int row = 1; row < 4; ++row) {
            const int py = plot.top + (plot.bottom - plot.top) * row / 4;
            MoveToEx(dc, plot.left, py, nullptr); LineTo(dc, plot.right, py);
        }
        SelectObject(dc, oldPen);
        DeleteObject(gridPen);
        traceLine(dc, plot, &TraceSample::bass, Cyan);
        traceLine(dc, plot, &TraceSample::mid, Purple);
        traceLine(dc, plot, &TraceSample::high, Green);
        traceLine(dc, plot, &TraceSample::kick, Red);
        traceLine(dc, plot, &TraceSample::snare, RGB(220, 140, 255));
        traceLine(dc, plot, &TraceSample::hats, Amber);
        traceLine(dc, plot, &TraceSample::threshold, RGB(85, 105, 116));

        const std::wstring legend = L"BASS / MIDS / HIGHS / ACCEPTED KICK / SNARE / HATS / THRESHOLD";
        drawText(dc, legend, RECT{plot.left, plot.bottom + scale(8), plot.right, plot.bottom + scale(31)}, smallFont_, Muted);
        drawText(dc, L"Accepted hits drive light and depth. Ambiguous transients remain visible in the export without contaminating instrument lanes.",
                 RECT{plot.left, plot.bottom + scale(34), plot.right, plot.bottom + scale(70)}, smallFont_, Text,
                 DT_LEFT | DT_TOP | DT_WORDBREAK);
        const int labelGap = scale(6);
        const int labelWidth = (plot.right - plot.left - labelGap * 3) / 4;
        static constexpr std::array<const wchar_t*,4> Labels{L"LABEL NONE", L"LABEL KICK", L"LABEL SNARE", L"LABEL HAT"};
        for (int index = 0; index < 4; ++index) {
            const int x = plot.left + index * (labelWidth + labelGap);
            addButton(dc, RECT{x, right.bottom - scale(94), x + labelWidth, right.bottom - scale(58)},
                      Labels[index], Action::LabelTrace, index, false, index == 0 ? Muted : index == 1 ? Red : index == 2 ? Purple : Amber);
        }
        addButton(dc, RECT{plot.left, right.bottom - scale(52), plot.right, right.bottom - scale(10)},
                  L"EXPORT CALIBRATION TRACE CSV", Action::ExportTrace, 0, false, Green);
    }

    void paintScreens(HDC dc,RECT body){
        fillRound(dc,body,11,Panel,Line);
        sectionLabel(dc,L"SYNCHRONIZED SCREEN OUTPUT",body.left+scale(18),body.top+scale(10),body.right-body.left-scale(36));
        drawText(dc,L"One visual across your displays. Mirror outputs stay clean; mismatched shapes use letterboxing.",RECT{body.left+scale(18),body.top+scale(38),body.right-scale(18),body.top+scale(65)},bodyFont_,Muted);
        const int top=body.top+scale(78),gap=scale(12),buttonWidth=(body.right-body.left-scale(48))/3;
        addButton(dc,RECT{body.left+scale(18),top,body.left+scale(18)+buttonWidth,top+scale(38)},L"MIRROR ALL OTHER DISPLAYS",Action::MirrorAll,0,state_.mirrorCount>0,Green);
        addButton(dc,RECT{body.left+scale(24)+buttonWidth,top,body.left+scale(24)+buttonWidth*2,top+scale(38)},L"MIRRORS OFF",Action::MirrorOff,0,state_.mirrorCount==0,Amber);
        addButton(dc,RECT{body.left+scale(30)+buttonWidth*2,top,body.right-scale(18),top+scale(38)},flag(rarestage::control::Fullscreen)?L"LEAVE STAGE FULLSCREEN":L"STAGE FULLSCREEN",Action::Fullscreen,0,flag(rarestage::control::Fullscreen),Purple);
        const int cardWidth=(body.right-body.left-scale(36)-gap)/2;
        const int cardHeight=std::min(scale(94),int(body.bottom-top-scale(174))/4);
        for(int index=0;index<std::clamp(state_.monitorCount,0,8);++index){
            const int x=body.left+scale(18)+(index%2)*(cardWidth+gap),y=top+scale(53)+(index/2)*(cardHeight+gap);
            RECT card{x,y,x+cardWidth,y+cardHeight};fillRound(dc,card,8,PanelRaised,Line);
            drawText(dc,state_.displayNames[index],RECT{x+scale(12),y+scale(4),card.right-scale(12),y+scale(30)},bodyFont_,Text);
            const int half=(cardWidth-scale(30))/2;
            addButton(dc,RECT{x+scale(10),y+scale(35),x+scale(10)+half,card.bottom-scale(8)},index==state_.monitor?L"MAIN STAGE DISPLAY":L"MOVE STAGE HERE",Action::SelectDisplay,index,index==state_.monitor,Cyan);
            if(index!=state_.monitor)addButton(dc,RECT{x+scale(20)+half,y+scale(35),card.right-scale(10),card.bottom-scale(8)},state_.mirrorMask&(1u<<index)?L"MIRROR ON":L"MIRROR OFF",Action::ToggleMirror,index,(state_.mirrorMask&(1u<<index))!=0,Green);
        }
        drawText(dc,state_.displayOutputStatus<0?L"Output could not open. Reconnect the display and retry.":L"ALT+ENTER or F11 toggles Stage fullscreen. ESC on a mirror closes mirror outputs.",RECT{body.left+scale(18),body.bottom-scale(48),body.right-scale(18),body.bottom-scale(15)},smallFont_,state_.displayOutputStatus<0?Red:Muted);
    }

    void paintOutput(HDC dc, RECT body) {
        const int gap = scale(12);
        const int columnWidth = (body.right - body.left - gap) / 2;
        RECT left{body.left, body.top, body.left + columnWidth, body.bottom};
        RECT right{left.right + gap, body.top, body.right, body.bottom};

        fillRound(dc, left, 11, Panel, Line);
        sectionLabel(dc, L"TIMING + SCENE MOTION", left.left + scale(14), left.top + scale(9), columnWidth - scale(28));
        int y = left.top + scale(39);
        parameterRow(dc, RECT{left.left + scale(14), y, left.right - scale(14), y + scale(45)},
                     L"WORLD DWELL", std::to_wstring(static_cast<int>(std::lround(state_.worldDwellSeconds))) + L" SEC", Setting::Dwell);
        y += scale(53);
        parameterRow(dc, RECT{left.left + scale(14), y, left.right - scale(14), y + scale(45)},
                     L"MOTION TIME", std::to_wstring(static_cast<int>(std::lround(state_.transitionSeconds * 10))) + L"/10 SEC", Setting::Transition);

        sectionLabel(dc, L"MOTION PATH", left.left + scale(14), y + scale(58), columnWidth - scale(28));
        y += scale(87);
        const int optionGap = scale(7);
        const int optionWidth = (columnWidth - scale(28) - optionGap * 3) / 4;
        for (int index = 0; index < 4; ++index) {
            const int x = left.left + scale(14) + index * (optionWidth + optionGap);
            addButton(dc, RECT{x, y, x + optionWidth, y + scale(44)}, TransitionNames[index],
                      Action::TransitionStyle, index, connected_ && state_.transitionStyle == index, Purple);
        }

        sectionLabel(dc, L"RENDER PROFILE", left.left + scale(14), y + scale(57), columnWidth - scale(28));
        y += scale(87);
        for (int index = 0; index < 4; ++index) {
            const int x = left.left + scale(14) + index * (optionWidth + optionGap);
            addButton(dc, RECT{x, y, x + optionWidth, y + scale(44)}, QualityNames[index],
                      Action::Quality, index, connected_ && state_.quality == index, Cyan);
        }

        sectionLabel(dc, L"ASYNC GPU FRAME BUDGET", left.left + scale(14), y + scale(58), columnWidth - scale(28));
        y += scale(87);
        meter(dc, L"GPU UTILIZATION  /  SELECTED PROFILE", connected_ ? std::clamp(state_.gpuBudgetUtilization, 0.0f, 1.25f) / 1.25f : 0.0f,
              RECT{left.left + scale(14), y, left.right - scale(14), y + scale(35)},
              connected_ && state_.gpuBudgetUtilization > 1.0f ? Red : Green);
        y += scale(40);
        std::wstring gpu = L"CURRENT  --   PEAK  --   BUDGET  --";
        if (connected_) {
            const auto tenth=[](float value){wchar_t text[24]{};swprintf_s(text,L"%.1f",value);return std::wstring(text);};
            gpu = L"CURRENT  " + tenth(state_.gpuFrameMs) + L" MS   /   PEAK  " + tenth(state_.gpuPeakMs) +
                  L" MS   /   BUDGET  " + tenth(state_.gpuBudgetMs) + L" MS";
        }
        drawText(dc, gpu, RECT{left.left + scale(14), y, left.right - scale(14), y + scale(27)}, smallFont_, connected_ ? Text : Muted);

        fillRound(dc, right, 11, Panel, Line);
        sectionLabel(dc, L"STAGE OUTPUT", right.left + scale(14), right.top + scale(9), columnWidth - scale(28));
        y = right.top + scale(39);
        const int half = (columnWidth - scale(35)) / 2;
        const int rowHeight = scale(46);
        const int rowGap = scale(8);
        addButton(dc, RECT{right.left + scale(14), y, right.left + scale(14) + half, y + rowHeight},
                  flag(rarestage::control::VsyncEnabled) ? L"VSYNC / DISPLAY" : L"UNLOCKED / HIGH REFRESH",
                  Action::Vsync, 0, flag(rarestage::control::VsyncEnabled), Cyan);
        addButton(dc, RECT{right.left + scale(21) + half, y, right.right - scale(14), y + rowHeight},
                  flag(rarestage::control::SpoutEnabled) ? L"SPOUT / CLEAN ON" : L"SPOUT OFF",
                  Action::Spout, 0, flag(rarestage::control::SpoutEnabled), Green);
        y += rowHeight + rowGap;
        addButton(dc, RECT{right.left + scale(14), y, right.left + scale(14) + half, y + rowHeight},
                  flag(rarestage::control::Fullscreen) ? L"LEAVE FULLSCREEN" : L"ENTER FULLSCREEN",
                  Action::Fullscreen, 0, flag(rarestage::control::Fullscreen), Purple);
        addButton(dc, RECT{right.left + scale(21) + half, y, right.right - scale(14), y + rowHeight},
                  flag(rarestage::control::StageOverlayVisible) ? L"STAGE HUD ON" : L"STAGE HUD OFF",
                  Action::StageOverlay, 0, flag(rarestage::control::StageOverlayVisible), Cyan);
        y += rowHeight + rowGap;
        addButton(dc, RECT{right.left + scale(14), y, right.left + scale(14) + half, y + rowHeight},
                  L"CYCLE AUDIO DEVICE", Action::AudioDevice, 0, false, Amber);
        addButton(dc, RECT{right.left + scale(21) + half, y, right.right - scale(14), y + rowHeight},
                  L"MONITOR " + std::to_wstring(state_.monitor + 1) + L" / " + std::to_wstring(std::max(1, state_.monitorCount)),
                  Action::Monitor, 0, false, Cyan);
        y += rowHeight + rowGap;
        addButton(dc, RECT{right.left + scale(14), y, right.right - scale(14), y + rowHeight},
                  flag(rarestage::control::Timeline) ? L"SHOW TIMELINE MODE" : L"SMART DIRECTOR MODE",
                  Action::Timeline, 0, flag(rarestage::control::Timeline), Green);

        y += rowHeight + rowGap;
        addButton(dc, RECT{right.left + scale(14), y, right.right - scale(14), y + rowHeight},
                  flag(rarestage::control::CapturePending) ? L"CAPTURING CLEAN STAGE FRAME..." : L"CAPTURE CLEAN STAGE FRAME",
                  Action::CaptureFrame, 0, flag(rarestage::control::CapturePending), Purple);

        sectionLabel(dc, L"CONNECTED PLAYBACK DEVICE", right.left + scale(14), y + scale(59), columnWidth - scale(28));
        y += scale(88);
        RECT device{right.left + scale(14), y, right.right - scale(14), y + scale(62)};
        fillRound(dc, device, 8, PanelRaised, Line);
        drawText(dc, connected_ ? state_.playbackDevice : L"No renderer telemetry",
                 RECT{device.left + scale(12), device.top, device.right - scale(12), device.bottom},
                 bodyFont_, connected_ ? Text : Muted);
        if (connected_ && state_.captureStatus != 0) {
            drawText(dc, state_.captureStatus > 0 ? L"LAST CAPTURE  " + std::wstring(state_.lastCapturePath) : L"LAST CAPTURE FAILED",
                     RECT{device.left, device.bottom + scale(6), device.right, device.bottom + scale(32)}, smallFont_, state_.captureStatus > 0 ? Green : Red);
        }

        RECT closeRect{right.left + scale(14), right.bottom - scale(54), right.right - scale(14), right.bottom - scale(10)};
        addButton(dc, closeRect, L"CLOSE RARESTAGE RENDERER", Action::QuitStage, 0, false, Red, true);
    }

    void paintProduction(HDC dc, RECT body) {
        const int gap = scale(12);
        const int columnWidth = (body.right - body.left - gap) / 2;
        RECT left{body.left, body.top, body.left + columnWidth, body.bottom};
        RECT right{left.right + gap, body.top, body.right, body.bottom};
        fillRound(dc, left, 11, Panel, Line);
        fillRound(dc, right, 11, Panel, Line);

        sectionLabel(dc, L"PRODUCTION DUTY  /  ONE-TOUCH SHOW MASTER", left.left + scale(14), left.top + scale(9), columnWidth - scale(28));
        int y = left.top + scale(39);
        const int dutyGap = scale(6);
        const int dutyWidth = (columnWidth - scale(28) - dutyGap * 4) / 5;
        for (int index = 0; index < 5; ++index) {
            const int x = left.left + scale(14) + index * (dutyWidth + dutyGap);
            addButton(dc, RECT{x, y, x + dutyWidth, y + scale(44)}, ProductionDutyNames[index],
                      Action::ProductionDuty, index, connected_ && state_.productionDuty == index,
                      index == 2 ? Red : index == 3 ? Purple : Green, index == 2);
        }

        sectionLabel(dc, L"OUTPUT MASTERING", left.left + scale(14), y + scale(58), columnWidth - scale(28));
        y += scale(88);
        const int optionGap = scale(7);
        const int optionWidth = (columnWidth - scale(28) - optionGap * 3) / 4;
        for (int index = 0; index < 4; ++index) {
            const int x = left.left + scale(14) + index * (optionWidth + optionGap);
            addButton(dc, RECT{x, y, x + optionWidth, y + scale(44)}, MasteringNames[index],
                      Action::MasteringProfile, index, connected_ && state_.masteringProfile == index,
                      index == 1 ? Green : index == 2 ? Amber : Cyan);
        }
        y += scale(54);
        parameterRow(dc, RECT{left.left + scale(14), y, left.right - scale(14), y + scale(45)},
                     L"MASTER BRIGHTNESS", std::to_wstring(static_cast<int>(std::lround(state_.masterBrightness * 100))) + L"%",
                     Setting::MasterBrightness);

        sectionLabel(dc, L"MAPPED STAGE COMPOSITION", left.left + scale(14), y + scale(59), columnWidth - scale(28));
        y += scale(89);
        for (int index = 0; index < 4; ++index) {
            const int x = left.left + scale(14) + index * (optionWidth + optionGap);
            addButton(dc, RECT{x, y, x + optionWidth, y + scale(48)}, StageLayoutNames[index],
                      Action::StageLayout, index, connected_ && state_.stageLayout == index, Purple);
        }
        y += scale(62);
        RECT masteringNote{left.left + scale(14), y, left.right - scale(14), left.bottom - scale(14)};
        fillRound(dc, masteringNote, 8, PanelRaised, Line);
        drawText(dc, L"LED SAFE rolls highlights before clipping. ARENA preserves saturation and impact. BROADCAST keeps the signal inside a restrained display range. Layout masks are applied after bloom so panel gaps remain truly black.",
                 RECT{masteringNote.left + scale(13), masteringNote.top + scale(10), masteringNote.right - scale(13), masteringNote.bottom - scale(10)},
                 smallFont_, Muted, DT_LEFT | DT_TOP | DT_WORDBREAK);

        sectionLabel(dc, L"MULTI-MACHINE SHOW CLOCK", right.left + scale(14), right.top + scale(9), columnWidth - scale(28));
        y = right.top + scale(39);
        const int syncGap = scale(8);
        const int syncWidth = (columnWidth - scale(28) - syncGap * 2) / 3;
        for (int index = 0; index < 3; ++index) {
            const int x = right.left + scale(14) + index * (syncWidth + syncGap);
            addButton(dc, RECT{x, y, x + syncWidth, y + scale(46)}, SyncModeNames[index],
                      Action::SyncMode, index, connected_ && state_.syncMode == index,
                      index == 1 ? Green : index == 2 ? Purple : Cyan);
        }
        y += scale(60);
        parameterRow(dc, RECT{right.left + scale(14), y, right.right - scale(14), y + scale(43)},
                     L"SYNC GROUP", std::to_wstring(std::clamp(state_.syncGroup, 1, 9999)), Setting::SyncGroup);
        y += scale(51);
        RECT syncCard{right.left + scale(14), y, right.right - scale(14), y + scale(126)};
        fillRound(dc, syncCard, 9, PanelRaised, flag(rarestage::control::SyncConnected) ? Green : Line);
        drawText(dc, state_.syncStatus[0] ? state_.syncStatus : L"LOCAL / STANDALONE",
                 RECT{syncCard.left + scale(13), syncCard.top + scale(8), syncCard.right - scale(13), syncCard.top + scale(34)},
                 headingFont_, flag(rarestage::control::SyncConnected) ? Green : Amber);
        drawText(dc, L"PACKET  " + std::to_wstring(state_.syncSequence) + L"   /   AGE  " +
                     std::to_wstring(static_cast<int>(std::lround(state_.syncPacketAgeMs))) + L" MS   /   PHASE ERROR  " +
                     std::to_wstring(static_cast<int>(std::lround(state_.syncPhaseErrorMs))) + L" MS",
                 RECT{syncCard.left + scale(13), syncCard.top + scale(38), syncCard.right - scale(13), syncCard.top + scale(65)}, smallFont_, Cyan);
        drawText(dc, L"Leader broadcasts world, act, palette, tempo phase, cue density and output treatment on UDP 47712. Followers preserve continuous motion if the leader disappears instead of freezing or blacking out.",
                 RECT{syncCard.left + scale(13), syncCard.top + scale(69), syncCard.right - scale(13), syncCard.bottom - scale(8)}, smallFont_, Muted,
                 DT_LEFT | DT_TOP | DT_WORDBREAK);

        sectionLabel(dc, L"CLUB LOOP ENGINE  /  VDJ CONTENT PACK", right.left + scale(14), syncCard.bottom + scale(17), columnWidth - scale(28));
        y = syncCard.bottom + scale(45);
        RECT pack{right.left + scale(14), y, right.right - scale(14), right.bottom - scale(14)};
        fillRound(dc, pack, 9, PanelRaised, Line);

        drawText(dc, L"DJ IDENTITY", RECT{pack.left + scale(13), pack.top + scale(7), pack.left + scale(110), pack.top + scale(32)}, smallFont_, Muted);
        RECT nameField{pack.left + scale(112), pack.top + scale(7), pack.right - scale(108), pack.top + scale(43)};
        const bool nameHover = PtInRect(&nameField, cursor_);
        fillRound(dc, nameField, 7, nameHover || editingDjName_ ? blendColor(PanelRaised, Cyan, .10f) : Panel, editingDjName_ ? Cyan : Line, editingDjName_ ? 2 : 1);
        const std::wstring shownName = editingDjName_ ? djNameBuffer_ + ((animationTick_ / 14) % 2 ? L"|" : L"") : (state_.djName[0] ? state_.djName : L"RARE");
        drawText(dc, shownName, RECT{nameField.left + scale(10), nameField.top, nameField.right - scale(10), nameField.bottom}, bodyFont_, Text);
        targets_.push_back({nameField, Action::EditDjName, 0});
        addButton(dc, RECT{pack.right - scale(98), pack.top + scale(7), pack.right - scale(13), pack.top + scale(43)},
                  flag(rarestage::control::DjIdentityVisible) ? L"ON-AIR" : L"HIDDEN", Action::DjIdentity, 0,
                  flag(rarestage::control::DjIdentityVisible), Purple);

        drawText(dc, L"SCENE CATEGORY", RECT{pack.left + scale(13), pack.top + scale(52), pack.right - scale(13), pack.top + scale(73)}, smallFont_, Muted);
        const int categoryGap = scale(5);
        const int categoryWidth = (pack.right - pack.left - scale(26) - categoryGap * 4) / 5;
        for (int index = 0; index < static_cast<int>(SceneCategoryNames.size()); ++index) {
            const int x = pack.left + scale(13) + index * (categoryWidth + categoryGap);
            addButton(dc, RECT{x, pack.top + scale(75), x + categoryWidth, pack.top + scale(112)}, SceneCategoryNames[index],
                      Action::SceneCategory, index, connected_ && state_.sceneCategory == index, index == 0 ? Green : Cyan);
        }

        drawText(dc, L"TEMPO ANCHOR", RECT{pack.left + scale(13), pack.top + scale(121), pack.right - scale(13), pack.top + scale(142)}, smallFont_, Muted);
        const int tempoGap = scale(7);
        const int tempoWidth = (pack.right - pack.left - scale(26) - tempoGap * 2) / 3;
        for (int index = 0; index < static_cast<int>(TempoAnchorNames.size()); ++index) {
            const int x = pack.left + scale(13) + index * (tempoWidth + tempoGap);
            addButton(dc, RECT{x, pack.top + scale(144), x + tempoWidth, pack.top + scale(182)}, TempoAnchorNames[index],
                      Action::TempoAnchor, index, connected_ && state_.tempoAnchor == index, index == 0 ? Green : Purple);
        }

        const std::wstring loopStatus = L"ACTIVE  " + std::wstring(state_.categoryName[0] ? state_.categoryName : L"AUTO") +
            L"   /   " + std::to_wstring(state_.loopPeriod) + L"-BEAT LOOP   /   GRID " +
            std::to_wstring(state_.effectiveTempoAnchor) + L" BPM";
        drawText(dc, loopStatus, RECT{pack.left + scale(13), pack.top + scale(190), pack.right - scale(13), pack.top + scale(216)}, headingFont_, Green);
        const std::wstring choreographyStatus = L"SHOWBRAIN  " + std::wstring(state_.choreographyName[0] ? state_.choreographyName : L"ESTABLISH") +
            L"   /   TARGET  " + std::wstring(state_.identityTarget[0] ? state_.identityTarget : L"RARE MARK") + L"   /   FORMATION " +
            std::to_wstring(static_cast<int>(std::lround(state_.identityWeight * 100))) + L"%";
        drawText(dc, choreographyStatus, RECT{pack.left + scale(13), pack.top + scale(218), pack.right - scale(13), pack.top + scale(244)}, smallFont_, Purple);
        drawText(dc, L"Smart Director rolls continuous scene patterns through establish, resolve and release stanzas. The Rare mark or DJ name becomes scene geometry; audio conducts pressure and light without stopping motion.",
                 RECT{pack.left + scale(13), pack.top + scale(244), pack.right - scale(13), pack.bottom - scale(8)}, smallFont_, Muted,
                 DT_LEFT | DT_TOP | DT_WORDBREAK);
    }

    void paintRehearsal(HDC dc, RECT body) {
        const int gap = scale(12);
        const int leftWidth = static_cast<int>((body.right - body.left) * .43f);
        RECT recorder{body.left, body.top, body.left + leftWidth, body.bottom};
        RECT review{recorder.right + gap, body.top, body.right, body.top + scale(324)};
        RECT events{review.left, review.bottom + gap, body.right, body.bottom};
        fillRound(dc, recorder, 11, Panel, Line);
        fillRound(dc, review, 11, Panel, Line);
        fillRound(dc, events, 11, Panel, Line);

        sectionLabel(dc, L"SET REVIEW  /  EVIDENCE RECORDER", recorder.left + scale(14), recorder.top + scale(9), leftWidth - scale(28));
        int y = recorder.top + scale(38);
        addButton(dc, RECT{recorder.left + scale(14), y, recorder.right - scale(14), y + scale(48)},
                  rehearsalActive_ ? L"STOP + SAVE REHEARSAL" : L"START REHEARSAL RECORDING",
                  Action::ToggleRehearsal, 0, rehearsalActive_, rehearsalActive_ ? Red : Green, rehearsalActive_);
        y += scale(57);
        addButton(dc, RECT{recorder.left + scale(14), y, recorder.right - scale(14), y + scale(42)},
                  smartCapture_ ? L"SMART CAPTURES / ARMED" : L"SMART CAPTURES / OFF",
                  Action::ToggleSmartCapture, 0, smartCapture_, Purple);

        y += scale(53);
        RECT stats{recorder.left + scale(14), y, recorder.right - scale(14), y + scale(83)};
        fillRound(dc, stats, 8, PanelRaised, Line);
        std::array<bool, rarestage::control::WorldCount> exercised{};
        for (const auto& sample : rehearsalSamples_) exercised[std::clamp(sample.world, 0, int(rarestage::control::WorldCount)-1)] = true;
        const auto duration = rehearsalActive_ ? GetTickCount64() - rehearsalStartedAt_ :
            (rehearsalSamples_.empty() ? 0 : rehearsalSamples_.back().timeMs);
        drawText(dc, rehearsalActive_ ? L"RECORDING  " + clockText(duration) : L"SESSION  " + clockText(duration),
                 RECT{stats.left + scale(12), stats.top + scale(5), stats.right - scale(12), stats.top + scale(29)}, headingFont_, rehearsalActive_ ? Green : Text);
        drawText(dc, L"SAMPLES  " + std::to_wstring(rehearsalSamples_.size()) + L"   /   WORLDS  " +
                     std::to_wstring(std::count(exercised.begin(), exercised.end(), true)) + L" / 18",
                 RECT{stats.left + scale(12), stats.top + scale(30), stats.right - scale(12), stats.top + scale(54)}, smallFont_, Muted);
        drawText(dc, L"CAPTURES  " + std::to_wstring(reviewItems_.size()) + L"   /   DIRECTOR  " +
                     std::to_wstring(state_.acceptedDirectorCueCount) + L"A / " + std::to_wstring(state_.rejectedDirectorCueCount) + L"R",
                 RECT{stats.left + scale(12), stats.top + scale(54), stats.right - scale(12), stats.bottom - scale(5)}, smallFont_, Cyan);

        y += scale(96);
        sectionLabel(dc, L"CUE DENSITY MASTER", recorder.left + scale(14), y, leftWidth - scale(28));
        y += scale(27);
        parameterRow(dc, RECT{recorder.left + scale(14), y, recorder.right - scale(14), y + scale(45)},
                     L"OPERATOR MASTER", std::to_wstring(static_cast<int>(std::lround(state_.cueDensity * 100))) + L"%", Setting::CueDensity);
        y += scale(53);
        drawText(dc, L"MASTERED RESULT  " + std::to_wstring(static_cast<int>(std::lround(state_.worldCueDensity * 100))) +
                     L"%   /   ADAPTIVE  " + std::to_wstring(static_cast<int>(std::lround(state_.adaptiveCueDensity * 100))) + L"%",
                 RECT{recorder.left + scale(14), y, recorder.right - scale(14), y + scale(23)}, smallFont_, Purple);
        y += scale(31);
        meter(dc, L"GPU FRAME BUDGET", connected_ ? std::min(state_.gpuBudgetUtilization, 1.25f) / 1.25f : 0.f,
              RECT{recorder.left + scale(14), y, recorder.right - scale(14), y + scale(35)},
              state_.gpuBudgetUtilization > 1.f ? Red : Green);

        y += scale(49);
        sectionLabel(dc, L"RECORDING CONTRACT", recorder.left + scale(14), y, leftWidth - scale(28));
        y += scale(25);
        drawText(dc, L"Telemetry stays continuous. Captures wait for GPU headroom after world and act changes, so review evidence does not stall the drop. Density changes light and cue detail—not motion time.",
                 RECT{recorder.left + scale(14), y, recorder.right - scale(14), recorder.bottom - scale(66)}, smallFont_, Muted,
                 DT_LEFT | DT_TOP | DT_WORDBREAK);
        addButton(dc, RECT{recorder.left + scale(14), recorder.bottom - scale(54), recorder.right - scale(14), recorder.bottom - scale(10)},
                  L"EXPORT / UPDATE SESSION PACKAGE", Action::ExportSession, 0, false, Cyan);

        sectionLabel(dc, L"CAPTURE REVIEW BOARD", review.left + scale(14), review.top + scale(9), review.right - review.left - scale(28));
        if (reviewItems_.empty()) {
            drawText(dc, L"Smart captures appear here after important world or act changes. Each frame can be approved, tagged, or marked for rework.",
                     RECT{review.left + scale(14), review.top + scale(48), review.right - scale(14), review.top + scale(108)}, bodyFont_, Muted,
                     DT_LEFT | DT_TOP | DT_WORDBREAK);
        } else {
            reviewIndex_ = std::min(reviewIndex_, reviewItems_.size() - 1);
            const auto& item = reviewItems_[reviewIndex_];
            const std::wstring status = item.status == 1 ? L"APPROVED" : item.status == 2 ? L"REWORK" : L"PENDING";
            const COLORREF statusColor = item.status == 1 ? Green : item.status == 2 ? Red : Amber;
            drawText(dc, L"FRAME " + std::to_wstring(reviewIndex_ + 1) + L" / " + std::to_wstring(reviewItems_.size()) + L"   " + status,
                     RECT{review.left + scale(14), review.top + scale(38), review.right - scale(14), review.top + scale(65)}, headingFont_, statusColor);
            drawText(dc, std::wstring(WorldNames[std::clamp(item.world, 0, int(rarestage::control::WorldCount)-1)]) + L"  /  " + SectionNames[std::clamp(item.section, 0, 4)] +
                         L"  /  " + clockText(item.timeMs) + L"  /  " + item.reason,
                     RECT{review.left + scale(14), review.top + scale(65), review.right - scale(14), review.top + scale(91)}, smallFont_, Cyan);
            drawText(dc, item.path, RECT{review.left + scale(14), review.top + scale(91), review.right - scale(14), review.top + scale(120)}, smallFont_, Muted);
        }
        const int navY = review.top + scale(127);
        const int half = (review.right - review.left - scale(35)) / 2;
        addButton(dc, RECT{review.left + scale(14), navY, review.left + scale(14) + half, navY + scale(36)}, L"PREVIOUS", Action::ReviewPrevious, 0);
        addButton(dc, RECT{review.left + scale(21) + half, navY, review.right - scale(14), navY + scale(36)}, L"NEXT", Action::ReviewNext, 0);
        addButton(dc, RECT{review.left + scale(14), navY + scale(43), review.left + scale(14) + half, navY + scale(82)}, L"APPROVE", Action::ReviewApprove, 0, false, Green);
        addButton(dc, RECT{review.left + scale(21) + half, navY + scale(43), review.right - scale(14), navY + scale(82)}, L"MARK REWORK", Action::ReviewRework, 0, false, Red, true);
        const int tagY = navY + scale(91);
        const int tagGap = scale(5);
        const int tagWidth = (review.right - review.left - scale(28) - tagGap * 4) / 5;
        static constexpr std::array<const wchar_t*,5> Tags{L"TOO BUSY", L"TOO DARK", L"LOGO", L"MOTION", L"GOOD"};
        for (int index = 0; index < 5; ++index) {
            const int x = review.left + scale(14) + index * (tagWidth + tagGap);
            const bool active = !reviewItems_.empty() && reviewItems_[reviewIndex_].tag == Tags[index];
            addButton(dc, RECT{x, tagY, x + tagWidth, tagY + scale(35)}, Tags[index], Action::ReviewTag, index, active, index == 4 ? Green : Purple);
        }

        sectionLabel(dc, L"LIVE CUE TIMELINE", events.left + scale(14), events.top + scale(9), events.right - events.left - scale(28));
        if (rehearsalEvents_.empty()) {
            drawText(dc, L"Start recording to collect world, act, capture, and GPU-budget evidence.",
                     RECT{events.left + scale(14), events.top + scale(42), events.right - scale(14), events.top + scale(72)}, bodyFont_, Muted);
        } else {
            const int maxRows = std::max(1, static_cast<int>((events.bottom - events.top - scale(51)) / scale(28)));
            const int first = std::max(0, static_cast<int>(rehearsalEvents_.size()) - maxRows);
            int rowY = events.top + scale(38);
            for (int index = first; index < static_cast<int>(rehearsalEvents_.size()); ++index) {
                const auto& event = rehearsalEvents_[index];
                drawText(dc, clockText(event.timeMs), RECT{events.left + scale(14), rowY, events.left + scale(76), rowY + scale(24)}, smallFont_, Cyan);
                drawText(dc, event.kind, RECT{events.left + scale(78), rowY, events.left + scale(146), rowY + scale(24)}, smallFont_, Purple);
                drawText(dc, event.detail, RECT{events.left + scale(150), rowY, events.right - scale(14), rowY + scale(24)}, smallFont_, Text);
                rowY += scale(28);
            }
        }
    }

    void paint() {
        PAINTSTRUCT paintStruct{};
        HDC target = BeginPaint(window_, &paintStruct);
        RECT client{};
        GetClientRect(window_, &client);
        if (client.right <= 0 || client.bottom <= 0) {
            EndPaint(window_, &paintStruct);
            return;
        }

        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
        const auto oldBitmap = SelectObject(dc, bitmap);
        paintAtmosphere(dc, client);
        targets_.clear();

        paintHeader(dc, client);
        paintTelemetry(dc, client);

        const int margin = scale(20);
        const int footerHeight = scale(42);
        RECT body{margin, scale(226), client.right - margin, client.bottom - margin - footerHeight - scale(10)};
        const float easedPage = 1.0f - std::pow(1.0f - pageTransition_, 3.0f);
        OffsetRect(&body, static_cast<int>(scale(18) * (1.0f - easedPage)), 0);
        if (page_ == Page::Show) paintShow(dc, body);
        else if (page_ == Page::Mix) paintMix(dc, body);
        else if (page_ == Page::Audio) paintAudio(dc, body);
        else if (page_ == Page::Output) paintOutput(dc, body);
        else if (page_ == Page::Production) paintProduction(dc, body);
        else if(page_==Page::Screens)paintScreens(dc,body);
        else paintRehearsal(dc, body);

        RECT footer{margin, client.bottom - margin - footerHeight, client.right - margin, client.bottom - margin};
        fillRound(dc, footer, 9, Panel, Line);
        drawText(dc, statusText_.empty() ? L"Waiting for RareStage..." : statusText_,
                 RECT{footer.left + scale(13), footer.top, footer.right - scale(285), footer.bottom},
                 smallFont_, connected_ ? Muted : Amber);
        const std::wstring mode = flag(rarestage::control::SyncFollower)
            ? (flag(rarestage::control::SyncConnected) ? L"SYNC FOLLOWER / LOCKED" : L"SYNC FOLLOWER / HOLD")
            : flag(rarestage::control::SyncLeader)
            ? L"SYNC LEADER / BROADCAST"
            : flag(rarestage::control::MusicalEnding)
            ? L"ENDING HOLD / WORLD PRESERVED"
            : flag(rarestage::control::ManualSectionOverride)
            ? L"MANUAL ACT  " + std::to_wstring(static_cast<int>(std::ceil(state_.manualOverrideRemainingSeconds))) + L"S"
            : flag(rarestage::control::PredictiveCueArmed)
            ? L"PREDICTIVE CUE / " + std::wstring(SectionNames[std::clamp(state_.predictedSection, 0, 4)])
            : flag(rarestage::control::Automatic) ? L"SMART DIRECTOR ARMED" : L"MANUAL WORLD";
        drawText(dc, mode, RECT{footer.right - scale(275), footer.top, footer.right - scale(13), footer.bottom},
                 headingFont_, flag(rarestage::control::MusicalEnding) || flag(rarestage::control::ManualSectionOverride) ? Amber : Cyan,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
        EndPaint(window_, &paintStruct);
        ++paintCount_;
    }

    HWND window_{};
    HWND rendererWindow_{};
    HANDLE mapping_{};
    HANDLE rendererProcess_{};
    const SharedState* mappedState_{};
    SharedState state_{};
    bool connected_{};
    bool compatible_{};
    bool everConnected_{};
    bool soakTest_{};
    bool closeStageAfterSoak_{};
    bool launchFocusPending_{};
    bool trackingMouse_{};
    bool rehearsalActive_{};
    bool smartCapture_{true};
    bool editingDjName_{};
    int exitCode_{};
    UINT dpi_{96};
    Page page_{Page::Show};
    POINT cursor_{-10000, -10000};
    float animationTime_{};
    float pageTransition_{1.0f};
    float connectionEase_{};
    std::uint32_t commandSerial_{};
    std::uint32_t pendingSerial_{};
    std::uint64_t lastTraceRevision_{};
    std::uint64_t pendingSentAt_{};
    std::uint64_t soakStartedAt_{};
    std::uint64_t connectedAt_{};
    std::uint64_t animationTick_{};
    std::uint64_t rehearsalStartedAt_{};
    std::uint64_t smartCaptureDue_{};
    std::uint64_t smartCaptureDeadline_{};
    std::uint64_t lastSmartCaptureAt_{};
    std::uint64_t lastGpuCaptureAt_{};
    std::uint32_t paintCount_{};
    std::uint32_t lastRehearsalCaptureCount_{};
    std::uint32_t lastDirectorDecisionRevision_{};
    std::uint32_t lastAcceptedDirectorCueCount_{};
    std::uint32_t lastRejectedDirectorCueCount_{};
    int lastRehearsalWorld_{-1};
    int lastRehearsalSection_{-1};
    float lastGpuUtilization_{};
    std::size_t reviewIndex_{};
    std::wstring pendingDescription_;
    std::wstring statusText_;
    std::wstring sessionStamp_;
    std::wstring lastSessionSummaryPath_;
    std::wstring pendingCaptureReason_;
    std::wstring captureReasonInFlight_;
    std::wstring djNameBuffer_{L"RARE"};
    std::vector<TraceSample> trace_;
    std::vector<RehearsalSample> rehearsalSamples_;
    std::vector<RehearsalEvent> rehearsalEvents_;
    std::vector<ReviewItem> reviewItems_;
    std::vector<HitTarget> targets_;
    HFONT titleFont_{};
    HFONT headingFont_{};
    HFONT bodyFont_{};
    HFONT smallFont_{};
};

bool ReplayCalibrationTrace(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    auto split=[](const std::string& line) {
        std::vector<std::string> fields;
        std::stringstream stream(line);
        std::string field;
        while (std::getline(stream, field, ',')) {
            if (!field.empty() && field.back() == '\r') field.pop_back();
            fields.push_back(field);
        }
        return fields;
    };
    std::string line;
    if (!std::getline(file, line)) return false;
    const auto header = split(line);
    const auto findColumn=[&](const char* name) {
        const auto found=std::find(header.begin(), header.end(), name);
        return found == header.end() ? -1 : static_cast<int>(std::distance(header.begin(), found));
    };
    const int detectedColumn=findColumn("transient_class");
    const int expectedColumn=findColumn("expected_class");
    if (detectedColumn < 0 || expectedColumn < 0) return false;
    std::array<std::array<std::uint32_t,4>,4> confusion{};
    std::uint32_t labeled=0, correct=0, invalid=0;
    while (std::getline(file, line)) {
        const auto fields=split(line);
        if (detectedColumn >= static_cast<int>(fields.size()) || expectedColumn >= static_cast<int>(fields.size())) {++invalid;continue;}
        try {
            const int detected=std::stoi(fields[detectedColumn]);
            const int expected=std::stoi(fields[expectedColumn]);
            if (expected < 0) continue;
            if (detected < 0 || detected > 3 || expected > 3) {++invalid;continue;}
            ++confusion[expected][detected];
            ++labeled;
            if (expected == detected) ++correct;
        } catch (...) { ++invalid; }
    }
    auto reportPath=path;
    reportPath.replace_extension(".analysis.txt");
    std::ofstream report(reportPath, std::ios::binary);
    if (!report) return false;
    report << "RareStage calibration trace replay\n"
           << "source=" << path.generic_string() << "\n"
           << "labeled_samples=" << labeled << "\n"
           << "correct_samples=" << correct << "\n"
           << "invalid_rows=" << invalid << "\n"
           << "accuracy=" << (labeled ? static_cast<double>(correct) / labeled : 0.0) << "\n"
           << "rows=expected columns=detected [none,kick,snare,hat]\n";
    for (int expected=0; expected<4; ++expected) {
        report << expected;
        for (int detected=0; detected<4; ++detected) report << ',' << confusion[expected][detected];
        report << '\n';
    }
    return labeled > 0 && invalid == 0 && report.good();
}

bool RehearsalContractTest() {
    struct Fixture { std::uint64_t time; int world, act; float gpu, budget, density; const char* event; };
    constexpr std::array<Fixture,6> fixture{{
        {0,3,0,4.1f,8.0f,.76f,"session_start"},
        {1000,9,1,5.0f,8.0f,.98f,"world_change"},
        {1800,9,2,7.6f,8.0f,1.16f,"drop"},
        {2100,9,2,5.1f,8.0f,1.16f,"smart_capture"},
        {4200,13,4,4.8f,8.0f,1.05f,"world_change"},
        {5300,13,4,4.4f,8.0f,1.05f,"smart_capture"}
    }};
    std::ofstream timeline("RareStage-rehearsal-contract.csv",std::ios::binary);
    std::ofstream summary("RareStage-rehearsal-contract.md",std::ios::binary);
    if(!timeline||!summary)return false;
    timeline<<"time_ms,world,act,gpu_ms,gpu_budget_ms,world_cue_density,event\n";
    int captures=0;std::uint64_t lastCapture=0;
    for(const auto& row:fixture){
        timeline<<row.time<<','<<row.world<<','<<row.act<<','<<row.gpu<<','<<row.budget<<','<<row.density<<','<<row.event<<'\n';
        if(std::strcmp(row.event,"smart_capture")==0){if(row.gpu/row.budget>=.90f||row.time-lastCapture<2000)return false;lastCapture=row.time;++captures;}
        if(row.density<.55f||row.density>1.35f)return false;
    }
    summary<<"# RareStage rehearsal contract\n\n- samples: "<<fixture.size()<<"\n- captures: "<<captures
           <<"\n- rule: captures wait for GPU headroom\n- rule: cue density never drives motion time\n";
    return captures==2&&timeline.good()&&summary.good();
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (std::wcsstr(GetCommandLineW(), L"--trace-replay-self-test")) {
        return ReplayCalibrationTrace(L"tests/audio_trace_contract.csv") ? 0 : 3;
    }
    if (std::wcsstr(GetCommandLineW(), L"--rehearsal-self-test")) {
        return RehearsalContractTest() ? 0 : 5;
    }
    int argumentCount=0;
    LPWSTR* arguments=CommandLineToArgvW(GetCommandLineW(),&argumentCount);
    if (arguments) {
        for (int index=1; index+1<argumentCount; ++index) {
            if (std::wcscmp(arguments[index],L"--trace-replay") != 0) continue;
            const std::filesystem::path tracePath=arguments[index+1];
            LocalFree(arguments);
            return ReplayCalibrationTrace(tracePath) ? 0 : 4;
        }
        LocalFree(arguments);
    }
    ConsoleApp app;
    if (!app.initialize(instance)) {
        MessageBoxW(nullptr, L"RareStage Console could not start.", L"RareStage Console", MB_OK | MB_ICONERROR);
        return 1;
    }
    if(std::wcsstr(GetCommandLineW(),L"--screen-ui-test")){const bool passed=app.captureScreenUiTest();app.requestClose();app.run();return passed?0:5;}
    if (std::wcsstr(GetCommandLineW(), L"--startup-test")) app.requestClose();
    else if (std::wcsstr(GetCommandLineW(), L"--connected-soak-test")) app.beginConnectedSoakTest();
    else if (std::wcsstr(GetCommandLineW(), L"--launch-stage-soak-test")) app.beginLaunchStageSoakTest();
    return app.run();
}

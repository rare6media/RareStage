#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include "rarestage_control.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <filesystem>
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

constexpr std::array<const wchar_t*, rarestage::control::WorldCount> WorldNames{
    L"MORPH WALL", L"BIOFRAME", L"CELESTIAL RIG",
    L"STAGEWALL", L"WAVEWALL", L"PIXEL BANK",
    L"PRISM CASCADE", L"KINETIC WEAVE", L"CHROMA BLADES",
    L"KINETIC ARRAY", L"KINETIC CANOPY", L"PIXEL LOOM",
    L"MATRIX VAULT", L"PANEL CASCADE", L"SIGNAL RAIL"
};
constexpr std::array<const wchar_t*, rarestage::control::SectionCount> SectionNames{
    L"OPEN", L"BUILD", L"DROP", L"BREAKDOWN", L"DRIVE"
};
constexpr std::array<const wchar_t*, rarestage::control::ProfileCount> ProfileNames{
    L"CALM", L"BALANCED", L"STAGE", L"AGGRESSIVE"
};
constexpr std::array<const wchar_t*, rarestage::control::PaletteCount> PaletteNames{
    L"RARE CYAN", L"NEBULA", L"SAKURA", L"SOLAR"
};
constexpr std::array<const wchar_t*, rarestage::control::PresetCount> PresetNames{
    L"CLEAN OPEN", L"MELODIC BLOOM", L"RARE DROP", L"GLITCH FINALE"
};
constexpr std::array<const wchar_t*, rarestage::control::TransitionStyleCount> TransitionNames{
    L"MORPH MATCH", L"HORIZON RISE", L"SIGNAL VEIL", L"GATE OPEN"
};
constexpr std::array<const wchar_t*, rarestage::control::QualityCount> QualityNames{
    L"PERFORMANCE", L"BALANCED", L"NATIVE HD", L"ULTRA 4K"
};

enum class Page { Show, Mix, Output };

enum class Setting {
    Sensitivity,
    Bass,
    Mid,
    High,
    Beat,
    Logo,
    Dwell,
    Transition
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
    AudioDevice,
    Monitor,
    Fullscreen,
    QuitStage
};

struct HitTarget {
    RECT rect{};
    Action action{Action::None};
    int value{};
};

class ConsoleApp {
public:
    ~ConsoleApp() {
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
            0, windowClass.lpszClassName, L"RareStage Console 0.10.3",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, 1380, 900,
            nullptr, nullptr, instance, this);
        if (!window_) return false;

        const BOOL dark = TRUE;
        DwmSetWindowAttribute(window_, 20, &dark, sizeof(dark));
        dpi_ = GetDpiForWindow(window_);
        createFonts();
        SetTimer(window_, 1, 33, nullptr);
        ShowWindow(window_, SW_SHOW);
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
            case WM_KEYDOWN:
                handleKey(wParam);
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
                info->ptMinTrackSize.y = scale(760);
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
        if (!rendererWindow_) rendererWindow_ = FindWindowW(rarestage::control::RendererWindowClass, nullptr);

        if (rendererWindow_ && !mapping_) {
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
        if (!rendererWindow_ || !mappedState_ ||
            !rarestage::control::ReadConsistent(mappedState_, next)) {
            connected_ = false;
            if (rendererWindow_ && !mapping_) {
                statusText_ = L"RareStage is running without the v0.10.3 Console endpoint.";
            }
            return;
        }

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

        std::wstring commandLine = L"\"" + executable.wstring() + L"\"";
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_SHOWNOACTIVATE;
        PROCESS_INFORMATION process{};
        if (CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE,
                           CREATE_NEW_PROCESS_GROUP,
                           nullptr, directory.c_str(), &startup, &process)) {
            CloseHandle(process.hThread);
            rendererProcess_ = process.hProcess;
            launchFocusPending_ = true;
            statusText_ = L"RareStage is starting. Console will remain in operator focus.";
        } else {
            statusText_ = L"RareStage could not be launched. Windows error " +
                          std::to_wstring(GetLastError()) + L".";
        }
    }

    void handleKey(WPARAM key) {
        if (key >= L'1' && key <= L'9') {
            const int world = static_cast<int>(key - L'1');
            send(Command::SelectWorld, world, WorldNames[world]);
        } else if (key == L'Q') send(Command::SelectWorld, 9, WorldNames[9]);
        else if (key == L'W') send(Command::SelectWorld, 10, WorldNames[10]);
        else if (key == L'E') send(Command::SelectWorld, 11, WorldNames[11]);
        else if (key == L'R') send(Command::SelectWorld, 12, WorldNames[12]);
        else if (key == L'Y') send(Command::SelectWorld, 13, WorldNames[13]);
        else if (key == L'U') send(Command::SelectWorld, 14, WorldNames[14]);
        else if (key == VK_OEM_4) send(Command::PreviousWorld, 0, L"Previous world");
        else if (key == VK_OEM_6) send(Command::NextWorld, 0, L"Next world");
        else if (key == L'A') send(Command::SetAutomatic, flag(rarestage::control::Automatic) ? 0 : 1, L"Smart Director");
        else if (key == VK_SPACE) send(Command::SetFrozen, flag(rarestage::control::Frozen) ? 0 : 1, L"Automation hold");
        else if (key == L'B') send(Command::SetBlackout, flag(rarestage::control::Blackout) ? 0 : 1, L"Blackout");
        else if (key == L'O') send(Command::CleanOpen, 0, L"Clean Open");
        else if (key == L'F') send(Command::Fallback, 0, L"Safe fallback");
        else if (key == L'H') send(Command::SetStageOverlay, flag(rarestage::control::StageOverlayVisible) ? 0 : 1, L"Stage HUD");
        else if (key == L'D') send(Command::CycleAudioDevice, 0, L"Audio device");
        else if (key == L'M') send(Command::CycleMonitor, 0, L"Output monitor");
        else if (key == L'T') send(Command::SetTimeline, flag(rarestage::control::Timeline) ? 0 : 1, L"Show mode");
        else if (key >= VK_F1 && key <= VK_F4) {
            const int preset = static_cast<int>(key - VK_F1);
            if (GetKeyState(VK_SHIFT) & 0x8000) send(Command::SavePreset, preset, L"Save preset F" + std::to_wstring(preset + 1));
            else send(Command::LoadPreset, preset, PresetNames[preset]);
        } else if (key == VK_TAB) {
            page_ = static_cast<Page>((static_cast<int>(page_) + 1) % 3);
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
        }
    }

    void activate(POINT point) {
        for (const auto& target : targets_) {
            if (!PtInRect(&target.rect, point)) continue;
            switch (target.action) {
                case Action::Launch: launchRenderer(); break;
                case Action::Page: page_ = static_cast<Page>(target.value); break;
                case Action::World: send(Command::SelectWorld, target.value, WorldNames[target.value]); break;
                case Action::Section: send(Command::ForceSection, target.value, SectionNames[target.value]); break;
                case Action::ClearSection: send(Command::ClearSectionOverride, 0, L"Automatic act detection"); break;
                case Action::Previous: send(Command::PreviousWorld, 0, L"Previous world"); break;
                case Action::Next: send(Command::NextWorld, 0, L"Next world"); break;
                case Action::Automatic: send(Command::SetAutomatic, flag(rarestage::control::Automatic) ? 0 : 1, L"Smart Director"); break;
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
                case Action::AudioDevice: send(Command::CycleAudioDevice, 0, L"Audio device"); break;
                case Action::Monitor: send(Command::CycleMonitor, 0, L"Output monitor"); break;
                case Action::Fullscreen: send(Command::SetFullscreen, flag(rarestage::control::Fullscreen) ? 0 : 1, L"Fullscreen"); break;
                case Action::QuitStage: send(Command::QuitRenderer, 0, L"Close stage"); break;
                default: break;
            }
            return;
        }
    }

    void fillRound(HDC dc, RECT rect, int radius, COLORREF fill, COLORREF outline = Line, int width = 1) const {
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, scale(width), outline);
        const auto oldBrush = SelectObject(dc, brush);
        const auto oldPen = SelectObject(dc, pen);
        RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, scale(radius), scale(radius));
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }

    void fillRect(HDC dc, RECT rect, COLORREF color) const {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
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
        const bool local = action == Action::Launch || action == Action::Page;
        const bool enabled = connected_ || local;
        COLORREF fill = enabled ? PanelRaised : Panel;
        COLORREF border = enabled ? Line : RGB(18, 35, 45);
        COLORREF text = enabled ? Text : RGB(67, 88, 98);
        if (active && enabled) {
            fill = danger ? RGB(71, 14, 29) : RGB(11, 46, 60);
            border = accent;
        }
        fillRound(dc, rect, 7, fill, border, active ? 2 : 1);
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
        drawText(dc, connection,
                 RECT{client.right - margin - scale(360), header.top, client.right - margin - scale(145), header.bottom},
                 bodyFont_, connected_ ? Green : Amber, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        addButton(dc, RECT{client.right - margin - scale(132), header.top + scale(8),
                           client.right - margin, header.bottom - scale(8)},
                  connected_ ? L"FOCUS STAGE" : L"LAUNCH STAGE", Action::Launch, 0, false, Green);

        const int tabTop = scale(68);
        const int tabWidth = scale(154);
        const std::array<const wchar_t*, 3> names{L"SHOW", L"MIX + DIRECTOR", L"OUTPUT"};
        for (int index = 0; index < 3; ++index) {
            addButton(dc, RECT{margin + index * (tabWidth + scale(8)), tabTop,
                               margin + index * (tabWidth + scale(8)) + tabWidth, tabTop + scale(34)},
                      names[index], Action::Page, index, static_cast<int>(page_) == index, Cyan);
        }
        drawText(dc, L"TAB cycles pages  |  F1-F4 load  |  SHIFT+F1-F4 save",
                 RECT{margin + scale(510), tabTop, client.right - margin, tabTop + scale(34)},
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
        sectionLabel(dc, L"WORLD CUES  /  FIFTEEN AUTHORED ENVIRONMENTS",
                     worlds.left + scale(14), worlds.top + scale(9), worlds.right - worlds.left - scale(28));

        const int gridTop = worlds.top + scale(38);
        const int gridGap = scale(8);
        const int columns = 3;
        const int rows = 5;
        const int gridWidth = (worlds.right - worlds.left - scale(28) - gridGap * (columns - 1)) / columns;
        const int gridHeight = (worlds.bottom - gridTop - scale(14) - gridGap * (rows - 1)) / rows;
        for (int index = 0; index < static_cast<int>(WorldNames.size()); ++index) {
            const int column = index % columns;
            const int row = index / columns;
            RECT button{worlds.left + scale(14) + column * (gridWidth + gridGap),
                        gridTop + row * (gridHeight + gridGap),
                        worlds.left + scale(14) + column * (gridWidth + gridGap) + gridWidth,
                        gridTop + row * (gridHeight + gridGap) + gridHeight};
            const std::wstring label = std::to_wstring(index + 1) + L"  /  " + WorldNames[index];
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
        for (int index = 0; index < 4; ++index) {
            const int x = left.left + scale(14) + index * (optionWidth + optionGap);
            addButton(dc, RECT{x, y, x + optionWidth, y + scale(36)}, PaletteNames[index], Action::Palette, index,
                      connected_ && state_.fallbackPalette == index, Cyan);
        }

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
        drawText(dc, activeWorld, RECT{decision.left + scale(14), decision.top + scale(8), decision.right - scale(14), decision.top + scale(38)}, headingFont_, Text);
        drawText(dc, connected_ ? SectionNames[std::clamp(state_.songSection, 0, 4)] : L"OFFLINE",
                 RECT{decision.left + scale(14), decision.top + scale(38), decision.right - scale(14), decision.top + scale(66)}, bodyFont_, Cyan);
        drawText(dc, L"ENERGY " + std::to_wstring(static_cast<int>(std::lround(state_.energy * 100))) +
                     L"%  |  TEMPO LOCK " + std::to_wstring(static_cast<int>(std::lround(state_.tempoConfidence * 100))) +
                     L"%  |  PALETTE " + PaletteNames[std::clamp(state_.directorPalette, 0, 3)],
                 RECT{decision.left + scale(14), decision.top + scale(68), decision.right - scale(14), decision.bottom - scale(8)}, smallFont_, Muted);
    }

    void paintOutput(HDC dc, RECT body) {
        const int gap = scale(12);
        const int columnWidth = (body.right - body.left - gap) / 2;
        RECT left{body.left, body.top, body.left + columnWidth, body.bottom};
        RECT right{left.right + gap, body.top, body.right, body.bottom};

        fillRound(dc, left, 11, Panel, Line);
        sectionLabel(dc, L"TIMING + TRANSITIONS", left.left + scale(14), left.top + scale(9), columnWidth - scale(28));
        int y = left.top + scale(39);
        parameterRow(dc, RECT{left.left + scale(14), y, left.right - scale(14), y + scale(45)},
                     L"WORLD DWELL", std::to_wstring(static_cast<int>(std::lround(state_.worldDwellSeconds))) + L" SEC", Setting::Dwell);
        y += scale(53);
        parameterRow(dc, RECT{left.left + scale(14), y, left.right - scale(14), y + scale(45)},
                     L"TRANSITION", std::to_wstring(static_cast<int>(std::lround(state_.transitionSeconds * 10))) + L"/10 SEC", Setting::Transition);

        sectionLabel(dc, L"TRANSITION LANGUAGE", left.left + scale(14), y + scale(58), columnWidth - scale(28));
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

        sectionLabel(dc, L"CONNECTED PLAYBACK DEVICE", right.left + scale(14), y + scale(59), columnWidth - scale(28));
        y += scale(88);
        RECT device{right.left + scale(14), y, right.right - scale(14), y + scale(62)};
        fillRound(dc, device, 8, PanelRaised, Line);
        drawText(dc, connected_ ? state_.playbackDevice : L"No renderer telemetry",
                 RECT{device.left + scale(12), device.top, device.right - scale(12), device.bottom},
                 bodyFont_, connected_ ? Text : Muted);

        RECT closeRect{right.left + scale(14), right.bottom - scale(54), right.right - scale(14), right.bottom - scale(10)};
        addButton(dc, closeRect, L"CLOSE RARESTAGE RENDERER", Action::QuitStage, 0, false, Red, true);
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
        fillRect(dc, client, Background);
        targets_.clear();

        paintHeader(dc, client);
        paintTelemetry(dc, client);

        const int margin = scale(20);
        const int footerHeight = scale(42);
        RECT body{margin, scale(226), client.right - margin, client.bottom - margin - footerHeight - scale(10)};
        if (page_ == Page::Show) paintShow(dc, body);
        else if (page_ == Page::Mix) paintMix(dc, body);
        else paintOutput(dc, body);

        RECT footer{margin, client.bottom - margin - footerHeight, client.right - margin, client.bottom - margin};
        fillRound(dc, footer, 9, Panel, Line);
        drawText(dc, statusText_.empty() ? L"Waiting for RareStage..." : statusText_,
                 RECT{footer.left + scale(13), footer.top, footer.right - scale(285), footer.bottom},
                 smallFont_, connected_ ? Muted : Amber);
        const std::wstring mode = flag(rarestage::control::ManualSectionOverride)
            ? L"MANUAL ACT  " + std::to_wstring(static_cast<int>(std::ceil(state_.manualOverrideRemainingSeconds))) + L"S"
            : flag(rarestage::control::Automatic) ? L"SMART DIRECTOR ARMED" : L"MANUAL WORLD";
        drawText(dc, mode, RECT{footer.right - scale(275), footer.top, footer.right - scale(13), footer.bottom},
                 headingFont_, flag(rarestage::control::ManualSectionOverride) ? Amber : Cyan,
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
    int exitCode_{};
    UINT dpi_{96};
    Page page_{Page::Show};
    std::uint32_t commandSerial_{};
    std::uint32_t pendingSerial_{};
    std::uint64_t pendingSentAt_{};
    std::uint64_t soakStartedAt_{};
    std::uint64_t connectedAt_{};
    std::uint32_t paintCount_{};
    std::wstring pendingDescription_;
    std::wstring statusText_;
    std::vector<HitTarget> targets_;
    HFONT titleFont_{};
    HFONT headingFont_{};
    HFONT bodyFont_{};
    HFONT smallFont_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ConsoleApp app;
    if (!app.initialize(instance)) {
        MessageBoxW(nullptr, L"RareStage Console could not start.", L"RareStage Console", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (std::wcsstr(GetCommandLineW(), L"--startup-test")) app.requestClose();
    else if (std::wcsstr(GetCommandLineW(), L"--connected-soak-test")) app.beginConnectedSoakTest();
    else if (std::wcsstr(GetCommandLineW(), L"--launch-stage-soak-test")) app.beginLaunchStageSoakTest();
    return app.run();
}

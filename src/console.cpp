#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include "rarestage_control.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using rarestage::control::Command;
using rarestage::control::CommandStatus;
using rarestage::control::SharedState;

constexpr COLORREF Background = RGB(4, 9, 17);
constexpr COLORREF Panel = RGB(9, 20, 32);
constexpr COLORREF PanelRaised = RGB(13, 29, 44);
constexpr COLORREF Line = RGB(28, 61, 80);
constexpr COLORREF Text = RGB(231, 249, 255);
constexpr COLORREF Muted = RGB(111, 143, 158);
constexpr COLORREF Cyan = RGB(25, 222, 244);
constexpr COLORREF Green = RGB(113, 247, 166);
constexpr COLORREF Purple = RGB(157, 103, 255);
constexpr COLORREF Amber = RGB(255, 190, 70);
constexpr COLORREF Red = RGB(255, 52, 89);

constexpr std::array<const wchar_t*, 9> WorldNames{
    L"MORPH DIMENSION", L"RARE BIOSPHERE", L"CELESTIAL GARDEN",
    L"STAGEWALL", L"NEON OCEAN", L"PIXEL CURRENT",
    L"PRISM CASCADE", L"KINETIC WEAVE", L"RARE ECLIPSE"
};
constexpr std::array<const wchar_t*, 5> SectionNames{
    L"OPEN", L"BUILD", L"DROP", L"BREAKDOWN", L"DRIVE"
};
constexpr std::array<const wchar_t*, 4> ProfileNames{
    L"CALM", L"BALANCED", L"STAGE", L"AGGRESSIVE"
};
constexpr std::array<const wchar_t*, 4> PresetNames{
    L"CLEAN OPEN", L"MELODIC BLOOM", L"RARE DROP", L"GLITCH FINALE"
};

enum class Action {
    None,
    Launch,
    World,
    Section,
    Previous,
    Next,
    Automatic,
    Freeze,
    Blackout,
    CleanOpen,
    Fallback,
    Preset,
    Profile,
    SensitivityDown,
    SensitivityUp,
    StageOverlay
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
            0, windowClass.lpszClassName, L"RareStage Console",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, 1260, 790,
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
                return 0;
            case WM_PAINT:
                paint();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_LBUTTONUP: {
                POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                activate(point);
                return 0;
            }
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
                info->ptMinTrackSize.x = scale(1080);
                info->ptMinTrackSize.y = scale(790);
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
                PostQuitMessage(0);
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
        titleFont_ = makeFont(25, FW_BOLD);
        headingFont_ = makeFont(12, FW_BOLD);
        bodyFont_ = makeFont(10, FW_SEMIBOLD);
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
    }

    void updateConnection() {
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
        if (!rendererWindow_ || !mappedState_ || !rarestage::control::ReadConsistent(mappedState_, next)) {
            connected_ = false;
            return;
        }

        const auto age = GetTickCount64() >= next.heartbeatTickMs
            ? GetTickCount64() - next.heartbeatTickMs
            : 0;
        if (age > 1800) {
            disconnect();
            statusText_ = L"Renderer heartbeat lost. Waiting for RareStage to restart.";
            return;
        }

        state_ = next;
        compatible_ = next.protocolVersion == rarestage::control::ProtocolVersion &&
                      next.structSize == sizeof(SharedState);
        connected_ = compatible_ && (next.flags & rarestage::control::RendererReady);
        if (!compatible_) {
            statusText_ = L"Protocol mismatch. Update RareStage and Console together.";
        } else if (pendingSerial_ && next.lastCommandSerial == pendingSerial_) {
            if (next.lastCommandStatus == CommandStatus::Accepted) statusText_ = pendingDescription_ + L" acknowledged.";
            else statusText_ = pendingDescription_ + L" rejected by RareStage.";
            pendingSerial_ = 0;
        } else if (connected_ && statusText_.empty()) {
            statusText_ = L"Console connected. Program output is isolated from this window.";
        }
    }

    bool flag(rarestage::control::StateFlag value) const {
        return (state_.flags & value) != 0;
    }

    void send(Command command, int value, const wchar_t* description) {
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
        pendingDescription_ = description;
        statusText_ = pendingDescription_ + L" sent...";
    }

    void launchRenderer() {
        if (rendererWindow_ && !IsWindow(rendererWindow_)) disconnect();
        if (rendererWindow_ && IsWindow(rendererWindow_)) {
            ShowWindow(rendererWindow_, SW_RESTORE);
            SetForegroundWindow(rendererWindow_);
            statusText_ = L"RareStage is already running.";
            return;
        }
        wchar_t module[MAX_PATH]{};
        GetModuleFileNameW(nullptr, module, MAX_PATH);
        const auto directory = std::filesystem::path(module).parent_path();
        const auto executable = directory / L"RareStage.exe";
        if (!std::filesystem::exists(executable)) {
            statusText_ = L"RareStage.exe was not found beside RareStageConsole.exe.";
            return;
        }
        std::wstring commandLine = L"\"" + executable.wstring() + L"\"";
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE, 0,
                           nullptr, directory.c_str(), &startup, &process)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            statusText_ = L"RareStage is starting...";
        } else {
            statusText_ = L"RareStage could not be launched.";
        }
    }

    void handleKey(WPARAM key) {
        if (key >= L'1' && key <= L'9') {
            const int world = static_cast<int>(key - L'1');
            send(Command::SelectWorld, world, WorldNames[world]);
        } else if (key == VK_OEM_4) send(Command::PreviousWorld, 0, L"Previous world");
        else if (key == VK_OEM_6) send(Command::NextWorld, 0, L"Next world");
        else if (key == L'A') send(Command::SetAutomatic, flag(rarestage::control::Automatic) ? 0 : 1, L"Smart Director");
        else if (key == VK_SPACE) send(Command::ToggleFreeze, 0, L"Automation hold");
        else if (key == L'B') send(Command::SetBlackout, flag(rarestage::control::Blackout) ? 0 : 1, L"Blackout");
        else if (key == L'O') send(Command::CleanOpen, 0, L"Clean Open");
        else if (key == L'F') send(Command::Fallback, 0, L"Fallback scene");
        else if (key == VK_TAB) send(Command::ToggleStageOverlay, 0, L"Stage HUD");
        else if (key >= VK_F1 && key <= VK_F4) {
            const int preset = static_cast<int>(key - VK_F1);
            send(Command::LoadPreset, preset, PresetNames[preset]);
        }
    }

    void activate(POINT point) {
        for (const auto& target : targets_) {
            if (!PtInRect(&target.rect, point)) continue;
            switch (target.action) {
                case Action::Launch: launchRenderer(); break;
                case Action::World: send(Command::SelectWorld, target.value, WorldNames[target.value]); break;
                case Action::Section: send(Command::ForceSection, target.value, SectionNames[target.value]); break;
                case Action::Previous: send(Command::PreviousWorld, 0, L"Previous world"); break;
                case Action::Next: send(Command::NextWorld, 0, L"Next world"); break;
                case Action::Automatic: send(Command::SetAutomatic, flag(rarestage::control::Automatic) ? 0 : 1, L"Smart Director"); break;
                case Action::Freeze: send(Command::ToggleFreeze, 0, L"Automation hold"); break;
                case Action::Blackout: send(Command::SetBlackout, flag(rarestage::control::Blackout) ? 0 : 1, L"Blackout"); break;
                case Action::CleanOpen: send(Command::CleanOpen, 0, L"Clean Open"); break;
                case Action::Fallback: send(Command::Fallback, 0, L"Fallback scene"); break;
                case Action::Preset: send(Command::LoadPreset, target.value, PresetNames[target.value]); break;
                case Action::Profile: send(Command::SetProfile, target.value, ProfileNames[target.value]); break;
                case Action::SensitivityDown: send(Command::SetSensitivityPercent, std::max(50, static_cast<int>(std::lround(state_.sensitivity * 100)) - 5), L"Sensitivity"); break;
                case Action::SensitivityUp: send(Command::SetSensitivityPercent, std::min(150, static_cast<int>(std::lround(state_.sensitivity * 100)) + 5), L"Sensitivity"); break;
                case Action::StageOverlay: send(Command::ToggleStageOverlay, 0, L"Stage HUD"); break;
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

    void sectionLabel(HDC dc, const wchar_t* value, int x, int y, int width) const {
        RECT rect{x, y, x + width, y + scale(22)};
        drawText(dc, value, rect, headingFont_, Muted);
    }

    void addButton(HDC dc, RECT rect, const std::wstring& label, Action action, int value,
                   bool active = false, COLORREF accent = Cyan, bool danger = false) {
        const bool enabled = connected_ || action == Action::Launch;
        COLORREF fill = enabled ? PanelRaised : Panel;
        COLORREF border = enabled ? Line : RGB(20, 38, 49);
        COLORREF text = enabled ? Text : RGB(70, 91, 101);
        if (active && enabled) {
            fill = danger ? RGB(71, 14, 29) : RGB(12, 47, 61);
            border = accent;
            text = danger ? RGB(255, 223, 229) : Text;
        }
        fillRound(dc, rect, 8, fill, border, active ? 2 : 1);
        drawText(dc, label, rect, bodyFont_, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (enabled) targets_.push_back({rect, action, value});
    }

    void meter(HDC dc, const wchar_t* label, float value, RECT rect, COLORREF color) const {
        value = std::clamp(value, 0.0f, 1.0f);
        RECT name{rect.left, rect.top, rect.right, rect.top + scale(18)};
        drawText(dc, label, name, smallFont_, Muted);
        RECT percentRect{rect.left, rect.top, rect.right, rect.top + scale(18)};
        drawText(dc, std::to_wstring(static_cast<int>(std::lround(value * 100))) + L"%",
                 percentRect, smallFont_, Text, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        RECT track{rect.left, rect.top + scale(24), rect.right, rect.top + scale(32)};
        fillRound(dc, track, 5, RGB(5, 13, 21), Line);
        if (value > 0.005f) {
            RECT level = track;
            level.right = level.left + static_cast<int>((level.right - level.left) * value);
            fillRound(dc, level, 5, color, color);
        }
    }

    void paint() {
        PAINTSTRUCT paintStruct{};
        HDC target = BeginPaint(window_, &paintStruct);
        RECT client{};
        GetClientRect(window_, &client);
        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
        const auto oldBitmap = SelectObject(dc, bitmap);
        fillRect(dc, client, Background);
        targets_.clear();

        const int margin = scale(20);
        const int gap = scale(12);
        const int width = client.right - margin * 2;

        RECT header{margin, scale(12), client.right - margin, scale(75)};
        drawText(dc, L"RARESTAGE", RECT{header.left, header.top, header.left + scale(210), header.bottom}, titleFont_, Text);
        drawText(dc, L"CONSOLE  /  LIVE OPERATOR", RECT{header.left + scale(208), header.top + scale(5), header.left + scale(510), header.bottom}, headingFont_, Cyan);
        const std::wstring connection = connected_ ? L"●  ENGINE CONNECTED" : compatible_ ? L"●  ENGINE STARTING" : L"●  ENGINE OFFLINE";
        RECT connectionRect{client.right - margin - scale(355), header.top + scale(2), client.right - margin - scale(145), header.bottom - scale(5)};
        drawText(dc, connection, connectionRect, bodyFont_, connected_ ? Green : Amber, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        RECT launchRect{client.right - margin - scale(132), header.top + scale(9), client.right - margin, header.bottom - scale(11)};
        addButton(dc, launchRect, connected_ ? L"FOCUS STAGE" : L"LAUNCH STAGE", Action::Launch, 0, false, Green);

        const int telemetryTop = scale(80);
        const int telemetryHeight = scale(116);
        RECT telemetry{margin, telemetryTop, client.right - margin, telemetryTop + telemetryHeight};
        fillRound(dc, telemetry, 12, Panel, Line);
        sectionLabel(dc, L"LIVE SIGNAL  /  RENDER HEALTH", telemetry.left + scale(16), telemetry.top + scale(10), telemetry.right - telemetry.left - scale(32));
        const int meterTop = telemetry.top + scale(40);
        const int meterGap = scale(14);
        const int meterWidth = (telemetry.right - telemetry.left - scale(32) - meterGap * 5) / 6;
        const std::array<float, 6> meterValues{state_.bass, state_.mid, state_.high, state_.percussion, state_.beat, state_.onset};
        const std::array<const wchar_t*, 6> meterNames{L"BASS", L"MIDS", L"HIGHS", L"PERCUSSION", L"BEAT", L"ONSET"};
        const std::array<COLORREF, 6> meterColors{Cyan, Purple, Green, Amber, Cyan, Red};
        for (int index = 0; index < 6; ++index) {
            const int x = telemetry.left + scale(16) + index * (meterWidth + meterGap);
            meter(dc, meterNames[index], connected_ ? meterValues[index] : 0.0f,
                  RECT{x, meterTop, x + meterWidth, meterTop + scale(42)}, meterColors[index]);
        }
        std::wstring health = connected_
            ? std::to_wstring(static_cast<int>(std::lround(state_.fps))) + L" FPS   •   " +
              (state_.bpm > 1 ? std::to_wstring(static_cast<int>(std::lround(state_.bpm))) + L" BPM" : L"BPM --") +
              L"   •   " + state_.playbackDevice
            : L"Waiting for RareStage telemetry";
        drawText(dc, health, RECT{telemetry.left + scale(16), telemetry.bottom - scale(28), telemetry.right - scale(16), telemetry.bottom - scale(7)}, smallFont_, connected_ ? Muted : Amber);

        const int bodyTop = telemetry.bottom + gap;
        const int footerHeight = scale(42);
        const int bodyBottom = client.bottom - margin - footerHeight - gap;
        const int leftWidth = static_cast<int>(width * 0.58f);
        const int rightLeft = margin + leftWidth + gap;
        const int rightWidth = client.right - margin - rightLeft;

        RECT worlds{margin, bodyTop, margin + leftWidth, bodyTop + scale(270)};
        fillRound(dc, worlds, 12, Panel, Line);
        sectionLabel(dc, L"WORLD CUES  /  1–9", worlds.left + scale(16), worlds.top + scale(10), worlds.right - worlds.left - scale(32));
        const int gridTop = worlds.top + scale(42);
        const int gridGap = scale(9);
        const int gridWidth = (worlds.right - worlds.left - scale(32) - gridGap * 2) / 3;
        const int gridHeight = (worlds.bottom - gridTop - scale(16) - gridGap * 2) / 3;
        for (int index = 0; index < 9; ++index) {
            const int column = index % 3;
            const int row = index / 3;
            RECT button{worlds.left + scale(16) + column * (gridWidth + gridGap),
                        gridTop + row * (gridHeight + gridGap),
                        worlds.left + scale(16) + column * (gridWidth + gridGap) + gridWidth,
                        gridTop + row * (gridHeight + gridGap) + gridHeight};
            std::wstring label = std::to_wstring(index + 1) + L"  /  " + WorldNames[index];
            addButton(dc, button, label, Action::World, index, connected_ && state_.world == index, Cyan);
        }

        RECT acts{margin, worlds.bottom + gap, margin + leftWidth, bodyBottom};
        fillRound(dc, acts, 12, Panel, Line);
        sectionLabel(dc, L"ACT CUES  /  MANUAL OVERRIDE THEN AUTO-RESUME", acts.left + scale(16), acts.top + scale(10), acts.right - acts.left - scale(32));
        const int actGap = scale(8);
        const int actWidth = (acts.right - acts.left - scale(32) - actGap * 4) / 5;
        const int actTop = acts.top + scale(44);
        const int actBottom = acts.bottom - scale(18);
        for (int index = 0; index < 5; ++index) {
            RECT button{acts.left + scale(16) + index * (actWidth + actGap), actTop,
                        acts.left + scale(16) + index * (actWidth + actGap) + actWidth, actBottom};
            const bool active = connected_ && state_.songSection == index;
            addButton(dc, button, SectionNames[index], Action::Section, index, active,
                      index == 2 ? Red : Purple, index == 2);
        }

        int rightTop = bodyTop;
        RECT transport{rightLeft, rightTop, rightLeft + rightWidth, rightTop + scale(144)};
        fillRound(dc, transport, 12, Panel, Line);
        sectionLabel(dc, L"SHOW TRANSPORT", transport.left + scale(16), transport.top + scale(10), transport.right - transport.left - scale(32));
        const int transportGap = scale(8);
        const int transportWidth = (transport.right - transport.left - scale(32) - transportGap * 2) / 3;
        int y = transport.top + scale(42);
        addButton(dc, RECT{transport.left + scale(16), y, transport.left + scale(16) + transportWidth, y + scale(38)}, L"◀  PREVIOUS", Action::Previous, 0);
        addButton(dc, RECT{transport.left + scale(16) + transportWidth + transportGap, y, transport.left + scale(16) + transportWidth * 2 + transportGap, y + scale(38)}, L"NEXT  ▶", Action::Next, 0);
        addButton(dc, RECT{transport.left + scale(16) + (transportWidth + transportGap) * 2, y, transport.right - scale(16), y + scale(38)}, L"STAGE HUD", Action::StageOverlay, 0, flag(rarestage::control::StageOverlayVisible), Cyan);
        y += scale(46);
        addButton(dc, RECT{transport.left + scale(16), y, transport.left + scale(16) + transportWidth, y + scale(38)}, L"SMART DIRECTOR", Action::Automatic, 0, flag(rarestage::control::Automatic), Green);
        addButton(dc, RECT{transport.left + scale(16) + transportWidth + transportGap, y, transport.left + scale(16) + transportWidth * 2 + transportGap, y + scale(38)}, L"HOLD AUTO", Action::Freeze, 0, flag(rarestage::control::Frozen), Amber);
        addButton(dc, RECT{transport.left + scale(16) + (transportWidth + transportGap) * 2, y, transport.right - scale(16), y + scale(38)}, L"CLEAN OPEN", Action::CleanOpen, 0, false, Cyan);

        rightTop = transport.bottom + gap;
        RECT safety{rightLeft, rightTop, rightLeft + rightWidth, rightTop + scale(94)};
        fillRound(dc, safety, 12, Panel, Line);
        sectionLabel(dc, L"SAFETY", safety.left + scale(16), safety.top + scale(8), safety.right - safety.left - scale(32));
        const int safetyWidth = (safety.right - safety.left - scale(40)) / 2;
        addButton(dc, RECT{safety.left + scale(16), safety.top + scale(35), safety.left + scale(16) + safetyWidth, safety.bottom - scale(12)}, flag(rarestage::control::Blackout) ? L"RELEASE BLACKOUT" : L"BLACKOUT", Action::Blackout, 0, flag(rarestage::control::Blackout), Red, true);
        addButton(dc, RECT{safety.left + scale(24) + safetyWidth, safety.top + scale(35), safety.right - scale(16), safety.bottom - scale(12)}, L"SAFE FALLBACK", Action::Fallback, 0, false, Amber);

        rightTop = safety.bottom + gap;
        RECT director{rightLeft, rightTop, rightLeft + rightWidth, rightTop + scale(132)};
        fillRound(dc, director, 12, Panel, Line);
        sectionLabel(dc, L"DIRECTOR INTENSITY", director.left + scale(16), director.top + scale(9), director.right - director.left - scale(32));
        const int profileGap = scale(7);
        const int profileWidth = (director.right - director.left - scale(32) - profileGap * 3) / 4;
        y = director.top + scale(39);
        for (int index = 0; index < 4; ++index) {
            RECT button{director.left + scale(16) + index * (profileWidth + profileGap), y,
                        director.left + scale(16) + index * (profileWidth + profileGap) + profileWidth, y + scale(36)};
            addButton(dc, button, ProfileNames[index], Action::Profile, index,
                      connected_ && state_.reactionProfile == index, Purple);
        }
        y += scale(45);
        RECT minus{director.left + scale(16), y, director.left + scale(58), director.bottom - scale(10)};
        RECT plus{director.right - scale(58), y, director.right - scale(16), director.bottom - scale(10)};
        RECT sensitivity{minus.right + scale(7), y, plus.left - scale(7), director.bottom - scale(10)};
        addButton(dc, minus, L"−", Action::SensitivityDown, 0);
        addButton(dc, plus, L"+", Action::SensitivityUp, 0);
        fillRound(dc, sensitivity, 8, PanelRaised, Line);
        drawText(dc, L"SENSITIVITY  " + std::to_wstring(static_cast<int>(std::lround(state_.sensitivity * 100))) + L"%", sensitivity, bodyFont_, Text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        rightTop = director.bottom + gap;
        RECT presets{rightLeft, rightTop, rightLeft + rightWidth, bodyBottom};
        fillRound(dc, presets, 12, Panel, Line);
        sectionLabel(dc, L"SHOW PRESETS  /  F1–F4", presets.left + scale(16), presets.top + scale(8), presets.right - presets.left - scale(32));
        const int presetGap = scale(7);
        const int presetWidth = (presets.right - presets.left - scale(32) - presetGap * 3) / 4;
        y = presets.top + scale(39);
        for (int index = 0; index < 4; ++index) {
            RECT button{presets.left + scale(16) + index * (presetWidth + presetGap), y,
                        presets.left + scale(16) + index * (presetWidth + presetGap) + presetWidth, presets.bottom - scale(12)};
            addButton(dc, button, L"F" + std::to_wstring(index + 1), Action::Preset, index,
                      connected_ && state_.preset == index, Cyan);
        }

        RECT footer{margin, client.bottom - margin - footerHeight, client.right - margin, client.bottom - margin};
        fillRound(dc, footer, 10, Panel, Line);
        const COLORREF footerColor = connected_ ? Muted : Amber;
        drawText(dc, statusText_.empty() ? L"Waiting for RareStage..." : statusText_,
                 RECT{footer.left + scale(14), footer.top, footer.right - scale(230), footer.bottom}, smallFont_, footerColor);
        const std::wstring overrideText = flag(rarestage::control::ManualSectionOverride)
            ? L"MANUAL ACT  " + std::to_wstring(static_cast<int>(std::ceil(state_.manualOverrideRemainingSeconds))) + L"s"
            : flag(rarestage::control::Automatic) ? L"AUTO ARMED" : L"MANUAL WORLD";
        drawText(dc, overrideText, RECT{footer.right - scale(220), footer.top, footer.right - scale(14), footer.bottom}, headingFont_, flag(rarestage::control::ManualSectionOverride) ? Amber : Cyan, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
        EndPaint(window_, &paintStruct);
    }

    HWND window_{};
    HWND rendererWindow_{};
    HANDLE mapping_{};
    const SharedState* mappedState_{};
    SharedState state_{};
    bool connected_{};
    bool compatible_{};
    UINT dpi_{96};
    std::uint32_t commandSerial_{};
    std::uint32_t pendingSerial_{};
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
    return app.run();
}

#pragma once

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace rarestage::control {

inline constexpr wchar_t MappingName[] = L"Local\\RareStage.Control.v1";
inline constexpr wchar_t RendererWindowClass[] = L"RareStageWindow";
inline constexpr UINT CommandMessage = WM_APP + 0x452;
inline constexpr std::uint32_t ProtocolVersion = 1;
inline constexpr std::uint32_t WorldCount = 9;
inline constexpr std::uint32_t SectionCount = 5;

enum class Command : std::uint32_t {
    Ping = 1,
    SelectWorld,
    PreviousWorld,
    NextWorld,
    SetAutomatic,
    ForceSection,
    ToggleFreeze,
    SetBlackout,
    CleanOpen,
    Fallback,
    LoadPreset,
    ToggleStageOverlay,
    SetProfile,
    SetSensitivityPercent
};

enum StateFlag : std::uint32_t {
    RendererReady = 1u << 0,
    Automatic = 1u << 1,
    Frozen = 1u << 2,
    Blackout = 1u << 3,
    StageOverlayVisible = 1u << 4,
    Timeline = 1u << 5,
    SpoutEnabled = 1u << 6,
    SpoutReady = 1u << 7,
    AudioAvailable = 1u << 8,
    ManualSectionOverride = 1u << 9,
    Fullscreen = 1u << 10
};

enum class CommandStatus : std::int32_t {
    None = 0,
    Accepted = 1,
    Rejected = -1
};

// This is a fixed-layout, read-mostly telemetry snapshot. The renderer is the
// only writer. Console uses the sequence field as a seqlock and never blocks
// the render thread.
struct alignas(64) SharedState {
    volatile LONG sequence{};
    std::uint32_t protocolVersion{ProtocolVersion};
    std::uint32_t structSize{};
    std::uint64_t heartbeatTickMs{};
    std::uint64_t stateRevision{};
    std::uint32_t rendererProcessId{};
    std::uint32_t flags{};
    std::int32_t world{};
    std::int32_t previousWorld{};
    std::int32_t songSection{};
    std::int32_t preset{};
    std::int32_t reactionProfile{};
    std::int32_t directorPalette{};
    std::uint32_t lastCommandSerial{};
    CommandStatus lastCommandStatus{CommandStatus::None};
    float fps{};
    float bpm{};
    float tempoConfidence{};
    float bass{};
    float mid{};
    float high{};
    float beat{};
    float onset{};
    float percussion{};
    float energy{};
    float transitionProgress{1.0f};
    float manualOverrideRemainingSeconds{};
    float sensitivity{1.0f};
    float bassGain{1.0f};
    float midGain{1.0f};
    float highGain{1.0f};
    float beatGain{1.0f};
    wchar_t playbackDevice[96]{};
};

static_assert(std::is_trivially_copyable_v<SharedState>);
static_assert(sizeof(SharedState) <= 1024);

inline LPARAM PackCommand(std::int32_t value, std::uint32_t serial) noexcept {
    const auto packed = (static_cast<std::uint64_t>(serial) << 32u) |
                        static_cast<std::uint32_t>(value);
    return static_cast<LPARAM>(packed);
}

inline std::int32_t CommandValue(LPARAM packed) noexcept {
    return static_cast<std::int32_t>(static_cast<std::uint64_t>(packed) & 0xffffffffu);
}

inline std::uint32_t CommandSerial(LPARAM packed) noexcept {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(packed) >> 32u);
}

inline void BeginWrite(SharedState* state) noexcept {
    InterlockedIncrement(&state->sequence);
    MemoryBarrier();
}

inline void EndWrite(SharedState* state) noexcept {
    MemoryBarrier();
    InterlockedIncrement(&state->sequence);
}

inline bool ReadConsistent(const SharedState* source, SharedState& destination) noexcept {
    if (!source) return false;
    for (int attempt = 0; attempt < 4; ++attempt) {
        const LONG before = InterlockedCompareExchange(
            const_cast<volatile LONG*>(&source->sequence), 0, 0);
        if (before & 1) continue;
        MemoryBarrier();
        std::memcpy(&destination, source, sizeof(destination));
        MemoryBarrier();
        const LONG after = InterlockedCompareExchange(
            const_cast<volatile LONG*>(&source->sequence), 0, 0);
        if (before == after && !(after & 1)) return true;
    }
    return false;
}

} // namespace rarestage::control

#pragma once

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace rarestage::control {

inline constexpr wchar_t MappingName[] = L"Local\\RareStage.Control.v11";
inline constexpr wchar_t RendererWindowClass[] = L"RareStageWindow";
inline constexpr UINT CommandMessage = WM_APP + 0x452;
inline constexpr std::uint32_t ProtocolVersion = 11;
inline constexpr std::uint32_t WorldCount = 18;
inline constexpr std::uint32_t SectionCount = 5;
inline constexpr std::uint32_t PaletteCount = 4;
inline constexpr std::uint32_t ProfileCount = 4;
inline constexpr std::uint32_t PresetCount = 4;
inline constexpr std::uint32_t TransitionStyleCount = 4;
inline constexpr std::uint32_t QualityCount = 4;
inline constexpr std::uint32_t AudioProfileCount = 4;
inline constexpr std::uint32_t MasteringProfileCount = 4;
inline constexpr std::uint32_t StageLayoutCount = 4;
inline constexpr std::uint32_t ProductionDutyCount = 5;
inline constexpr std::uint32_t SyncModeCount = 3;

enum class Command : std::uint32_t {
    Ping = 1,
    SelectWorld,
    PreviousWorld,
    NextWorld,
    SetAutomatic,
    ForceSection,
    ClearSectionOverride,
    SetFrozen,
    SetBlackout,
    CleanOpen,
    Fallback,
    LoadPreset,
    SavePreset,
    SetStageOverlay,
    SetProfile,
    SetPalette,
    SetSensitivityPercent,
    SetBassGainPercent,
    SetMidGainPercent,
    SetHighGainPercent,
    SetBeatGainPercent,
    SetLogoScalePercent,
    SetWorldDwellSeconds,
    SetTransitionTenths,
    SetTransitionStyle,
    SetQuality,
    SetVsync,
    SetTimeline,
    SetSpout,
    SetAudioProfile,
    CycleAudioDevice,
    CycleMonitor,
    SetFullscreen,
    QuitRenderer,
    CaptureFrame,
    SetCueDensityPercent,
    SetRehearsalActive,
    SetMasteringProfile,
    SetMasterBrightnessPercent,
    SetStageLayout,
    ApplyProductionDuty,
    SetSyncMode,
    SetSyncGroup
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
    Fullscreen = 1u << 10,
    VsyncEnabled = 1u << 11,
    MusicalEnding = 1u << 12,
    CapturePending = 1u << 13,
    RehearsalActive = 1u << 14,
    PredictiveCueArmed = 1u << 15,
    SyncConnected = 1u << 16,
    SyncLeader = 1u << 17,
    SyncFollower = 1u << 18,
    LedSafeMaster = 1u << 19
};

enum class CommandStatus : std::int32_t {
    None = 0,
    Accepted = 1,
    Rejected = -1
};

// Fixed-layout telemetry snapshot. RareStage is the only writer. Console uses
// the sequence field as a seqlock and never blocks the renderer thread.
struct alignas(64) SharedState {
    volatile LONG sequence{};
    std::uint32_t protocolVersion{ProtocolVersion};
    std::uint32_t structSize{};
    std::uint32_t worldCount{WorldCount};
    std::uint32_t sectionCount{SectionCount};
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
    std::int32_t fallbackPalette{};
    std::int32_t transitionStyle{};
    std::int32_t quality{};
    std::int32_t audioProfile{};
    std::int32_t resolvedAudioProfile{1};
    std::int32_t monitor{};
    std::int32_t monitorCount{1};
    std::int32_t directorCandidateWorld{};
    std::int32_t directorRunnerUpWorld{};
    std::int32_t predictedSection{};
    std::int32_t preparedWorld{-1};
    std::int32_t masteringProfile{1};
    std::int32_t stageLayout{};
    std::int32_t productionDuty{1};
    std::int32_t syncMode{};
    std::int32_t syncGroup{1};
    std::int32_t worldFamily{};
    std::int32_t worldMovement{};
    std::int32_t patternFrom{};
    std::int32_t patternTo{};
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
    float snare{};
    float kick{};
    float kickConfidence{};
    float snareConfidence{};
    float hatConfidence{};
    float kickEvidence{};
    float snareEvidence{};
    float hatEvidence{};
    float transientAmbiguity{};
    float detectorThreshold{};
    float calibrationStability{};
    float analysisLatencyMs{};
    float lowFluxShare{};
    float midFluxShare{};
    float highFluxShare{};
    float lowPowerShare{};
    float midPowerShare{};
    float highPowerShare{};
    float calibrationProgress{};
    float sampleRate{};
    float energy{};
    float transitionProgress{1.0f};
    float manualOverrideRemainingSeconds{};
    float sensitivity{1.0f};
    float bassGain{1.0f};
    float midGain{1.0f};
    float highGain{1.0f};
    float beatGain{1.0f};
    float logoScale{1.0f};
    float worldDwellSeconds{72.0f};
    float transitionSeconds{1.4f};
    float directorCandidateScore{};
    float directorRunnerUpScore{};
    float sectionEvidence{};
    float endingConfidence{};
    float gpuFrameMs{};
    float gpuPeakMs{};
    float gpuBudgetMs{};
    float gpuBudgetUtilization{};
    float cueDensity{1.0f};
    float worldCueDensity{1.0f};
    float rehearsalSeconds{};
    float predictedSectionConfidence{};
    float predictionLeadBeats{};
    float adaptiveCueDensity{1.0f};
    float tempoRecoveryConfidence{};
    float masterBrightness{0.92f};
    float syncPhaseErrorMs{};
    float syncPacketAgeMs{};
    float sectionAgeBeats{};
    float buildTension{};
    float dropReadiness{};
    float musicalNovelty{};
    float songArc{};
    float movementProgress{};
    float patternBlend{};
    float transitionCompatibility{1.0f};
    std::uint32_t transientClass{};
    std::uint32_t acceptedKickCount{};
    std::uint32_t acceptedSnareCount{};
    std::uint32_t acceptedHatCount{};
    std::uint32_t rejectedTransientCount{};
    std::uint32_t captureCount{};
    std::uint32_t rehearsalRevision{};
    std::uint32_t directorDecisionRevision{};
    std::uint32_t acceptedDirectorCueCount{};
    std::uint32_t rejectedDirectorCueCount{};
    std::uint32_t syncSequence{};
    std::int32_t captureStatus{};
    wchar_t playbackDevice[96]{};
    wchar_t worldName[48]{};
    wchar_t directorReason[96]{};
    wchar_t lastCapturePath[160]{};
    wchar_t lastDirectorDecision[96]{};
    wchar_t lastRejectedCue[96]{};
    wchar_t syncStatus[64]{};
    wchar_t movementName[32]{};
    wchar_t patternName[48]{};
};

static_assert(sizeof(LPARAM) >= sizeof(std::uint64_t), "RareStage Console requires Windows x64");
static_assert(std::is_trivially_copyable_v<SharedState>);
static_assert(sizeof(SharedState) <= 2048);

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
    for (int attempt = 0; attempt < 6; ++attempt) {
        // The Console maps this snapshot with FILE_MAP_READ. Do not use an
        // Interlocked read-modify-write operation here: even exchanging a
        // value with itself faults on a read-only mapped view. Aligned 32-bit
        // loads are atomic on Windows, and MSVC volatile loads have acquire
        // semantics; the barriers keep the payload copy between the two
        // sequence observations.
        const LONG before = source->sequence;
        if (before & 1) continue;
        MemoryBarrier();
        std::memcpy(&destination, source, sizeof(destination));
        MemoryBarrier();
        const LONG after = source->sequence;
        if (before == after && !(after & 1)) return true;
    }
    return false;
}

} // namespace rarestage::control

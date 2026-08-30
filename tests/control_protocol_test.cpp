#include "rarestage_control.h"

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <iostream>

int main() {
    using namespace rarestage::control;

    constexpr std::uint32_t serial = 0xfedcba98u;
    constexpr std::int32_t value = -42;
    const LPARAM packed = PackCommand(value, serial);
    if (CommandValue(packed) != value || CommandSerial(packed) != serial) {
        std::cerr << "Command packing failed\n";
        return 1;
    }

    if (WorldCount != 18 || SectionCount != 5 || ProtocolVersion != 7 || AudioProfileCount != 4) {
        std::cerr << "Protocol constants are stale\n";
        return 2;
    }

    SharedState source{};
    source.protocolVersion = ProtocolVersion;
    source.structSize = sizeof(SharedState);
    source.worldCount = WorldCount;
    source.sectionCount = SectionCount;
    BeginWrite(&source);
    source.heartbeatTickMs = 123456;
    source.stateRevision = 9;
    source.world = 13;
    source.songSection = 2;
    source.flags = RendererReady | Automatic | AudioAvailable | VsyncEnabled;
    source.bass = 0.75f;
    source.kick = 0.82f;
    source.kickConfidence = 0.91f;
    source.kickEvidence = 0.88f;
    source.snareEvidence = 0.24f;
    source.hatEvidence = 0.11f;
    source.transientAmbiguity = 0.08f;
    source.detectorThreshold = 0.29f;
    source.calibrationStability = 0.93f;
    source.analysisLatencyMs = 42.67f;
    source.lowFluxShare = 0.61f;
    source.midFluxShare = 0.27f;
    source.highFluxShare = 0.12f;
    source.lowPowerShare = 0.52f;
    source.midPowerShare = 0.33f;
    source.highPowerShare = 0.15f;
    source.transientClass = 1;
    source.acceptedKickCount = 12;
    source.acceptedSnareCount = 8;
    source.acceptedHatCount = 21;
    source.rejectedTransientCount = 3;
    source.audioProfile = 2;
    source.resolvedAudioProfile = 2;
    source.sampleRate = 48000.0f;
    source.directorCandidateWorld = 15;
    source.directorRunnerUpWorld = 3;
    source.directorCandidateScore = 1.72f;
    source.directorRunnerUpScore = 1.51f;
    source.sectionEvidence = 0.84f;
    source.endingConfidence = 0.62f;
    source.gpuFrameMs = 5.25f;
    source.gpuPeakMs = 7.75f;
    source.gpuBudgetMs = 8.0f;
    source.gpuBudgetUtilization = 0.65625f;
    source.captureCount = 4;
    source.captureStatus = 1;
    wcscpy_s(source.directorReason, _countof(source.directorReason), L"ENDING HOLD / PRESERVE WORLD");
    wcscpy_s(source.lastCapturePath, _countof(source.lastCapturePath), L"C:\\RareStage\\Captures\\frame.png");
    source.percussion = 0.50f;
    source.snare = 0.64f;
    source.logoScale = 1.15f;
    source.worldDwellSeconds = 64.0f;
    source.transitionSeconds = 1.2f;
    source.lastCommandSerial = serial;
    source.lastCommandStatus = CommandStatus::Accepted;
    EndWrite(&source);

    SharedState copy{};
    if (!ReadConsistent(&source, copy)) {
        std::cerr << "Consistent shared-state read failed\n";
        return 3;
    }
    if (copy.protocolVersion != ProtocolVersion || copy.structSize != sizeof(SharedState) ||
        copy.worldCount != WorldCount || copy.sectionCount != SectionCount ||
        copy.world != 13 || copy.songSection != 2 || copy.stateRevision != 9 ||
        copy.lastCommandSerial != serial || copy.lastCommandStatus != CommandStatus::Accepted ||
        copy.kick != 0.82f || copy.kickConfidence != 0.91f || copy.snare != 0.64f ||
        copy.kickEvidence != 0.88f || copy.snareEvidence != 0.24f ||
        copy.hatEvidence != 0.11f || copy.transientAmbiguity != 0.08f ||
        copy.detectorThreshold != 0.29f || copy.calibrationStability != 0.93f ||
        copy.analysisLatencyMs != 42.67f || copy.lowFluxShare != 0.61f ||
        copy.midFluxShare != 0.27f || copy.highFluxShare != 0.12f ||
        copy.lowPowerShare != 0.52f || copy.midPowerShare != 0.33f ||
        copy.highPowerShare != 0.15f || copy.transientClass != 1 ||
        copy.acceptedKickCount != 12 || copy.acceptedSnareCount != 8 ||
        copy.acceptedHatCount != 21 || copy.rejectedTransientCount != 3 ||
        copy.audioProfile != 2 || copy.resolvedAudioProfile != 2 || copy.sampleRate != 48000.0f ||
        copy.directorCandidateWorld != 15 || copy.directorRunnerUpWorld != 3 ||
        copy.directorCandidateScore != 1.72f || copy.directorRunnerUpScore != 1.51f ||
        copy.sectionEvidence != 0.84f || copy.endingConfidence != 0.62f ||
        copy.gpuFrameMs != 5.25f || copy.gpuPeakMs != 7.75f || copy.gpuBudgetMs != 8.0f ||
        copy.gpuBudgetUtilization != 0.65625f || copy.captureCount != 4 || copy.captureStatus != 1 ||
        std::wcscmp(copy.directorReason, L"ENDING HOLD / PRESERVE WORLD") != 0 ||
        std::wcscmp(copy.lastCapturePath, L"C:\\RareStage\\Captures\\frame.png") != 0 ||
        copy.logoScale != 1.15f ||
        copy.worldDwellSeconds != 64.0f || copy.transitionSeconds != 1.2f) {
        std::cerr << "Shared-state round trip failed\n";
        return 4;
    }
    if (copy.sequence & 1) {
        std::cerr << "Shared-state sequence remained in the write phase\n";
        return 5;
    }

    // Regression coverage for the v0.10.1 Console crash: the reader view is
    // intentionally read-only, exactly as it is in RareStageConsole.exe.
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                        0, sizeof(SharedState), nullptr);
    if (!mapping) {
        std::cerr << "Read-only mapping creation failed\n";
        return 6;
    }
    auto* writable = static_cast<SharedState*>(
        MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(SharedState)));
    const auto* readonly = static_cast<const SharedState*>(
        MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(SharedState)));
    if (!writable || !readonly) {
        if (readonly) UnmapViewOfFile(readonly);
        if (writable) UnmapViewOfFile(writable);
        CloseHandle(mapping);
        std::cerr << "Read-only mapping views failed\n";
        return 7;
    }
    std::memcpy(writable, &source, sizeof(source));
    SharedState readonlyCopy{};
    const bool readonlyRead = ReadConsistent(readonly, readonlyCopy);
    UnmapViewOfFile(readonly);
    UnmapViewOfFile(writable);
    CloseHandle(mapping);
    if (!readonlyRead || readonlyCopy.world != source.world ||
        readonlyCopy.stateRevision != source.stateRevision) {
        std::cerr << "Read-only shared-state regression test failed\n";
        return 8;
    }

    return 0;
}

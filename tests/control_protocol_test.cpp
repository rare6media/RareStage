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

    if (WorldCount != 18 || SectionCount != 5 || ProtocolVersion != 5 || AudioProfileCount != 4) {
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
    source.audioProfile = 2;
    source.resolvedAudioProfile = 2;
    source.sampleRate = 48000.0f;
    source.directorCandidateWorld = 15;
    source.directorRunnerUpWorld = 3;
    source.directorCandidateScore = 1.72f;
    source.directorRunnerUpScore = 1.51f;
    source.sectionEvidence = 0.84f;
    source.endingConfidence = 0.62f;
    wcscpy_s(source.directorReason, _countof(source.directorReason), L"ENDING HOLD / PRESERVE WORLD");
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
        copy.audioProfile != 2 || copy.resolvedAudioProfile != 2 || copy.sampleRate != 48000.0f ||
        copy.directorCandidateWorld != 15 || copy.directorRunnerUpWorld != 3 ||
        copy.directorCandidateScore != 1.72f || copy.directorRunnerUpScore != 1.51f ||
        copy.sectionEvidence != 0.84f || copy.endingConfidence != 0.62f ||
        std::wcscmp(copy.directorReason, L"ENDING HOLD / PRESERVE WORLD") != 0 ||
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

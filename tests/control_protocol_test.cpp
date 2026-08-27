#include "rarestage_control.h"

#include <cstdint>
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

    if (WorldCount != 15 || SectionCount != 5 || ProtocolVersion != 2) {
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
    source.percussion = 0.50f;
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
        copy.kick != 0.82f || copy.logoScale != 1.15f ||
        copy.worldDwellSeconds != 64.0f || copy.transitionSeconds != 1.2f) {
        std::cerr << "Shared-state round trip failed\n";
        return 4;
    }
    if (copy.sequence & 1) {
        std::cerr << "Shared-state sequence remained in the write phase\n";
        return 5;
    }

    return 0;
}


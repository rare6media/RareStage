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

    SharedState source{};
    source.protocolVersion = ProtocolVersion;
    source.structSize = sizeof(SharedState);
    BeginWrite(&source);
    source.heartbeatTickMs = 123456;
    source.stateRevision = 9;
    source.world = 4;
    source.songSection = 2;
    source.flags = RendererReady | Automatic | AudioAvailable;
    source.bass = 0.75f;
    source.percussion = 0.50f;
    source.lastCommandSerial = serial;
    source.lastCommandStatus = CommandStatus::Accepted;
    EndWrite(&source);

    SharedState copy{};
    if (!ReadConsistent(&source, copy)) {
        std::cerr << "Consistent shared-state read failed\n";
        return 2;
    }
    if (copy.protocolVersion != ProtocolVersion || copy.structSize != sizeof(SharedState) ||
        copy.world != 4 || copy.songSection != 2 || copy.stateRevision != 9 ||
        copy.lastCommandSerial != serial || copy.lastCommandStatus != CommandStatus::Accepted) {
        std::cerr << "Shared-state round trip failed\n";
        return 3;
    }
    if (copy.sequence & 1) {
        std::cerr << "Shared-state sequence remained in the write phase\n";
        return 4;
    }

    return 0;
}


#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace somavr::per_eye_view_history_math {

constexpr size_t kViewHistoryPacketSize = 0x40;
using ViewHistoryPacket = std::array<uint8_t, kViewHistoryPacketSize>;

struct Bank {
    bool active = false;
    uintptr_t rendererIdentity = 0;
    uintptr_t historyIdentity = 0;
    std::array<ViewHistoryPacket, 2> packets{};
    std::array<bool, 2> valid{};
    std::array<uint64_t, 2> poseFrames{};
};

struct PrepareResult {
    bool valid = false;
    bool reset = false;
    bool seeded = false;
    ViewHistoryPacket restorePacket{};
};

bool SetActive(Bank& bank, bool active);
PrepareResult Prepare(
    Bank& bank,
    uintptr_t rendererIdentity,
    uintptr_t historyIdentity,
    int eyeIndex,
    uint64_t poseFrame,
    const ViewHistoryPacket& livePacket);
bool Commit(
    Bank& bank,
    uintptr_t rendererIdentity,
    uintptr_t historyIdentity,
    int eyeIndex,
    uint64_t poseFrame,
    const ViewHistoryPacket& packet);
void Reset(Bank& bank);

} // namespace somavr::per_eye_view_history_math

#include "HPLPerEyeViewHistoryMath.h"

namespace somavr::per_eye_view_history_math {
namespace {

bool IsValidIdentity(uintptr_t rendererIdentity, uintptr_t historyIdentity)
{
    return rendererIdentity != 0 && historyIdentity != 0;
}

bool IsValidEye(int eyeIndex)
{
    return eyeIndex == 0 || eyeIndex == 1;
}

void Seed(
    Bank& bank,
    uintptr_t rendererIdentity,
    uintptr_t historyIdentity,
    const ViewHistoryPacket& livePacket)
{
    bank.rendererIdentity = rendererIdentity;
    bank.historyIdentity = historyIdentity;
    bank.packets[0] = livePacket;
    bank.packets[1] = livePacket;
    bank.valid = {true, true};
    bank.poseFrames = {};
}

} // namespace

bool SetActive(Bank& bank, bool active)
{
    if (bank.active == active) {
        return false;
    }
    Reset(bank);
    bank.active = active;
    return true;
}

PrepareResult Prepare(
    Bank& bank,
    uintptr_t rendererIdentity,
    uintptr_t historyIdentity,
    int eyeIndex,
    uint64_t poseFrame,
    const ViewHistoryPacket& livePacket)
{
    PrepareResult result;
    if (!bank.active
        || !IsValidIdentity(rendererIdentity, historyIdentity)
        || !IsValidEye(eyeIndex)
        || poseFrame == 0) {
        return result;
    }

    const size_t eye = static_cast<size_t>(eyeIndex);
    const bool identityChanged = bank.rendererIdentity != rendererIdentity
        || bank.historyIdentity != historyIdentity;
    const bool poseRegressed = !identityChanged
        && bank.poseFrames[eye] != 0
        && poseFrame < bank.poseFrames[eye];
    if (identityChanged || poseRegressed || !bank.valid[eye]) {
        Seed(bank, rendererIdentity, historyIdentity, livePacket);
        result.reset = identityChanged || poseRegressed;
        result.seeded = true;
    }

    result.valid = bank.valid[eye];
    if (result.valid) {
        result.restorePacket = bank.packets[eye];
    }
    return result;
}

bool Commit(
    Bank& bank,
    uintptr_t rendererIdentity,
    uintptr_t historyIdentity,
    int eyeIndex,
    uint64_t poseFrame,
    const ViewHistoryPacket& packet)
{
    if (!bank.active
        || bank.rendererIdentity != rendererIdentity
        || bank.historyIdentity != historyIdentity
        || !IsValidEye(eyeIndex)
        || poseFrame == 0) {
        return false;
    }

    const size_t eye = static_cast<size_t>(eyeIndex);
    bank.packets[eye] = packet;
    bank.valid[eye] = true;
    bank.poseFrames[eye] = poseFrame;
    return true;
}

void Reset(Bank& bank)
{
    bank = {};
}

} // namespace somavr::per_eye_view_history_math

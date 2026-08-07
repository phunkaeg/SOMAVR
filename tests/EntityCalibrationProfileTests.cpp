#include "HPLEntityCalibrationProfiles.h"

#include <Windows.h>

#include <cmath>
#include <filesystem>
#include <iostream>

namespace {

bool Near(float left, float right)
{
    return std::fabs(left - right) <= 1.0e-6f;
}

} // namespace

int main()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / (L"somavr-entity-profile-test-" + std::to_wstring(GetCurrentProcessId()));
    const std::filesystem::path path = root / L"profiles.ini";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);

    somavr::Config config;
    config.hplHandRootOffsetY = -0.075f;
    config.hplHandTargetScale = 1.0f;
    config.hplFlashlightOffsetZ = 0.03f;
    somavr::entity_calibration::Store first;
    first.Initialize(config, path);

    somavr::entity_calibration::Capabilities handsCapabilities;
    handsCapabilities.playerHands = true;
    const auto hands = first.Resolve("PlayerHands_0", handsCapabilities);
    const auto handsAgain = first.Resolve("PlayerHands_0", handsCapabilities);
    if (!hands.valid || hands.loadedFromDisk || hands.id != handsAgain.id
        || hands.family != somavr::entity_calibration::Family::PlayerHands
        || !Near(hands.values.offsetY, -0.075f)
        || !Near(hands.values.scale, 1.0f)) {
        std::cerr << "FAILED: seed/cache player hands profile\n";
        return 1;
    }

    somavr::entity_calibration::Values tuned = hands.values;
    tuned.offsetY = -0.123f;
    if (!first.Update("PlayerHands_0", tuned) || !first.Save()) {
        std::cerr << "FAILED: update/save profile\n";
        return 1;
    }

    somavr::entity_calibration::Store second;
    second.Initialize(config, path);
    const auto loaded = second.Resolve("PlayerHands_0", handsCapabilities);
    const bool passed = loaded.valid
        && loaded.loadedFromDisk
        && loaded.family == somavr::entity_calibration::Family::PlayerHands
        && Near(loaded.values.offsetY, -0.123f);
    second.Reset();
    std::filesystem::remove_all(root, ec);
    if (!passed) {
        std::cerr << "FAILED: reload exact profile\n";
        return 1;
    }
    std::cout << "Entity calibration profile tests passed\n";
    return 0;
}

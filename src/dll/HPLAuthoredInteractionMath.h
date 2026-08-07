#pragma once

#include "HPLCameraMath.h"

#include <array>
#include <cstdint>

namespace somavr::authored_interaction_math {

enum class MedicineStage : uint8_t {
    AwaitBottle,
    AwaitCap,
    AwaitDrink,
    Complete,
};

struct MedicineSettings {
    camera_math::Vector3 capLocalOffset{0.0f, 0.08f, 0.0f};
    camera_math::Vector3 bottleUpLocal{0.0f, 1.0f, 0.0f};
    float capProximity = 0.10f;
    float mouthProximity = 0.16f;
    float drinkTipDegrees = 65.0f;
    uint32_t drinkHoldFrames = 12;
};

struct MedicineState {
    MedicineStage stage = MedicineStage::AwaitBottle;
    uint32_t drinkFrames = 0;
};

struct MedicineFrame {
    bool bottleValid = false;
    std::array<float, 16> bottleWorld{};
    bool leftHandValid = false;
    camera_math::Vector3 leftHandPosition{};
    bool leftActionPressed = false;
    bool headValid = false;
    camera_math::Vector3 headPosition{};
};

struct MedicineResult {
    MedicineStage stage = MedicineStage::AwaitBottle;
    camera_math::Vector3 capPosition{};
    float capDistance = 0.0f;
    float mouthDistance = 0.0f;
    float tipDegrees = 0.0f;
    bool capInRange = false;
    bool mouthInRange = false;
    bool tipped = false;
    bool capRemoved = false;
    bool drinkCompleted = false;
};

bool UpdateMedicine(
    MedicineState& state,
    const MedicineSettings& settings,
    const MedicineFrame& frame,
    MedicineResult& result);

const char* MedicineStageName(MedicineStage stage);

} // namespace somavr::authored_interaction_math

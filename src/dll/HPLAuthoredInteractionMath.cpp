#include "HPLAuthoredInteractionMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::authored_interaction_math {
namespace {

constexpr float kPi = 3.14159265358979323846f;

bool IsFinite(const camera_math::Vector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float Distance(const camera_math::Vector3& a, const camera_math::Vector3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return std::sqrt(x*x + y*y + z*z);
}

camera_math::Vector3 TransformPoint(
    const std::array<float, 16>& matrix,
    const camera_math::Vector3& point)
{
    return {
        matrix[0] * point.x + matrix[1] * point.y + matrix[2] * point.z + matrix[3],
        matrix[4] * point.x + matrix[5] * point.y + matrix[6] * point.z + matrix[7],
        matrix[8] * point.x + matrix[9] * point.y + matrix[10] * point.z + matrix[11],
    };
}

camera_math::Vector3 TransformDirection(
    const std::array<float, 16>& matrix,
    const camera_math::Vector3& direction)
{
    return {
        matrix[0] * direction.x + matrix[1] * direction.y + matrix[2] * direction.z,
        matrix[4] * direction.x + matrix[5] * direction.y + matrix[6] * direction.z,
        matrix[8] * direction.x + matrix[9] * direction.y + matrix[10] * direction.z,
    };
}

bool Normalize(camera_math::Vector3& value)
{
    const float length = std::sqrt(value.x*value.x + value.y*value.y + value.z*value.z);
    if (!std::isfinite(length) || length <= 1.0e-6f) return false;
    value.x /= length;
    value.y /= length;
    value.z /= length;
    return true;
}

} // namespace

bool UpdateMedicine(
    MedicineState& state,
    const MedicineSettings& settings,
    const MedicineFrame& frame,
    MedicineResult& result)
{
    result = {};
    result.stage = state.stage;
    if (!frame.bottleValid || !IsFinite(settings.capLocalOffset)
        || !IsFinite(settings.bottleUpLocal)
        || !std::isfinite(settings.capProximity)
        || !std::isfinite(settings.mouthProximity)
        || !std::isfinite(settings.drinkTipDegrees)
        || settings.capProximity <= 0.0f || settings.mouthProximity <= 0.0f
        || settings.drinkTipDegrees < 0.0f || settings.drinkTipDegrees > 180.0f
        || settings.drinkHoldFrames == 0) {
        return false;
    }
    for (float value : frame.bottleWorld) {
        if (!std::isfinite(value)) return false;
    }

    if (state.stage == MedicineStage::AwaitBottle) {
        state.stage = MedicineStage::AwaitCap;
    }

    result.capPosition = TransformPoint(frame.bottleWorld, settings.capLocalOffset);
    if (frame.leftHandValid && IsFinite(frame.leftHandPosition)) {
        result.capDistance = Distance(frame.leftHandPosition, result.capPosition);
        result.capInRange = result.capDistance <= settings.capProximity;
    }
    if (state.stage == MedicineStage::AwaitCap
        && result.capInRange && frame.leftActionPressed) {
        state.stage = MedicineStage::AwaitDrink;
        state.drinkFrames = 0;
        result.capRemoved = true;
    }

    camera_math::Vector3 bottleUp = TransformDirection(
        frame.bottleWorld, settings.bottleUpLocal);
    if (!Normalize(bottleUp)) return false;
    result.tipDegrees = std::acos(std::clamp(bottleUp.y, -1.0f, 1.0f)) * 180.0f / kPi;
    result.tipped = result.tipDegrees >= settings.drinkTipDegrees;
    if (frame.headValid && IsFinite(frame.headPosition)) {
        result.mouthDistance = Distance(result.capPosition, frame.headPosition);
        result.mouthInRange = result.mouthDistance <= settings.mouthProximity;
    }

    if (state.stage == MedicineStage::AwaitDrink) {
        if (result.mouthInRange && result.tipped) {
            ++state.drinkFrames;
        } else {
            state.drinkFrames = 0;
        }
        if (state.drinkFrames >= settings.drinkHoldFrames) {
            state.stage = MedicineStage::Complete;
            result.drinkCompleted = true;
        }
    }
    result.stage = state.stage;
    return true;
}

const char* MedicineStageName(MedicineStage stage)
{
    switch (stage) {
    case MedicineStage::AwaitBottle: return "await_bottle";
    case MedicineStage::AwaitCap: return "await_cap";
    case MedicineStage::AwaitDrink: return "await_drink";
    case MedicineStage::Complete: return "complete";
    }
    return "unknown";
}

} // namespace somavr::authored_interaction_math

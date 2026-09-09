#include "HPLGrabMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::grab_math {

camera_math::Vector3 ResolveAngularTargetVelocity(
    const camera_math::Quaternion& anchor,
    const camera_math::Quaternion& current,
    float gain,
    float sign,
    float maxAngularSpeed)
{
    using camera_math::Conjugate;
    using camera_math::Multiply;
    using camera_math::Normalize;
    using camera_math::Quaternion;
    using camera_math::Vector3;

    if (!std::isfinite(gain) || !std::isfinite(sign) || !std::isfinite(maxAngularSpeed)
        || gain <= 0.0f || std::fabs(sign) < 0.001f || maxAngularSpeed <= 0.0f) {
        return {};
    }

    Quaternion delta = Normalize(Multiply(Normalize(current), Conjugate(Normalize(anchor))));
    if (delta.w < 0.0f) {
        delta.x = -delta.x;
        delta.y = -delta.y;
        delta.z = -delta.z;
        delta.w = -delta.w;
    }

    const float halfAngleSin = std::sqrt(std::max(
        0.0f,
        delta.x * delta.x + delta.y * delta.y + delta.z * delta.z));
    if (!std::isfinite(halfAngleSin) || halfAngleSin < 1.0e-6f) {
        return {};
    }

    const float angle = 2.0f * std::atan2(halfAngleSin, std::clamp(delta.w, -1.0f, 1.0f));
    const float speed = std::min(angle * gain, maxAngularSpeed) * sign;
    const float axisScale = speed / halfAngleSin;
    const Vector3 result{delta.x * axisScale, delta.y * axisScale, delta.z * axisScale};
    if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z)) {
        return {};
    }
    return result;
}

camera_math::Quaternion ResolveRelativeOrientationTarget(
    const camera_math::Quaternion& anchorController,
    const camera_math::Quaternion& currentController,
    const camera_math::Quaternion& anchorObject)
{
    return camera_math::Normalize(camera_math::Multiply(
        camera_math::Multiply(
            camera_math::Normalize(currentController),
            camera_math::Conjugate(camera_math::Normalize(anchorController))),
        camera_math::Normalize(anchorObject)));
}

camera_math::Vector3 ClampVectorMagnitude(
    const camera_math::Vector3& value,
    float maximumMagnitude)
{
    if (!std::isfinite(value.x) || !std::isfinite(value.y)
        || !std::isfinite(value.z) || !std::isfinite(maximumMagnitude)
        || maximumMagnitude <= 0.0f) {
        return {};
    }
    const float lengthSquared =
        value.x * value.x + value.y * value.y + value.z * value.z;
    if (!std::isfinite(lengthSquared)
        || lengthSquared <= maximumMagnitude * maximumMagnitude) {
        return value;
    }
    const float scale = maximumMagnitude / std::sqrt(lengthSquared);
    return {value.x * scale, value.y * scale, value.z * scale};
}

camera_math::Vector3 ResolveGrabPositionCorrection(
    const camera_math::Vector3& initialHandCorrection,
    const camera_math::Vector3& controllerMovement,
    float movementScale,
    float maximumMovement)
{
    if (!std::isfinite(initialHandCorrection.x)
        || !std::isfinite(initialHandCorrection.y)
        || !std::isfinite(initialHandCorrection.z)
        || !std::isfinite(controllerMovement.x)
        || !std::isfinite(controllerMovement.y)
        || !std::isfinite(controllerMovement.z)
        || !std::isfinite(movementScale)
        || !std::isfinite(maximumMovement)
        || movementScale < 0.0f
        || maximumMovement <= 0.0f) {
        return {};
    }
    const camera_math::Vector3 scaledMovement{
        controllerMovement.x * movementScale,
        controllerMovement.y * movementScale,
        controllerMovement.z * movementScale,
    };
    const camera_math::Vector3 boundedMovement =
        ClampVectorMagnitude(scaledMovement, maximumMovement);
    return {
        initialHandCorrection.x + boundedMovement.x,
        initialHandCorrection.y + boundedMovement.y,
        initialHandCorrection.z + boundedMovement.z,
    };
}

float ResolveSlideTargetSpeed(
    float controllerVelocityAlongPin,
    float controllerDisplacementAlongPin,
    float bodyDisplacementAlongPin,
    float velocityScale,
    float positionGain,
    float maximumSpeed)
{
    if (!std::isfinite(controllerVelocityAlongPin)
        || !std::isfinite(controllerDisplacementAlongPin)
        || !std::isfinite(bodyDisplacementAlongPin)
        || !std::isfinite(velocityScale)
        || !std::isfinite(positionGain)
        || !std::isfinite(maximumSpeed)
        || velocityScale < 0.0f || positionGain < 0.0f
        || maximumSpeed <= 0.0f) {
        return 0.0f;
    }
    // Feed-forward and position feedback must follow the same scaled target.
    const float positionError =
        controllerDisplacementAlongPin * velocityScale - bodyDisplacementAlongPin;
    return std::clamp(
        controllerVelocityAlongPin * velocityScale + positionError * positionGain,
        -maximumSpeed,
        maximumSpeed);
}

camera_math::Vector3 RebaseRigidPoint(
    const camera_math::Vector3& point,
    const camera_math::Vector3& anchorPosition,
    const camera_math::Quaternion& anchorOrientation,
    const camera_math::Vector3& currentPosition,
    const camera_math::Quaternion& currentOrientation)
{
    const auto local = camera_math::RotateVector(
        camera_math::Conjugate(camera_math::Normalize(anchorOrientation)),
        {point.x - anchorPosition.x, point.y - anchorPosition.y, point.z - anchorPosition.z});
    const auto offset = camera_math::RotateVector(camera_math::Normalize(currentOrientation), local);
    return {currentPosition.x + offset.x, currentPosition.y + offset.y, currentPosition.z + offset.z};
}

bool IsPhysicalThrowRelease(
    bool velocityValid, const camera_math::Vector3& velocity, float velocityThreshold)
{
    const float speedSquared = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    const float threshold = std::max(0.8f, velocityThreshold);
    return velocityValid && std::isfinite(velocityThreshold) && std::isfinite(speedSquared)
        && speedSquared >= threshold * threshold;
}

NativeThrowDecision AdvanceNativeThrowHandoff(
    NativeThrowHandoff& state, uint64_t nowMs, bool throwState, bool request)
{
    NativeThrowDecision decision{};
    if (state.pending) {
        decision.timedOut = throwState && nowMs >= state.deadlineMs;
        if (!throwState || decision.timedOut) state = {};
    } else if (throwState && request) {
        state.pending = true;
        state.deadlineMs = nowMs + 250;
        decision.begin = true;
    }
    decision.holdButtons = state.pending;
    return decision;
}

float ResolveHingeAngularVelocity(
    const camera_math::Vector3& pivot,
    const camera_math::Vector3& point,
    const camera_math::Vector3& pointVelocity,
    const camera_math::Vector3& pin,
    float gain,
    float maxAngularSpeed)
{
    if (!std::isfinite(gain) || gain <= 0.0f
        || !std::isfinite(maxAngularSpeed) || maxAngularSpeed <= 0.0f) {
        return 0.0f;
    }
    const float pinLengthSquared = pin.x * pin.x + pin.y * pin.y + pin.z * pin.z;
    if (!std::isfinite(pinLengthSquared) || pinLengthSquared < 0.25f) {
        return 0.0f;
    }
    const float inversePinLength = 1.0f / std::sqrt(pinLengthSquared);
    const camera_math::Vector3 axis{
        pin.x * inversePinLength,
        pin.y * inversePinLength,
        pin.z * inversePinLength,
    };
    const camera_math::Vector3 pivotToPoint{
        point.x - pivot.x,
        point.y - pivot.y,
        point.z - pivot.z,
    };
    const float axial = pivotToPoint.x * axis.x
        + pivotToPoint.y * axis.y
        + pivotToPoint.z * axis.z;
    const camera_math::Vector3 radius{
        pivotToPoint.x - axis.x * axial,
        pivotToPoint.y - axis.y * axial,
        pivotToPoint.z - axis.z * axial,
    };
    const float radiusSquared = radius.x * radius.x
        + radius.y * radius.y
        + radius.z * radius.z;
    if (!std::isfinite(radiusSquared) || radiusSquared < 0.0025f) {
        return 0.0f;
    }
    const camera_math::Vector3 cross{
        radius.y * pointVelocity.z - radius.z * pointVelocity.y,
        radius.z * pointVelocity.x - radius.x * pointVelocity.z,
        radius.x * pointVelocity.y - radius.y * pointVelocity.x,
    };
    const float angularVelocity = (cross.x * axis.x + cross.y * axis.y + cross.z * axis.z)
        / radiusSquared * gain;
    if (!std::isfinite(angularVelocity)) return 0.0f;
    return std::clamp(angularVelocity, -maxAngularSpeed, maxAngularSpeed);
}

float CombineHingeAngularVelocity(
    float pointAngularVelocity,
    const camera_math::Vector3& controllerAngularVelocity,
    const camera_math::Vector3& pin,
    float controllerScale,
    float maxAngularSpeed)
{
    const float pinLengthSquared = pin.x * pin.x + pin.y * pin.y + pin.z * pin.z;
    if (!std::isfinite(pointAngularVelocity)
        || !std::isfinite(controllerScale)
        || !std::isfinite(maxAngularSpeed) || maxAngularSpeed <= 0.0f
        || !std::isfinite(pinLengthSquared) || pinLengthSquared < 0.25f) {
        return 0.0f;
    }
    const float inversePinLength = 1.0f / std::sqrt(pinLengthSquared);
    const float wrist = (
        controllerAngularVelocity.x * pin.x
        + controllerAngularVelocity.y * pin.y
        + controllerAngularVelocity.z * pin.z) * inversePinLength * controllerScale;
    if (!std::isfinite(wrist)) return std::clamp(
        pointAngularVelocity, -maxAngularSpeed, maxAngularSpeed);
    return std::clamp(
        pointAngularVelocity + wrist, -maxAngularSpeed, maxAngularSpeed);
}

float ResolveThrowVelocityScale(
    float controllerSpeed,
    float velocityThreshold,
    float velocityReference,
    bool enabled)
{
    if (!enabled
        || !std::isfinite(controllerSpeed)
        || !std::isfinite(velocityThreshold)
        || !std::isfinite(velocityReference)
        || controllerSpeed < velocityThreshold
        || velocityReference < 0.1f) {
        return 1.0f;
    }
    return std::clamp(controllerSpeed / velocityReference, 1.0f, 2.0f);
}

camera_math::Vector3 ResolveSafeThrowDirection(
    const camera_math::Vector3& requestedDirection,
    const camera_math::Vector3& cameraForward,
    float minimumForwardDot)
{
    const auto normalize = [](const camera_math::Vector3& value) {
        const float lengthSquared =
            value.x * value.x + value.y * value.y + value.z * value.z;
        if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-8f) {
            return camera_math::Vector3{};
        }
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        return camera_math::Vector3{
            value.x * inverseLength,
            value.y * inverseLength,
            value.z * inverseLength,
        };
    };
    if (!std::isfinite(minimumForwardDot)) return {};
    const float requiredDot = std::clamp(minimumForwardDot, 0.0f, 0.95f);
    const camera_math::Vector3 direction = normalize(requestedDirection);
    const camera_math::Vector3 forward = normalize(cameraForward);
    const float directionLengthSquared =
        direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    const float forwardLengthSquared =
        forward.x * forward.x + forward.y * forward.y + forward.z * forward.z;
    if (directionLengthSquared < 0.5f || forwardLengthSquared < 0.5f) return {};

    const float forwardDot = direction.x * forward.x
        + direction.y * forward.y + direction.z * forward.z;
    if (forwardDot >= requiredDot) return direction;

    camera_math::Vector3 perpendicular{
        direction.x - forward.x * forwardDot,
        direction.y - forward.y * forwardDot,
        direction.z - forward.z * forwardDot,
    };
    perpendicular = normalize(perpendicular);
    const float perpendicularLengthSquared =
        perpendicular.x * perpendicular.x
        + perpendicular.y * perpendicular.y
        + perpendicular.z * perpendicular.z;
    if (perpendicularLengthSquared < 0.5f) return forward;
    const float perpendicularScale = std::sqrt(1.0f - requiredDot * requiredDot);
    return {
        perpendicular.x * perpendicularScale + forward.x * requiredDot,
        perpendicular.y * perpendicularScale + forward.y * requiredDot,
        perpendicular.z * perpendicularScale + forward.z * requiredDot,
    };
}

} // namespace somavr::grab_math

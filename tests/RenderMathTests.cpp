#include "HPLCameraMath.h"
#include "HPLComfortMath.h"
#include "HPLFlashlightMath.h"
#include "HPLHandsMath.h"
#include "HPLGrabMath.h"
#include "HPLHudMath.h"
#include "HPLInputMath.h"
#include "HPLMenuMath.h"
#include "HPLPhysicalCrouchMath.h"
#include "OpenGLMatrixAnalysis.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string>

namespace {

bool Near(float left, float right, float epsilon = 1.0e-5f)
{
    return std::abs(left - right) <= epsilon;
}

int Check(bool condition, const char* message)
{
    if (condition) {
        return 0;
    }
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    using namespace somavr;

    int failures = 0;
    failures += Check(camera_math::ValidateStereoProjectionMath(), "projection self-test");
    failures += Check(
        Near(camera_math::ComputeRoomscaleSafetyFactor(0.75f, 1.0f, 0.10f), 0.65f),
        "room-scale safety retracts collision fraction by clearance");
    failures += Check(
        Near(camera_math::ComputeRoomscaleSafetyFactor(0.05f, 1.0f, 0.10f), 0.0f),
        "room-scale safety clearance clamps before nearby geometry");
    failures += Check(
        Near(camera_math::ComputeRoomscaleSafetyFactor(2.0f, 1.0f, 0.0f), 1.0f)
            && Near(camera_math::ComputeRoomscaleSafetyFactor(0.5f, 0.0f, 0.1f), 1.0f),
        "room-scale safety bounds fractions and preserves invalid zero-distance input");
    const camera_math::Vector3 safeEyeOffset = camera_math::ReplaceTrackedHeadTranslation(
        {0.34f, 0.12f, -0.20f},
        {0.30f, 0.10f, -0.20f},
        {0.15f, 0.05f, -0.10f});
    failures += Check(
        Near(safeEyeOffset.x, 0.19f)
            && Near(safeEyeOffset.y, 0.07f)
            && Near(safeEyeOffset.z, -0.10f),
        "room-scale safety replaces shared head motion while preserving eye-relative offset");

    failures += Check(
        comfort_math::ShouldSuppressCameraAdd(1, true, true, true),
        "comfort policy suppresses head bob");
    failures += Check(
        comfort_math::ShouldSuppressCameraAdd(2, true, true, true),
        "comfort policy suppresses camera shake");
    failures += Check(
        comfort_math::ShouldSuppressCameraAdd(9, true, true, true),
        "comfort policy suppresses sway");
    failures += Check(
        !comfort_math::ShouldSuppressCameraAdd(5, true, true, true)
            && !comfort_math::ShouldSuppressCameraAdd(7, true, true, true)
            && !comfort_math::ShouldSuppressCameraAdd(10, true, true, true),
        "comfort policy preserves script lean and conversation channels");
    failures += Check(
        !comfort_math::ShouldSuppressCameraAdd(1, false, true, true)
            && !comfort_math::ShouldSuppressCameraAdd(2, true, false, true)
            && !comfort_math::ShouldSuppressCameraAdd(9, true, true, false),
        "comfort policy honors independent channel switches");

    const input_math::Axis2 centeredStick = input_math::ApplyRadialDeadzone(0.2f, 0.1f, 0.35f);
    failures += Check(Near(centeredStick.x, 0.0f) && Near(centeredStick.y, 0.0f), "radial deadzone center");
    const input_math::Axis2 fullDiagonal = input_math::ApplyRadialDeadzone(0.70710678f, 0.70710678f, 0.35f);
    failures += Check(
        Near(std::sqrt(fullDiagonal.x * fullDiagonal.x + fullDiagonal.y * fullDiagonal.y), 1.0f),
        "radial deadzone preserves full diagonal magnitude");
    const input_math::Axis2 halfStick = input_math::ApplyRadialDeadzone(0.0f, 0.675f, 0.35f);
    failures += Check(Near(halfStick.x, 0.0f) && Near(halfStick.y, 0.5f), "radial deadzone rescales magnitude");
    failures += Check(Near(input_math::DegreesToRadians(30.0f), 0.5235988f), "snap-turn degree conversion");
    constexpr float kHalfSqrtTwo = 0.7071067811865475f;
    const input_math::Axis2 headRightMovement = input_math::ApplyHeadRelativeMovement(
        0.0f,
        1.0f,
        {0.0f, -kHalfSqrtTwo, 0.0f, kHalfSqrtTwo});
    failures += Check(
        Near(headRightMovement.x, 1.0f) && Near(headRightMovement.y, 0.0f),
        "head-relative forward follows rightward head yaw");
    const input_math::Axis2 pitchedHeadMovement = input_math::ApplyHeadRelativeMovement(
        0.0f,
        1.0f,
        {kHalfSqrtTwo, 0.0f, 0.0f, kHalfSqrtTwo});
    failures += Check(
        Near(pitchedHeadMovement.x, 0.0f) && Near(pitchedHeadMovement.y, 1.0f),
        "head-relative movement ignores head pitch");

    input_math::ManipulationMotionState manipulationState;
    const input_math::ManipulationMouseDelta manipulationRight =
        input_math::ComputeManipulationMouseDelta(
            {}, {0.015625f, 0.0f, 0.0f}, {}, 1024.0f, 0.0005f, 80, 1.0f, -1.0f,
            manipulationState);
    failures += Check(
        manipulationRight.x == 16 && manipulationRight.y == 0
            && Near(manipulationRight.rightMeters, 0.015625f),
        "physical manipulation projects head-right hand motion");
    const input_math::ManipulationMouseDelta manipulationUp =
        input_math::ComputeManipulationMouseDelta(
            {}, {0.0f, 0.03125f, 0.0f}, {}, 1024.0f, 0.0005f, 80, 1.0f, -1.0f,
            manipulationState);
    failures += Check(
        manipulationUp.x == 0 && manipulationUp.y == -32
            && Near(manipulationUp.upMeters, 0.03125f),
        "physical manipulation maps hand-up to mouse-up");
    input_math::ManipulationMotionState cappedManipulationState;
    const input_math::ManipulationMouseDelta cappedManipulation =
        input_math::ComputeManipulationMouseDelta(
            {}, {1.0f, -1.0f, 0.0f}, {}, 1000.0f, 0.0005f, 80, 1.0f, -1.0f,
            cappedManipulationState);
    failures += Check(
        cappedManipulation.x == 80 && cappedManipulation.y == 80,
        "physical manipulation caps per-frame mouse deltas without backlog");
    const input_math::ManipulationMouseDelta deadzoneManipulation =
        input_math::ComputeManipulationMouseDelta(
            {}, {0.0001f, 0.0001f, 0.0f}, {}, 1000.0f, 0.0005f, 80, 1.0f, -1.0f,
            cappedManipulationState);
    failures += Check(
        deadzoneManipulation.x == 0 && deadzoneManipulation.y == 0,
        "physical manipulation ignores sub-deadzone jitter");

    crouch_math::PhysicalCrouchState crouchState;
    failures += Check(
        crouch_math::UpdatePhysicalCrouch(crouchState, 1.70f, true, 1, 0.35f, 0.25f)
            == crouch_math::PhysicalCrouchUpdate::Calibrated,
        "physical crouch calibrates standing height");
    failures += Check(
        crouch_math::UpdatePhysicalCrouch(crouchState, 1.34f, true, 1, 0.35f, 0.25f)
            == crouch_math::PhysicalCrouchUpdate::Enter,
        "physical crouch enters below threshold");
    failures += Check(
        crouch_math::UpdatePhysicalCrouch(crouchState, 1.40f, true, 1, 0.35f, 0.25f)
            == crouch_math::PhysicalCrouchUpdate::None,
        "physical crouch hysteresis holds state");
    failures += Check(
        crouch_math::UpdatePhysicalCrouch(crouchState, 1.46f, true, 1, 0.35f, 0.25f)
            == crouch_math::PhysicalCrouchUpdate::Exit,
        "physical crouch exits above release threshold");

    hud_math::HudQuadPose hudPose;
    failures += Check(
        hud_math::BuildHeadLockedQuadPose(
            {1.0f, 2.0f, 3.0f},
            {},
            1.5f,
            0.1f,
            1.6f,
            16.0f / 9.0f,
            hudPose),
        "head-locked HUD pose construction");
    failures += Check(
        Near(hudPose.position.x, 1.0f)
            && Near(hudPose.position.y, 2.1f)
            && Near(hudPose.position.z, 1.5f),
        "head-locked HUD local offset");
    failures += Check(
        Near(hudPose.widthMeters, 1.6f) && Near(hudPose.heightMeters, 0.9f),
        "head-locked HUD aspect ratio");
    failures += Check(
        !hud_math::BuildHeadLockedQuadPose({}, {}, 0.0f, 0.0f, 1.0f, 1.0f, hudPose),
        "head-locked HUD rejects invalid distance");
    float reticleSizeMeters = 0.0f;
    failures += Check(
        hud_math::ComputeAngularQuadSize(2.0f, 1.0f, 0.005f, 0.1f, reticleSizeMeters)
            && Near(reticleSizeMeters, 0.034907f, 0.00001f),
        "interaction reticle angular size");
    failures += Check(
        hud_math::ComputeAngularQuadSize(0.1f, 0.1f, 0.008f, 0.08f, reticleSizeMeters)
            && Near(reticleSizeMeters, 0.008f),
        "interaction reticle minimum size clamp");
    failures += Check(
        hud_math::ComputeAngularQuadSize(100.0f, 5.0f, 0.008f, 0.08f, reticleSizeMeters)
            && Near(reticleSizeMeters, 0.08f),
        "interaction reticle maximum size clamp");

    std::array<float, 16> handMatrix{};
    const hands_math::HandRootCalibration handCalibration{
        {0.1f, -0.075f, 0.2f},
        {},
    };
    failures += Check(
        hands_math::BuildControllerHandMatrix(
            {1.0f, 2.0f, 3.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f},
            0.25f,
            handCalibration,
            handMatrix),
        "controller hand root builds from tracked basis");
    failures += Check(
        Near(handMatrix[0], -0.25f)
            && Near(handMatrix[5], 0.25f)
            && Near(handMatrix[10], -0.25f)
            && Near(handMatrix[15], 1.0f),
        "controller hand root preserves SOMA rotateY(pi) basis and scale");
    failures += Check(
        Near(handMatrix[3], 1.1f)
            && Near(handMatrix[7], 1.925f)
            && Near(handMatrix[11], 2.8f),
        "controller hand root applies controller-local position calibration");
    failures += Check(
        !hands_math::BuildControllerHandMatrix(
            {},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            0.25f,
            {},
            handMatrix),
        "controller hand root rejects collinear tracking basis");

    std::array<float, 16> flashlightMatrix{};
    const flashlight_math::FlashlightCalibration flashlightCalibration{
        {0.1f, -0.05f, 0.2f},
        {},
    };
    failures += Check(
        flashlight_math::BuildControllerFlashlightMatrix(
            {1.0f, 2.0f, 3.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f},
            flashlightCalibration,
            flashlightMatrix),
        "controller flashlight builds from tracked aim basis");
    failures += Check(
        Near(flashlightMatrix[0], 1.0f)
            && Near(flashlightMatrix[5], 1.0f)
            && Near(flashlightMatrix[10], 1.0f)
            && Near(flashlightMatrix[15], 1.0f),
        "controller flashlight maps OpenXR forward to HPL local negative Z");
    failures += Check(
        Near(flashlightMatrix[3], 1.1f)
            && Near(flashlightMatrix[7], 1.95f)
            && Near(flashlightMatrix[11], 2.8f),
        "controller flashlight applies aim-local position calibration");
    failures += Check(
        !flashlight_math::BuildControllerFlashlightMatrix(
            {},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {},
            flashlightMatrix),
        "controller flashlight rejects collinear tracking basis");

    using grab_math::ResolveAngularTargetVelocity;
    const camera_math::Vector3 noGrabRotation = ResolveAngularTargetVelocity({}, {}, 100.0f, 1.0f, 6.0f);
    failures += Check(
        Near(noGrabRotation.x, 0.0f) && Near(noGrabRotation.y, 0.0f) && Near(noGrabRotation.z, 0.0f),
        "grab rotation identity delta");
    const camera_math::Vector3 yawGrabRotation = ResolveAngularTargetVelocity(
        {},
        {0.0f, kHalfSqrtTwo, 0.0f, kHalfSqrtTwo},
        100.0f,
        1.0f,
        6.0f);
    failures += Check(
        Near(yawGrabRotation.x, 0.0f) && Near(yawGrabRotation.y, 6.0f) && Near(yawGrabRotation.z, 0.0f),
        "grab rotation follows shortest yaw arc and speed cap");
    const camera_math::Vector3 equivalentGrabRotation = ResolveAngularTargetVelocity(
        {},
        {0.0f, 0.0f, 0.0f, -1.0f},
        100.0f,
        1.0f,
        6.0f);
    failures += Check(
        Near(equivalentGrabRotation.x, 0.0f)
            && Near(equivalentGrabRotation.y, 0.0f)
            && Near(equivalentGrabRotation.z, 0.0f),
        "grab rotation treats negated quaternion as equivalent");

    menu_math::MenuPointerPosition menuPointer;
    failures += Check(
        menu_math::ProjectAimToMenu({}, {}, 70.0f, 50.0f, menuPointer)
            && Near(menuPointer.x, 0.5f)
            && Near(menuPointer.y, 0.5f),
        "head-relative menu aim projects to center");
    failures += Check(
        !menu_math::ProjectAimToMenu(
            {},
            {0.0f, 1.0f, 0.0f, 0.0f},
            70.0f,
            50.0f,
            menuPointer),
        "menu aim rejects controller pointing behind head");
    failures += Check(
        !menu_math::ProjectAimToMenu(
            {},
            {0.0f, 0.0f, 0.0f, 0.0f},
            70.0f,
            50.0f,
            menuPointer),
        "menu aim rejects malformed orientation");

    OpenXREyeView asymmetricEye;
    asymmetricEye.valid = true;
    asymmetricEye.angleLeft = -0.82f;
    asymmetricEye.angleRight = 0.66f;
    asymmetricEye.angleDown = -0.49f;
    asymmetricEye.angleUp = 0.73f;
    const float horizontalSpan = std::tan(asymmetricEye.angleRight) - std::tan(asymmetricEye.angleLeft);
    const float verticalSpan = std::tan(asymmetricEye.angleUp) - std::tan(asymmetricEye.angleDown);

    const OpenXREyeView centeredEye = camera_math::CenterProjectionFov(asymmetricEye);
    failures += Check(Near(centeredEye.angleLeft, -centeredEye.angleRight), "horizontal FOV centering");
    failures += Check(Near(centeredEye.angleDown, -centeredEye.angleUp), "vertical FOV centering");
    failures += Check(
        Near(std::tan(centeredEye.angleRight) - std::tan(centeredEye.angleLeft), horizontalSpan),
        "horizontal tangent span preservation");
    failures += Check(
        Near(std::tan(centeredEye.angleUp) - std::tan(centeredEye.angleDown), verticalSpan),
        "vertical tangent span preservation");

    std::array<float, 16> projection{};
    float verticalFov = 0.0f;
    float aspect = 0.0f;
    failures += Check(
        camera_math::BuildOpenXRProjection(centeredEye, 0.03f, 1000.0f, projection, verticalFov, aspect),
        "centered projection construction");
    failures += Check(Near(projection[2], 0.0f), "horizontal projection offset");
    failures += Check(Near(projection[6], 0.0f), "vertical projection offset");

    const camera_math::Quaternion identity;
    const camera_math::Vector3 inputVector{1.0f, 2.0f, 3.0f};
    const camera_math::Vector3 identityRotated = camera_math::RotateVector(identity, inputVector);
    failures += Check(
        Near(identityRotated.x, inputVector.x)
            && Near(identityRotated.y, inputVector.y)
            && Near(identityRotated.z, inputVector.z),
        "identity quaternion rotation");
    const camera_math::Vector3 yawRotated = camera_math::RotateVector(
        {0.0f, kHalfSqrtTwo, 0.0f, kHalfSqrtTwo},
        {0.0f, 0.0f, 1.0f});
    failures += Check(
        Near(yawRotated.x, 1.0f) && Near(yawRotated.y, 0.0f) && Near(yawRotated.z, 0.0f),
        "quarter-turn yaw rotation");

    const camera_math::Quaternion matrixTestRotation = camera_math::Normalize(
        {0.23f, -0.41f, 0.17f, 0.86f});
    const std::array<float, 16> rotationMatrix = camera_math::RotationMatrix(matrixTestRotation);
    const auto rowDot = [&rotationMatrix](size_t leftRow, size_t rightRow) {
        float value = 0.0f;
        for (size_t column = 0; column < 3; ++column) {
            value += rotationMatrix[leftRow * 4 + column]
                * rotationMatrix[rightRow * 4 + column];
        }
        return value;
    };
    failures += Check(
        Near(rowDot(0, 0), 1.0f) && Near(rowDot(1, 1), 1.0f) && Near(rowDot(2, 2), 1.0f),
        "rotation matrix unit basis");
    failures += Check(
        Near(rowDot(0, 1), 0.0f) && Near(rowDot(0, 2), 0.0f) && Near(rowDot(1, 2), 0.0f),
        "rotation matrix orthogonal basis");

    const camera_math::Vector3 matrixTestVector{0.31f, -0.27f, 0.73f};
    const camera_math::Vector3 quaternionResult = camera_math::RotateVector(
        matrixTestRotation,
        matrixTestVector);
    const camera_math::Vector3 matrixResult{
        rotationMatrix[0] * matrixTestVector.x + rotationMatrix[1] * matrixTestVector.y
            + rotationMatrix[2] * matrixTestVector.z,
        rotationMatrix[4] * matrixTestVector.x + rotationMatrix[5] * matrixTestVector.y
            + rotationMatrix[6] * matrixTestVector.z,
        rotationMatrix[8] * matrixTestVector.x + rotationMatrix[9] * matrixTestVector.y
            + rotationMatrix[10] * matrixTestVector.z,
    };
    failures += Check(
        Near(matrixResult.x, quaternionResult.x)
            && Near(matrixResult.y, quaternionResult.y)
            && Near(matrixResult.z, quaternionResult.z),
        "rotation matrix matches quaternion rotation");

    camera_math::PoseStabilityState poseLatch;
    float positionStep = 0.0f;
    float orientationStep = 0.0f;
    camera_math::PoseStabilityUpdate poseUpdate = camera_math::UpdatePoseStability(
        poseLatch,
        1,
        camera_math::Quaternion{},
        {0.0f, -1.2447f, 0.0f},
        8,
        0.25f,
        0.7853982f,
        positionStep,
        orientationStep);
    failures += Check(
        poseUpdate == camera_math::PoseStabilityUpdate::Started
            && poseLatch.consecutiveFrames == 1,
        "pose stability starts on first unique frame");
    poseUpdate = camera_math::UpdatePoseStability(
        poseLatch,
        1,
        camera_math::Quaternion{},
        {0.0f, -1.2447f, 0.0f},
        8,
        0.25f,
        0.7853982f,
        positionStep,
        orientationStep);
    failures += Check(
        poseUpdate == camera_math::PoseStabilityUpdate::DuplicateFrame
            && poseLatch.consecutiveFrames == 1,
        "pose stability ignores duplicate game-frame samples");

    const camera_math::Quaternion settledOrientation = camera_math::Normalize(
        {0.07f, 0.55f, 0.04f, -0.83f});
    const camera_math::Vector3 settledPosition{-0.45f, 0.55f, -0.33f};
    poseUpdate = camera_math::UpdatePoseStability(
        poseLatch,
        2,
        settledOrientation,
        settledPosition,
        8,
        0.25f,
        0.7853982f,
        positionStep,
        orientationStep);
    failures += Check(
        poseUpdate == camera_math::PoseStabilityUpdate::Reset
            && poseLatch.consecutiveFrames == 1
            && positionStep > 1.0f,
        "pose stability resets after reference-space jump");

    for (uint64_t frame = 3; frame <= 9; ++frame) {
        poseUpdate = camera_math::UpdatePoseStability(
            poseLatch,
            frame,
            settledOrientation,
            settledPosition,
            8,
            0.25f,
            0.7853982f,
            positionStep,
            orientationStep);
    }
    failures += Check(
        poseUpdate == camera_math::PoseStabilityUpdate::Ready
            && poseLatch.consecutiveFrames == 8,
        "pose stability requires consecutive settled frames");

    const camera_math::Quaternion equivalentOrientation{
        -settledOrientation.x,
        -settledOrientation.y,
        -settledOrientation.z,
        -settledOrientation.w,
    };
    poseUpdate = camera_math::UpdatePoseStability(
        poseLatch,
        10,
        equivalentOrientation,
        settledPosition,
        8,
        0.25f,
        0.7853982f,
        positionStep,
        orientationStep);
    failures += Check(
        poseUpdate == camera_math::PoseStabilityUpdate::Ready
            && Near(orientationStep, 0.0f),
        "pose stability accepts equivalent quaternion sign");

    const std::array<float, 16> identityMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    const std::array<float, 16> translation = camera_math::TranslationMatrix({4.0f, 5.0f, 6.0f});
    failures += Check(
        camera_math::MatrixMultiply(identityMatrix, translation) == translation,
        "matrix identity multiplication");

    const camera_math::Vector3 roomscaleEye{1.0f, 2.0f, 3.0f};
    const camera_math::Vector3 roomscaleCenter{0.9f, 1.5f, 2.8f};
    const camera_math::Vector3 roomscaleNeutral{0.5f, 1.0f, 2.0f};
    const camera_math::Vector3 fullRoomscale = camera_math::ResolveTrackedEyeOffset(
        roomscaleEye,
        roomscaleCenter,
        roomscaleNeutral,
        camera_math::Quaternion{},
        true,
        true,
        2.0f,
        0.1f);
    failures += Check(
        Near(fullRoomscale.x, 1.0f)
            && Near(fullRoomscale.y, 2.2f)
            && Near(fullRoomscale.z, 2.0f),
        "roomscale and eye-height calibration");
    const camera_math::Vector3 horizontalRoomscale = camera_math::ResolveTrackedEyeOffset(
        roomscaleEye,
        roomscaleCenter,
        roomscaleNeutral,
        camera_math::Quaternion{},
        true,
        false,
        2.0f,
        0.1f);
    failures += Check(
        Near(horizontalRoomscale.x, 1.0f)
            && Near(horizontalRoomscale.y, 1.2f)
            && Near(horizontalRoomscale.z, 2.0f),
        "vertical roomscale suppression preserves stereo height");

    const GLfloat perspective[16] = {
        0.803333f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.428148f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.000060f, -0.060002f,
        0.0f, 0.0f, -1.0f, 0.0f,
    };
    const gl_matrix::MatrixSummary summary = gl_matrix::SummarizeMatrix(perspective);
    failures += Check(summary.valid && summary.projectionLike, "OpenGL perspective classification");
    failures += Check(Near(summary.aspect, 16.0f / 9.0f, 0.0001f), "OpenGL matrix aspect");
    failures += Check(!gl_matrix::SummarizeMatrix(identityMatrix.data()).projectionLike, "identity is not projection");
    failures += Check(
        gl_matrix::MatrixValuesText(perspective).find(',') != std::string::npos,
        "matrix value formatting");

    hud_math::InteractionReticleColor reticleColor;
    failures += Check(
        !hud_math::ComputeInteractionReticleColor(0, reticleColor)
            && !hud_math::ComputeInteractionReticleColor(35, reticleColor),
        "semantic reticle rejects none and sentinel states");
    failures += Check(
        hud_math::ComputeInteractionReticleColor(14, reticleColor)
            && Near(reticleColor.red, 0.25f)
            && Near(reticleColor.green, 1.0f)
            && Near(reticleColor.blue, 0.55f),
        "semantic reticle classifies pickup state");
    failures += Check(
        hud_math::ComputeInteractionReticleColor(4, reticleColor)
            && Near(reticleColor.red, 1.0f)
            && Near(reticleColor.green, 0.72f)
            && Near(reticleColor.blue, 0.20f),
        "semantic reticle classifies manipulation state");
    failures += Check(
        hud_math::ComputeInteractionReticleColor(30, reticleColor)
            && Near(reticleColor.red, 1.0f)
            && Near(reticleColor.green, 0.30f)
            && Near(reticleColor.blue, 0.25f),
        "semantic reticle classifies unavailable state");
    float hapticAmplitudeScale = 0.0f;
    float hapticDurationScale = 0.0f;
    failures += Check(
        !hud_math::ComputeInteractionHapticProfile(1, hapticAmplitudeScale, hapticDurationScale),
        "semantic focus haptics ignore default cursor");
    failures += Check(
        hud_math::ComputeInteractionHapticProfile(14, hapticAmplitudeScale, hapticDurationScale)
            && Near(hapticAmplitudeScale, 0.75f)
            && Near(hapticDurationScale, 0.80f),
        "semantic focus haptics classify pickup state");
    failures += Check(
        hud_math::ComputeInteractionHapticProfile(4, hapticAmplitudeScale, hapticDurationScale)
            && Near(hapticAmplitudeScale, 1.15f)
            && Near(hapticDurationScale, 1.25f),
        "semantic focus haptics classify manipulation state");

    if (failures == 0) {
        std::cout << "Render math tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

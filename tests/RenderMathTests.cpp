#include "HPLCameraMath.h"
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
    constexpr float kHalfSqrtTwo = 0.7071067811865475f;
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

    if (failures == 0) {
        std::cout << "Render math tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

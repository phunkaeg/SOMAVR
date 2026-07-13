#pragma once

#include "OpenXRRuntime.h"

#include <array>

namespace somavr::camera_math {

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Quaternion Normalize(Quaternion value);
Quaternion Conjugate(const Quaternion& value);
Quaternion Multiply(const Quaternion& left, const Quaternion& right);
Vector3 RotateVector(const Quaternion& input, const Vector3& value);

std::array<float, 16> RotationMatrix(const Quaternion& input);
std::array<float, 16> MatrixMultiply(
    const std::array<float, 16>& left,
    const std::array<float, 16>& right);
std::array<float, 16> TranslationMatrix(const Vector3& translation);

OpenXREyeView CenterProjectionFov(const OpenXREyeView& eye);
bool BuildOpenXRProjection(
    const OpenXREyeView& eye,
    float nearPlane,
    float farPlane,
    std::array<float, 16>& projection,
    float& verticalFov,
    float& aspect);
bool ValidateStereoProjectionMath();

} // namespace somavr::camera_math

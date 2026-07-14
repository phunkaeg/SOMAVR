#include "HPLInputMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::input_math
{

Axis2 ApplyRadialDeadzone(float x, float y, float deadzone)
{
    const float clampedDeadzone = std::clamp(deadzone, 0.0f, 0.999f);
    const float length = std::sqrt(x * x + y * y);
    if (!std::isfinite(length) || length <= clampedDeadzone || length <= 0.000001f)
        return {};
    const float normalizedLength = std::min((length - clampedDeadzone) / (1.0f - clampedDeadzone), 1.0f);
    const float scale = normalizedLength / length;
    return {x * scale, y * scale};
}

float DegreesToRadians(float degrees)
{
    constexpr float kDegreesToRadians = 0.01745329251994329577f;
    return degrees * kDegreesToRadians;
}

} // namespace somavr::input_math

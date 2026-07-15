#include "OpenXRComfortVignetteMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::comfort_vignette_math {
namespace {

float NormalizeAfterDeadzone(float value, float deadzone)
{
    if (!std::isfinite(value) || !std::isfinite(deadzone)) return 0.0f;
    const float magnitude = std::clamp(std::abs(value), 0.0f, 1.0f);
    const float threshold = std::clamp(deadzone, 0.0f, 0.95f);
    return std::clamp((magnitude - threshold) / (1.0f - threshold), 0.0f, 1.0f);
}

} // namespace

float ComputeMotionIntensity(
    float moveX,
    float moveY,
    float turnX,
    bool includeSmoothTurn,
    float moveDeadzone,
    float turnDeadzone)
{
    if (!std::isfinite(moveX) || !std::isfinite(moveY)) return 0.0f;
    const float moveMagnitude = std::sqrt(moveX * moveX + moveY * moveY);
    const float movement = NormalizeAfterDeadzone(moveMagnitude, moveDeadzone);
    const float turning = includeSmoothTurn
        ? NormalizeAfterDeadzone(turnX, turnDeadzone)
        : 0.0f;
    return std::max(movement, turning);
}

float AdvanceEnvelope(
    float currentLevel,
    float targetLevel,
    float deltaSeconds,
    int fadeMilliseconds)
{
    if (!std::isfinite(currentLevel) || !std::isfinite(targetLevel)) return 0.0f;
    const float current = std::clamp(currentLevel, 0.0f, 1.0f);
    const float target = std::clamp(targetLevel, 0.0f, 1.0f);
    if (fadeMilliseconds <= 0) return target;
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) return current;
    const float step = std::clamp(
        deltaSeconds / (static_cast<float>(fadeMilliseconds) / 1000.0f),
        0.0f,
        1.0f);
    if (target > current) return std::min(target, current + step);
    return std::max(target, current - step);
}

bool Rasterize(
    int sizePixels,
    float level,
    float strength,
    float innerRadius,
    std::vector<uint8_t>& rgbaPixels)
{
    if (sizePixels < 32 || sizePixels > 1024
        || !std::isfinite(level) || !std::isfinite(strength)
        || !std::isfinite(innerRadius)) {
        return false;
    }

    const float opacity = std::clamp(level, 0.0f, 1.0f)
        * std::clamp(strength, 0.0f, 1.0f);
    const float inner = std::clamp(innerRadius, 0.0f, 0.98f);
    rgbaPixels.assign(static_cast<size_t>(sizePixels) * sizePixels * 4, 0);
    const float scale = 2.0f / static_cast<float>(sizePixels);
    for (int y = 0; y < sizePixels; ++y) {
        const float normalizedY = (static_cast<float>(y) + 0.5f) * scale - 1.0f;
        for (int x = 0; x < sizePixels; ++x) {
            const float normalizedX = (static_cast<float>(x) + 0.5f) * scale - 1.0f;
            const float radius = std::sqrt(
                normalizedX * normalizedX + normalizedY * normalizedY);
            const float transition = std::clamp((radius - inner) / (1.0f - inner), 0.0f, 1.0f);
            const float smooth = transition * transition * (3.0f - 2.0f * transition);
            const uint8_t alpha = static_cast<uint8_t>(std::lround(opacity * smooth * 255.0f));
            const size_t pixel = (static_cast<size_t>(y) * sizePixels + x) * 4;
            rgbaPixels[pixel + 3] = alpha;
        }
    }
    return true;
}

} // namespace somavr::comfort_vignette_math

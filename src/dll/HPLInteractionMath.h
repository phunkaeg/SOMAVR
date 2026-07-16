#pragma once

#include <cstdint>

namespace somavr::interaction_math {

struct HandCandidate {
    bool tracked = false;
    bool hit = false;
    bool pressed = false;
    float distance = 0.0f;
};

uint32_t SelectInteractionHand(
    const HandCandidate& left,
    const HandCandidate& right,
    uint32_t preferredHand,
    int previousHand);

} // namespace somavr::interaction_math

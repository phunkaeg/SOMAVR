#include "HPLInteractionMath.h"

#include <cmath>

namespace somavr::interaction_math {

uint32_t SelectInteractionHand(
    const HandCandidate& left,
    const HandCandidate& right,
    uint32_t preferredHand,
    int previousHand)
{
    const HandCandidate hands[2] = {left, right};
    preferredHand = preferredHand < 2 ? preferredHand : 1u;
    const bool previousValid = previousHand >= 0 && previousHand < 2;

    if (left.pressed != right.pressed) {
        return left.pressed ? 0u : 1u;
    }
    if (left.pressed && right.pressed) {
        if (previousValid && hands[previousHand].tracked) {
            return static_cast<uint32_t>(previousHand);
        }
        return hands[preferredHand].tracked ? preferredHand : (preferredHand ^ 1u);
    }

    if (left.hit != right.hit) {
        return left.hit ? 0u : 1u;
    }
    if (left.hit && right.hit) {
        if (previousValid && hands[previousHand].hit) {
            return static_cast<uint32_t>(previousHand);
        }
        if (hands[preferredHand].hit) return preferredHand;
        if (std::isfinite(left.distance) && std::isfinite(right.distance)) {
            return left.distance <= right.distance ? 0u : 1u;
        }
    }

    if (previousValid && hands[previousHand].tracked) {
        return static_cast<uint32_t>(previousHand);
    }
    return hands[preferredHand].tracked ? preferredHand : (preferredHand ^ 1u);
}

} // namespace somavr::interaction_math

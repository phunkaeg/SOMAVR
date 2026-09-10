#pragma once

#include <cstdint>

namespace somavr::hands_bootstrap {

struct Identity {
    uintptr_t map = 0;
    uintptr_t script = 0;
    uint64_t loadingGeneration = 0;
    bool operator==(const Identity&) const = default;
};

enum class Action { Wait, Invoke, Confirmed, TimedOut };

// One attempt per world/script lifetime; toggles and pauses cannot re-arm it.
class Policy {
public:
    Action Observe(Identity identity, bool eligible, bool hasSeed,
        uint64_t frame, uint64_t nowMs)
    {
        if (identity.map == 0 || identity.script == 0) {
            ResetSettling();
            return Action::Wait;
        }
        if (!(identity == identity_)) {
            *this = {};
            identity_ = identity;
        }
        if (eligible && hasSeed && !confirmed_) {
            confirmed_ = true;
            return Action::Confirmed;
        }
        if (confirmed_ || timedOut_) return Action::Wait;
        if (attempted_) {
            if (nowMs >= attemptMs_ && nowMs - attemptMs_ >= 5000) {
                timedOut_ = true;
                return Action::TimedOut;
            }
            return Action::Wait;
        }
        if (!eligible) {
            ResetSettling();
            return Action::Wait;
        }
        if (settledFrames_ == 0 || frame < lastFrame_) {
            settledFrames_ = 1;
            startMs_ = nowMs;
        } else if (frame != lastFrame_) {
            ++settledFrames_;
        }
        lastFrame_ = frame;
        if (settledFrames_ < 30 || nowMs < startMs_ || nowMs - startMs_ < 750)
            return Action::Wait;
        attempted_ = true;
        attemptMs_ = nowMs;
        return Action::Invoke;
    }

private:
    void ResetSettling() { settledFrames_ = 0; }
    Identity identity_{};
    uint64_t lastFrame_ = 0;
    uint64_t startMs_ = 0;
    uint64_t attemptMs_ = 0;
    uint32_t settledFrames_ = 0;
    bool attempted_ = false;
    bool confirmed_ = false;
    bool timedOut_ = false;
};

} // namespace somavr::hands_bootstrap

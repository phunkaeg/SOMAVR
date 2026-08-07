#pragma once

#include <atomic>
#include <cstdint>

namespace somavr::crash_capture {

enum class Decision {
    Capture,
    Busy,
    Duplicate,
    LimitReached,
};

class Policy {
public:
    explicit Policy(uint32_t maximumAttempts = 3)
        : maximumAttempts_(maximumAttempts)
    {
    }

    Decision Begin(uint32_t exceptionCode, uintptr_t exceptionAddress)
    {
        if (active_.test_and_set(std::memory_order_acquire)) {
            return Decision::Busy;
        }

        const uint32_t attempts = attempts_.load(std::memory_order_relaxed);
        if (attempts >= maximumAttempts_) {
            active_.clear(std::memory_order_release);
            return Decision::LimitReached;
        }
        if (attempts > 0
            && lastCode_.load(std::memory_order_relaxed) == exceptionCode
            && lastAddress_.load(std::memory_order_relaxed) == exceptionAddress) {
            active_.clear(std::memory_order_release);
            return Decision::Duplicate;
        }

        lastCode_.store(exceptionCode, std::memory_order_relaxed);
        lastAddress_.store(exceptionAddress, std::memory_order_relaxed);
        attempts_.store(attempts + 1, std::memory_order_relaxed);
        return Decision::Capture;
    }

    void End()
    {
        active_.clear(std::memory_order_release);
    }

    uint32_t Attempts() const
    {
        return attempts_.load(std::memory_order_relaxed);
    }

private:
    const uint32_t maximumAttempts_;
    std::atomic_flag active_ = ATOMIC_FLAG_INIT;
    std::atomic<uint32_t> attempts_{0};
    std::atomic<uint32_t> lastCode_{0};
    std::atomic<uintptr_t> lastAddress_{0};
};

} // namespace somavr::crash_capture

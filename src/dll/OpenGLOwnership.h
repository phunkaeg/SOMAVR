#pragma once

#include <atomic>
#include <cstdint>

namespace somavr {

inline thread_local uint32_t g_ownOpenGLDepth = 0;
inline std::atomic<uint64_t> g_ownOpenGLBypasses = 0;

inline bool IsOwnOpenGLWork()
{
    const bool active = g_ownOpenGLDepth != 0;
    if (active) {
        g_ownOpenGLBypasses.fetch_add(1, std::memory_order_relaxed);
    }
    return active;
}

inline uint64_t OwnOpenGLBypassCount()
{
    return g_ownOpenGLBypasses.load(std::memory_order_relaxed);
}

class ScopedOwnOpenGLWork final {
public:
    ScopedOwnOpenGLWork() { ++g_ownOpenGLDepth; }
    ~ScopedOwnOpenGLWork() { --g_ownOpenGLDepth; }

    ScopedOwnOpenGLWork(const ScopedOwnOpenGLWork&) = delete;
    ScopedOwnOpenGLWork& operator=(const ScopedOwnOpenGLWork&) = delete;
};

} // namespace somavr

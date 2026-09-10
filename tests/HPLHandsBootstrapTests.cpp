#include "HPLHandsBootstrapPolicy.h"

#include <iostream>

using namespace somavr::hands_bootstrap;

int main()
{
    int failures = 0;
    auto check = [&](bool condition, const char* message) {
        if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
    };
    Identity world{1, 2, 3};
    Policy p;
    for (uint64_t f = 0; f < 600; ++f)
        check(p.Observe(world, false, false, f, f * 40) == Action::Wait,
            "splash/loading duration never triggers creation");
    for (uint64_t f = 0; f < 29; ++f)
        check(p.Observe(world, true, false, f, 25000 + f * 40) == Action::Wait,
            "settle before invocation");
    check(p.Observe(world, true, false, 29, 26200) == Action::Invoke, "one invocation");
    check(p.Observe(world, true, true, 30, 26240) == Action::Confirmed, "native seed confirms");
    for (uint64_t f = 31; f < 500; ++f)
        check(p.Observe(world, f % 2 == 0, false, f, f * 1000) == Action::Wait,
            "no respawn after pause, toggle, or transient missing seed");
    ++world.loadingGeneration;
    for (uint64_t f = 0; f < 29; ++f) p.Observe(world, true, false, f, f * 40);
    check(p.Observe(world, true, false, 29, 1200) == Action::Invoke,
        "loading epoch re-arms even if heap pointers reused");
    check(p.Observe(world, true, false, 30, 6200) == Action::TimedOut, "bounded confirmation wait");
    check(p.Observe(world, true, false, 900, 999999) == Action::Wait, "no retry storm after failure");
    check(p.Observe(world, true, true, 901, 1000000) == Action::Confirmed,
        "later native vial creation can still confirm");
    Policy duplicate;
    for (uint64_t n = 0; n < 100; ++n)
        check(duplicate.Observe(world, true, false, 1, n * 100) == Action::Wait,
            "repeated render eye/frame does not settle");
    Policy existing;
    check(existing.Observe(world, false, true, 0, 0) == Action::Wait,
        "loading cannot confirm a stale rig from the previous world");
    check(existing.Observe(world, true, true, 1, 0) == Action::Confirmed, "existing rig untouched");
    Policy interrupted;
    for (uint64_t f = 0; f < 29; ++f) interrupted.Observe(world, true, false, f, f * 40);
    interrupted.Observe(world, false, false, 29, 1200);
    check(interrupted.Observe(world, true, false, 30, 9000) == Action::Wait,
        "authored interaction interrupts settling");
    check(interrupted.Observe({}, true, false, 31, 10000) == Action::Wait, "null world fails closed");
    if (!failures) std::cout << "Hands bootstrap policy checks passed\n";
    return failures ? 1 : 0;
}

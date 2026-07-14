#include "CompatibilityScan.h"

#include <iostream>

namespace {

int Check(bool condition, const char* message)
{
    if (condition) return 0;
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    using somavr::injector::ClassifyCompatibilityName;
    using somavr::injector::CompatibilityFinding;
    using somavr::injector::CompatibilitySeverity;

    int failures = 0;
    CompatibilityFinding finding;
    failures += Check(
        ClassifyCompatibilityName(L"somavr.dll", true, finding)
            && finding.severity == CompatibilitySeverity::Blocking,
        "loaded SOMAVR blocks duplicate injection");
    failures += Check(
        !ClassifyCompatibilityName(L"somavr.dll", false, finding),
        "packaged SOMAVR DLL is not a directory conflict");
    failures += Check(
        ClassifyCompatibilityName(L"opengl32.dll", false, finding)
            && finding.severity == CompatibilitySeverity::Warning,
        "local OpenGL proxy is reported");
    failures += Check(
        ClassifyCompatibilityName(L"ReShade64.dll", true, finding),
        "loaded ReShade component is reported");
    failures += Check(
        ClassifyCompatibilityName(L"OpenXR_Toolkit.dll", true, finding),
        "OpenXR Toolkit layer is reported");
    failures += Check(
        !ClassifyCompatibilityName(L"gameoverlayrenderer64.dll", true, finding),
        "Steam overlay is not classified as a known conflict");
    failures += Check(
        !ClassifyCompatibilityName(L"SDL2.dll", false, finding),
        "normal SOMA dependency is not classified");

    if (failures == 0) {
        std::cout << "Injector compatibility tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

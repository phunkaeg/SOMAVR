#include "CompatibilityScan.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

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
    using somavr::injector::MatchSomaInteractionSignature;

    int failures = 0;
    CompatibilityFinding finding;
    const uint8_t outerSignature[] = {
        0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x50,
        0x48, 0x8b, 0xbc, 0x24, 0x88, 0x00, 0x00, 0x00,
    };
    const uint8_t innerSignature[] = {
        0x40, 0x57, 0x48, 0x83, 0xec, 0x60, 0x48, 0x8b, 0x05, 0x13,
        0xed, 0x64, 0x00, 0x48, 0x8b, 0xf9, 0x4d, 0x8b, 0xd0,
    };
    failures += Check(
        MatchSomaInteractionSignature(0x0cd750, outerSignature, sizeof(outerSignature)),
        "supported outer interaction signature matches");
    failures += Check(
        MatchSomaInteractionSignature(0x1438c0, innerSignature, sizeof(innerSignature)),
        "supported inner interaction signature includes the leading REX prefix");
    uint8_t regressedInner[sizeof(innerSignature)] = {};
    std::copy(std::begin(innerSignature), std::end(innerSignature), std::begin(regressedInner));
    regressedInner[0] = 0x57;
    failures += Check(
        !MatchSomaInteractionSignature(0x1438c0, regressedInner, sizeof(regressedInner)),
        "doctor signature matcher rejects the 0.64 missing-prefix regression");
    const uint8_t setUsePost[] = {
        0x88, 0x91, 0xc5, 0x00, 0x00, 0x00, 0xc3,
    };
    const uint8_t setPost[] = {
        0x48, 0x81, 0xc1, 0x08, 0x01, 0x00, 0x00,
        0x41, 0xb8, 0x40, 0x00, 0x00, 0x00, 0xe9,
    };
    const uint8_t applyPost[] = {
        0x48, 0x8b, 0xc4, 0x48, 0x89, 0x70, 0x10, 0x57,
        0x48, 0x81, 0xec, 0xa0, 0x00, 0x00, 0x00,
        0x80, 0xb9, 0xc5, 0x00, 0x00, 0x00, 0x00,
    };
    failures += Check(
        MatchSomaInteractionSignature(0x4a9490, setUsePost, sizeof(setUsePost))
            && MatchSomaInteractionSignature(0x4a94a0, setPost, sizeof(setPost))
            && MatchSomaInteractionSignature(0x240290, applyPost, sizeof(applyPost)),
        "visible-hands post-transform signatures are accepted");
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
    const std::filesystem::path scanDirectory =
        std::filesystem::temp_directory_path() / "somavr-compatibility-scan-test";
    std::error_code ec;
    std::filesystem::create_directories(scanDirectory, ec);
    std::ofstream(scanDirectory / "opengl32.dll").put('\0');
    std::ofstream(scanDirectory / "SDL2.dll").put('\0');
    const auto directoryFindings =
        somavr::injector::ScanCompatibilityDirectory(scanDirectory);
    failures += Check(directoryFindings.size() == 1
            && directoryFindings[0].path.filename() == L"opengl32.dll",
        "directory-only readiness scan reports proxies without a process");
    std::filesystem::remove_all(scanDirectory, ec);

    if (failures == 0) {
        std::cout << "Injector compatibility tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

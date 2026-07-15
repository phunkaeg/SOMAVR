#include "HPLDualRenderDiagnostics.h"

#include "HPLTemporalMutationMath.h"
#include "Logger.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace somavr {
namespace {

using temporal_mutation_math::MutationSummary;

constexpr size_t kRendererCaptureSize = 0x800;
constexpr size_t kCurrentStateCaptureSize = 0x200;
constexpr size_t kHistoryStateCaptureSize = 0x100;
constexpr size_t kSettingsCaptureSize = 0x180;
constexpr size_t kRendererCurrentStateOffset = 0x20;
constexpr size_t kRendererHistoryStateOffset = 0x438;

struct RegionCapture {
    const char* name = "unknown";
    const void* address = nullptr;
    size_t size = 0;
    bool readable = false;
    std::vector<uint8_t> before;
    MutationSummary mutation;
};

struct ActiveCapture {
    bool active = false;
    uint64_t frame = 0;
    uint64_t attempt = 0;
    HPLDualRenderPass pass = HPLDualRenderPass::None;
    std::array<RegionCapture, 4> regions{};
};

struct PairRecord {
    bool valid = false;
    uint64_t frame = 0;
    uint64_t attempt = 0;
    std::array<MutationSummary, 4> mutations{};
    std::array<bool, 4> readable{};
};

thread_local ActiveCapture g_activeCapture;
thread_local PairRecord g_firstEyeRecord;
std::atomic<uint64_t> g_captures = 0;
std::atomic<uint64_t> g_failedRegions = 0;
std::atomic<uint64_t> g_correlatedPairs = 0;
std::atomic<uint64_t> g_equivalentPairs = 0;

bool IsReadableProtection(DWORD protection)
{
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD base = protection & 0xff;
    return base == PAGE_READONLY
        || base == PAGE_READWRITE
        || base == PAGE_WRITECOPY
        || base == PAGE_EXECUTE_READ
        || base == PAGE_EXECUTE_READWRITE
        || base == PAGE_EXECUTE_WRITECOPY;
}

bool CopyReadable(const void* source, size_t size, std::vector<uint8_t>& destination)
{
    destination.clear();
    if (source == nullptr || size == 0) {
        return false;
    }

    const auto* cursor = static_cast<const uint8_t*>(source);
    size_t copied = 0;
    destination.resize(size);
    while (copied < size) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(cursor + copied, &info, sizeof(info)) == 0
            || info.State != MEM_COMMIT
            || !IsReadableProtection(info.Protect)) {
            destination.clear();
            return false;
        }
        const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
        const uintptr_t current = reinterpret_cast<uintptr_t>(cursor + copied);
        const size_t available = regionEnd > current ? static_cast<size_t>(regionEnd - current) : 0;
        if (available == 0) {
            destination.clear();
            return false;
        }
        const size_t chunk = (std::min)(available, size - copied);
        std::memcpy(destination.data() + copied, cursor + copied, chunk);
        copied += chunk;
    }
    return true;
}

bool ReadPointer(const void* object, size_t offset, void*& value)
{
    std::vector<uint8_t> bytes;
    if (!CopyReadable(static_cast<const uint8_t*>(object) + offset, sizeof(value), bytes)) {
        value = nullptr;
        return false;
    }
    std::memcpy(&value, bytes.data(), sizeof(value));
    return true;
}

const char* PassName(HPLDualRenderPass pass)
{
    switch (pass) {
    case HPLDualRenderPass::FirstEye: return "first_eye";
    case HPLDualRenderPass::ReplayEye: return "replay_eye";
    default: return "none";
    }
}

std::string FormatSpans(const MutationSummary& mutation)
{
    std::ostringstream text;
    if (mutation.spanCount == 0) {
        return "none";
    }
    for (size_t index = 0; index < mutation.spanCount; ++index) {
        if (index != 0) {
            text << ',';
        }
        text << "0x" << std::hex << mutation.spans[index].offset
             << "+0x" << mutation.spans[index].length;
    }
    if (mutation.spansTruncated) {
        text << ",more";
    }
    return text.str();
}

void ConfigureRegion(RegionCapture& region, const char* name, const void* address, size_t size)
{
    region = {};
    region.name = name;
    region.address = address;
    region.size = size;
    region.readable = CopyReadable(address, size, region.before);
    if (!region.readable) {
        g_failedRegions.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace

void BeginHPLDualRenderTemporalCapture(
    uint64_t frame,
    uint64_t attempt,
    HPLDualRenderPass pass,
    void* renderer,
    void* settings)
{
    g_activeCapture = {};
    if (pass == HPLDualRenderPass::None || renderer == nullptr || attempt == 0) {
        return;
    }

    void* currentState = nullptr;
    void* historyState = nullptr;
    ReadPointer(renderer, kRendererCurrentStateOffset, currentState);
    ReadPointer(renderer, kRendererHistoryStateOffset, historyState);

    g_activeCapture.active = true;
    g_activeCapture.frame = frame;
    g_activeCapture.attempt = attempt;
    g_activeCapture.pass = pass;
    ConfigureRegion(g_activeCapture.regions[0], "renderer", renderer, kRendererCaptureSize);
    ConfigureRegion(g_activeCapture.regions[1], "current_state", currentState, kCurrentStateCaptureSize);
    ConfigureRegion(g_activeCapture.regions[2], "history_state", historyState, kHistoryStateCaptureSize);
    ConfigureRegion(g_activeCapture.regions[3], "settings", settings, kSettingsCaptureSize);
}

void EndHPLDualRenderTemporalCapture()
{
    if (!g_activeCapture.active) {
        return;
    }

    bool allEquivalent = true;
    bool anyCorrelated = false;
    for (size_t index = 0; index < g_activeCapture.regions.size(); ++index) {
        RegionCapture& region = g_activeCapture.regions[index];
        std::vector<uint8_t> after;
        if (!region.readable) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_temporal_mutation frame=%llu attempt=%llu pass=%s region=%s address=%p readable=0 phase=before",
                static_cast<unsigned long long>(g_activeCapture.frame),
                static_cast<unsigned long long>(g_activeCapture.attempt),
                PassName(g_activeCapture.pass),
                region.name,
                region.address);
            continue;
        }
        if (!CopyReadable(region.address, region.size, after)) {
            region.readable = false;
            g_failedRegions.fetch_add(1, std::memory_order_relaxed);
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_temporal_mutation frame=%llu attempt=%llu pass=%s region=%s address=%p readable=0 phase=after",
                static_cast<unsigned long long>(g_activeCapture.frame),
                static_cast<unsigned long long>(g_activeCapture.attempt),
                PassName(g_activeCapture.pass),
                region.name,
                region.address);
            continue;
        }

        region.mutation = temporal_mutation_math::SummarizeMutation(
            region.before.data(), after.data(), region.size);
        const std::string spans = FormatSpans(region.mutation);
        Logger::Instance().Write(
            region.mutation.changedBytes != 0 ? LogLevel::Warn : LogLevel::Info,
            "hpl_temporal_mutation frame=%llu attempt=%llu pass=%s region=%s address=%p size=0x%llx changedBytes=%llu beforeHash=0x%llx afterHash=0x%llx spans=%s",
            static_cast<unsigned long long>(g_activeCapture.frame),
            static_cast<unsigned long long>(g_activeCapture.attempt),
            PassName(g_activeCapture.pass),
            region.name,
            region.address,
            static_cast<unsigned long long>(region.size),
            static_cast<unsigned long long>(region.mutation.changedBytes),
            static_cast<unsigned long long>(region.mutation.beforeHash),
            static_cast<unsigned long long>(region.mutation.afterHash),
            spans.c_str());

        if (g_activeCapture.pass == HPLDualRenderPass::ReplayEye
            && g_firstEyeRecord.valid
            && g_firstEyeRecord.attempt == g_activeCapture.attempt
            && g_firstEyeRecord.readable[index]) {
            anyCorrelated = true;
            allEquivalent = allEquivalent
                && temporal_mutation_math::HasEquivalentMutationPattern(
                    g_firstEyeRecord.mutations[index], region.mutation);
        }
    }

    g_captures.fetch_add(1, std::memory_order_relaxed);
    if (g_activeCapture.pass == HPLDualRenderPass::FirstEye) {
        g_firstEyeRecord = {};
        g_firstEyeRecord.valid = true;
        g_firstEyeRecord.frame = g_activeCapture.frame;
        g_firstEyeRecord.attempt = g_activeCapture.attempt;
        for (size_t index = 0; index < g_activeCapture.regions.size(); ++index) {
            g_firstEyeRecord.readable[index] = g_activeCapture.regions[index].readable;
            g_firstEyeRecord.mutations[index] = g_activeCapture.regions[index].mutation;
        }
    } else if (g_activeCapture.pass == HPLDualRenderPass::ReplayEye) {
        if (anyCorrelated) {
            g_correlatedPairs.fetch_add(1, std::memory_order_relaxed);
            if (allEquivalent) {
                g_equivalentPairs.fetch_add(1, std::memory_order_relaxed);
            }
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_temporal_pair frame=%llu attempt=%llu result=%s policy=diagnostic_only_no_state_restore",
                static_cast<unsigned long long>(g_activeCapture.frame),
                static_cast<unsigned long long>(g_activeCapture.attempt),
                allEquivalent ? "equivalent_mutation_ranges" : "eye_specific_mutation_ranges");
        }
        g_firstEyeRecord = {};
    }
    g_activeCapture = {};
}

HPLDualRenderDiagnosticsSummary GetHPLDualRenderDiagnosticsSummary()
{
    return {
        g_captures.load(std::memory_order_relaxed),
        g_failedRegions.load(std::memory_order_relaxed),
        g_correlatedPairs.load(std::memory_order_relaxed),
        g_equivalentPairs.load(std::memory_order_relaxed),
    };
}

void ResetHPLDualRenderDiagnostics()
{
    g_activeCapture = {};
    g_firstEyeRecord = {};
    g_captures.store(0, std::memory_order_relaxed);
    g_failedRegions.store(0, std::memory_order_relaxed);
    g_correlatedPairs.store(0, std::memory_order_relaxed);
    g_equivalentPairs.store(0, std::memory_order_relaxed);
}

} // namespace somavr

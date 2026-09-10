#include "HPLHandsBootstrap.h"
#include "HPLHandsBootstrapPolicy.h"
#include "HPLCameraBridge.h"
#include "HPLHandsBridge.h"
#include "HPLNativeLocomotion.h"
#include "HPLPlayerState.h"
#include "HPLPresentationBridge.h"
#include "Logger.h"
#include "NativeMemoryAccess.h"

#include <Windows.h>
#include <MinHook.h>

#include <array>
#include <atomic>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

// Target receipts and concrete virtual callees: docs/HANDS_BOOTSTRAP_RE.md.
constexpr uintptr_t kPostUpdateRva = 0x1ab3a0;
constexpr uintptr_t kModuleVtableRva = 0x68fcc8;
constexpr uintptr_t kUpdateableVtableRva = 0x68fbb8;
constexpr uintptr_t kScriptObjectVtableRva = 0x6f2f48;
constexpr uintptr_t kScriptContextVtableRva = 0x6c0e38;
constexpr size_t kUpdateableOffset = 0x110;
constexpr uintptr_t kGameContextSlotRva = 0x7925e0;

constexpr uint8_t kPostUpdate[] = {
    0x40,0x53,0x48,0x83,0xec,0x60,0x80,0x79,0x40,0x00,0x0f,0x29,0x74,0x24,0x50,
    0x48,0x8b,0xd9,0x0f,0x28,0xf1,0x0f,0x84,0x14,0x01,0x00,0x00,0x48,0x8b,0x41,0xd8,
    0x48,0x85,0xc0,0x0f,0x84,0x07,0x01,0x00,0x00,0xf6,0x40,0x68,0x20,
    0x0f,0x84,0xfd,0x00,0x00,0x00,0x48,0x8d,0x15,0xdf,0xa9,0x4d,0x00};
constexpr uint8_t kHasMethod[] = {
    0x48,0x83,0xb9,0xe8,0,0,0,0,0x75,3,0x32,0xc0,0xc3,0x48,0x8b,0x89,0xe8,0,0,0,
    0x48,0x8b,1,0x48,0xff,0x60,0x38};
constexpr uint8_t kPrepare[] = {
    0x40,0x55,0x56,0x57,0x48,0x81,0xec,0xb0,0,0,0,0x48,0x83,0xb9,0xe8,0,0,0,0,
    0x41,0x8b,0xe8,0x48,0x8b,0xf2,0x48,0x8b,0xf9,0x75,0x0d,0x32,0xc0};
constexpr uint8_t kSetBool[] = {
    0x48,0x83,0xec,0x28,0x48,0x83,0xb9,0xe8,0,0,0,0,0x74,0x11,0x48,0x83,0x79,0x28,0,
    0x74,0x0a,0x48,0x8b,0x49,0x28,0x48,0x8b,1,0xff,0x50,0x18,0x48,0x83,0xc4,0x28,0xc3};
constexpr uint8_t kExecute[] = {
    0x40,0x53,0x57,0x48,0x83,0xec,0x28,0x48,0x83,0xb9,0xe8,0,0,0,0,0x48,0x8b,0xd9,
    0x0f,0x84,0x36,1,0,0,0x48,0x8b,0x79,0x28,0x48,0x85,0xff,0x0f,0x84,0x29,1,0,0};
constexpr uint8_t kGetCurrentMap[] = {
    0x48,0x8b,5,0x29,0x59,0x6c,0,0x48,0x8b,0x80,0x48,1,0,0,
    0x48,0x8b,0x80,0x90,0,0,0,0xc3};

// Read-only MSVC string argument, not std::string across CRTs. The native method
// cache copies the declaration (0x481140); it never owns or frees this storage.
struct NativeDeclaration {
    const char* data;
    uint64_t padding = 0;
    uint64_t size;
    uint64_t capacity;
};
static_assert(sizeof(NativeDeclaration) == 32);
static_assert(offsetof(NativeDeclaration, size) == 16);
static_assert(offsetof(NativeDeclaration, capacity) == 24);
constexpr NativeDeclaration kVisibleDecl{"void SetVisible(bool)", 0, 21, 21};
constexpr NativeDeclaration kActiveDecl{"void SetActive(bool)", 0, 20, 20};
static_assert(sizeof("void SetVisible(bool)") == kVisibleDecl.size + 1);
static_assert(sizeof("void SetActive(bool)") == kActiveDecl.size + 1);
using PostUpdateFn = void (*)(void*, float);
using MethodFn = bool (*)(void*, const NativeDeclaration*, int);
using SetBoolFn = void (*)(void*, int, bool);
using ExecuteFn = bool (*)(void*);

uintptr_t g_base = 0;
PostUpdateFn g_originalPostUpdate = nullptr;
MethodFn g_hasMethod = nullptr;
MethodFn g_prepare = nullptr;
SetBoolFn g_setBool = nullptr;
ExecuteFn g_execute = nullptr;
std::mutex g_mutex;
hands_bootstrap::Policy g_policy;
std::atomic<uint64_t> g_calls{0}, g_ownerHits{0}, g_readyHits{0}, g_attempts{0}, g_confirmations{0};
std::atomic<bool> g_faulted{false};
thread_local bool g_invoking = false;
void* g_target = nullptr;
uint32_t g_lastGateMask = UINT32_MAX;
uint64_t g_lastGateLogMs = 0;

bool ExactVtable(const void* object, uintptr_t rva)
{
    uintptr_t table = 0;
    return native_memory::TryRead(object, table) && table == g_base + rva;
}

bool ValidateContract(uintptr_t rva, const uint8_t* signature, size_t size, const char* name)
{
    const auto* base = reinterpret_cast<const uint8_t*>(g_base);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64
        || rva > nt->OptionalHeader.SizeOfImage || size > nt->OptionalHeader.SizeOfImage - rva)
        return false;
    size_t matches = 0;
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (unsigned n = 0; n < nt->FileHeader.NumberOfSections; ++n, ++section) {
        const size_t length = section->Misc.VirtualSize;
        if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE) || length < size
            || section->VirtualAddress > nt->OptionalHeader.SizeOfImage
            || length > nt->OptionalHeader.SizeOfImage - section->VirtualAddress) continue;
        for (size_t i = 0; i <= length - size; ++i)
            matches += std::memcmp(base + section->VirtualAddress + i, signature, size) == 0;
    }
    const bool exact = std::memcmp(base + rva, signature, size) == 0;
    Logger::Instance().Write(exact && matches == 1 ? LogLevel::Info : LogLevel::Error,
        "hpl_hands_bootstrap contract=%s rva=0x%llx matches=%zu exact=%d",
        name, static_cast<unsigned long long>(rva), matches, exact ? 1 : 0);
    return exact && matches == 1;
}

bool ResolveWorld(void*& map)
{
    void* context = nullptr;
    void* handler = nullptr;
    return native_memory::TryRead(reinterpret_cast<void*>(g_base + kGameContextSlotRva), context)
        && native_memory::TryReadField(context, 0x148, handler)
        && native_memory::TryReadField(handler, 0x90, map) && map != nullptr;
}

bool CompletedContextReady(void* owner, void* script)
{
    void* manager = nullptr;
    void* context = nullptr;
    void* contextObject = nullptr;
    void* backend = nullptr;
    uint8_t reserved = 1, executing = 1;
    // Only reuse an owner whose own native PostUpdate has just prepared/executed
    // a context. No render-thread script calls and no guessed context allocation.
    return native_memory::TryReadField(owner, 0x10, manager) && manager != nullptr
        && native_memory::TryReadField(owner, 0x28, context)
        && ExactVtable(context, kScriptContextVtableRva)
        && native_memory::TryReadField(context, 0x80, contextObject) && contextObject == script
        && native_memory::TryReadField(context, 0x38, backend) && backend != nullptr
        && native_memory::TryReadField(context, 0x30, reserved) && reserved == 0
        && native_memory::TryReadField(context, 0x48, executing) && executing == 0;
}

bool InvokeBoolMethod(void* owner, const NativeDeclaration& declaration)
{
    // -1 selects the declaration-keyed cache, never a built-in callback slot.
    if (!g_prepare(owner, &declaration, -1)) return false;
    g_setBool(owner, 0, true);
    return g_execute(owner);
}

bool InvokeHands(void* owner)
{
    __try {
        if (!g_hasMethod(owner, &kVisibleDecl, -1) || !g_hasMethod(owner, &kActiveDecl, -1))
            return false;
        if (!InvokeBoolMethod(owner, kVisibleDecl)) return false;
        return InvokeBoolMethod(owner, kActiveDecl);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        // A native fault permanently disables this lane; never retry a partial call.
        g_faulted.store(true, std::memory_order_relaxed);
        return false;
    }
}

void TryBootstrap(void* updateable)
{
    if (g_invoking || g_faulted.load(std::memory_order_relaxed)
        || !ExactVtable(updateable, kUpdateableVtableRva)) return;
    const auto address = reinterpret_cast<uintptr_t>(updateable);
    if (address < kUpdateableOffset) return;
    void* owner = reinterpret_cast<void*>(address - kUpdateableOffset);
    int id = -1;
    void* script = nullptr;
    if (!ExactVtable(owner, kModuleVtableRva)
        || !native_memory::TryReadField(owner, 0x158, id) || id != 18
        || !native_memory::TryReadField(owner, 0xe8, script)
        || !ExactVtable(script, kScriptObjectVtableRva)) return;
    ++g_ownerHits;
    void* map = nullptr;
    ResolveWorld(map);
    const auto camera = GetHPLCameraBridgeStatus();
    HPLPlayerStateSnapshot player{};
    bool paused = true;
    const bool playerValid = GetHPLPlayerStateSnapshot(player) && player.playerValid
        && player.cameraControlValid && player.characterBodyCameraValid;
    const uint64_t poseAge = camera.headPoseFrame >= player.frame
        ? camera.headPoseFrame - player.frame : player.frame - camera.headPoseFrame;
    uint32_t gates = 0;
    if (map == nullptr) gates |= 1;
    if (IsHPLLoadingScreenActive() || IsHPLWakePresentationActive()) gates |= 2;
    if (!playerValid) gates |= 4;
    if (player.playerStateId != 0 || player.moveStateId != 0) gates |= 8;
    if (player.authoredCameraActive || player.semanticAuthoredState
        || player.characterBodyCameraDetached) gates |= 16;
    if (!camera.trackingEnabled || camera.activeCamera != player.camera) gates |= 32;
    if (!camera.headWorldPositionValid || !camera.headSceneOrientationValid || poseAge > 3) gates |= 64;
    if (!GetHPLGamePausedState(paused) || paused) gates |= 128;
    if (gates == 0 && !IsHPLPlayerStateActiveNow(0, player.player, player.characterBody)) gates |= 256;
    if (!CompletedContextReady(owner, script)) gates |= 512;
    const bool ready = gates == 0;
    if (ready) ++g_readyHits;
    const bool hasSeed = HasHPLRetainedHandsSeed();
    std::lock_guard lock(g_mutex);
    const uint64_t now = GetTickCount64();
    if (gates != g_lastGateMask && (g_lastGateMask == UINT32_MAX || now - g_lastGateLogMs >= 1000)) {
        g_lastGateMask = gates;
        g_lastGateLogMs = now;
        Logger::Instance().Write(LogLevel::Info,
            "hpl_hands_bootstrap gate_mask=0x%x owner=%p map=%p playerState=%d moveState=%d poseAge=%llu seed=%d policy=zero_mask_required",
            gates, owner, map, player.playerStateId, player.moveStateId,
            static_cast<unsigned long long>(poseAge), hasSeed ? 1 : 0);
    }
    const auto action = g_policy.Observe({reinterpret_cast<uintptr_t>(map),
        reinterpret_cast<uintptr_t>(script), GetHPLLoadingGeneration()},
        ready, hasSeed, player.frame, now);
    if (action == hands_bootstrap::Action::Invoke) {
        ++g_attempts;
        Logger::Instance().Write(LogLevel::Info,
            "hpl_hands_bootstrap invoke owner=%p updateable=%p script=%p map=%p frame=%llu route=campaign_SetVisible_SetActive",
            owner, updateable, script, map, static_cast<unsigned long long>(player.frame));
        g_invoking = true;
        const bool dispatched = InvokeHands(owner);
        g_invoking = false;
        Logger::Instance().Write(dispatched ? LogLevel::Info : LogLevel::Warn,
            "hpl_hands_bootstrap dispatched=%d nativeFault=%d acceptance=pending_native_seed no_retry=1",
            dispatched ? 1 : 0, g_faulted.load() ? 1 : 0);
    } else if (action == hands_bootstrap::Action::Confirmed) {
        ++g_confirmations;
        Logger::Instance().Write(LogLevel::Info,
            "hpl_hands_bootstrap native_seed_confirmed map=%p frame=%llu attempts=%llu source=existing_hand_bridge headset_acceptance=pending",
            map, static_cast<unsigned long long>(player.frame),
            static_cast<unsigned long long>(g_attempts.load()));
    } else if (action == hands_bootstrap::Action::TimedOut) {
        Logger::Instance().Write(LogLevel::Warn,
            "hpl_hands_bootstrap seed_timeout map=%p action=wait_for_native_vial_creation no_retry=1", map);
    }
}

void HookPostUpdate(void* updateable, float timeStep)
{
    const uint64_t calls = ++g_calls;
    g_originalPostUpdate(updateable, timeStep);
    TryBootstrap(updateable);
    if (calls == 1 || calls % 30000 == 0) {
        Logger::Instance().Write(LogLevel::Info,
            "hpl_hands_bootstrap heartbeat callbacks=%llu owners=%llu ready=%llu attempts=%llu seeds=%llu",
            static_cast<unsigned long long>(calls), static_cast<unsigned long long>(g_ownerHits.load()),
            static_cast<unsigned long long>(g_readyHits.load()), static_cast<unsigned long long>(g_attempts.load()),
            static_cast<unsigned long long>(g_confirmations.load()));
    }
}

} // namespace

bool InstallHPLHandsBootstrap(const Config& config)
{
    std::lock_guard lock(g_mutex);
    if (!config.hplHandAlwaysVisible || !config.hplHandBootstrap) {
        Logger::Instance().Write(LogLevel::Info, "hpl_hands_bootstrap disabled alwaysVisible=%d bootstrap=%d",
            config.hplHandAlwaysVisible ? 1 : 0, config.hplHandBootstrap ? 1 : 0);
        return true;
    }
    if (g_target != nullptr) return true;
    g_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (g_base == 0) return false;
    if (!ValidateContract(kPostUpdateRva, kPostUpdate, sizeof(kPostUpdate), "post_update")
        || !ValidateContract(0x1dc170, kHasMethod, sizeof(kHasMethod), "has_method")
        || !ValidateContract(0x1dd830, kPrepare, sizeof(kPrepare), "prepare")
        || !ValidateContract(0x1dc190, kSetBool, sizeof(kSetBool), "set_bool")
        || !ValidateContract(0x1dd340, kExecute, sizeof(kExecute), "execute")
        || !ValidateContract(0xcccb0, kGetCurrentMap, sizeof(kGetCurrentMap), "current_map")) return false;
    g_hasMethod = reinterpret_cast<MethodFn>(g_base + 0x1dc170);
    g_prepare = reinterpret_cast<MethodFn>(g_base + 0x1dd830);
    g_setBool = reinterpret_cast<SetBoolFn>(g_base + 0x1dc190);
    g_execute = reinterpret_cast<ExecuteFn>(g_base + 0x1dd340);
    void* target = reinterpret_cast<void*>(g_base + kPostUpdateRva);
    auto status = MH_CreateHook(target, reinterpret_cast<void*>(&HookPostUpdate),
        reinterpret_cast<void**>(&g_originalPostUpdate));
    if (status == MH_OK) {
        status = MH_EnableHook(target);
        if (status != MH_OK) { MH_RemoveHook(target); g_originalPostUpdate = nullptr; }
    }
    Logger::Instance().Write(status == MH_OK ? LogLevel::Info : LogLevel::Error,
        "hpl_hands_bootstrap install=%s receiver_adjust=-0x110 module=18 phase=after_native_PostUpdate settle=30_frames_and_750ms max_attempts=1_per_world",
        MH_StatusToString(status));
    if (status == MH_OK) g_target = target;
    return status == MH_OK;
}

void RemoveHPLHandsBootstrap()
{
    if (g_target != nullptr) MH_DisableHook(g_target);
    std::lock_guard lock(g_mutex);
    if (g_target != nullptr) MH_RemoveHook(g_target);
    g_target = nullptr;
    Logger::Instance().Write(LogLevel::Info,
        "hpl_hands_bootstrap summary callbacks=%llu owners=%llu ready=%llu attempts=%llu seeds=%llu nativeFault=%d",
        static_cast<unsigned long long>(g_calls.load()), static_cast<unsigned long long>(g_ownerHits.load()),
        static_cast<unsigned long long>(g_readyHits.load()), static_cast<unsigned long long>(g_attempts.load()),
        static_cast<unsigned long long>(g_confirmations.load()), g_faulted.load() ? 1 : 0);
}

} // namespace somavr

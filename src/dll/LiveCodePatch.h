#pragma once

// Shared machinery for rewriting live code in the running game.
//
// Overwriting bytes of a function that other threads may be executing is not
// atomic: a thread parked inside the range can read a half-old, half-new
// instruction stream and jump somewhere that is not code. MinHook handles this
// internally, but the short HPL setters that cannot host a MinHook trampoline
// are patched by hand, and those writes need the same protection.
//
// WriteCodeBytes suspends every peer thread, refuses the write if any of them
// has its instruction pointer inside the patch range, and verifies the target
// still holds the bytes the caller expected before committing. Callers that
// spill past the end of a short function must pass the FULL patch extent as
// `size`, not the function body length, or the overlap check will wave through
// a thread parked in the padding about to be overwritten.

#include "PatchSafety.h"

#include <Windows.h>
#include <TlHelp32.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace somavr::live_patch {

enum class PatchWriteFailure {
    None,
    ThreadSnapshot,
    ThreadEnumeration,
    ThreadOpen,
    ThreadSuspend,
    ThreadContext,
    InstructionPointerInRange,
    ExpectedBytesChanged,
    Protect,
};

inline const char* PatchWriteFailureName(PatchWriteFailure failure)
{
    switch (failure) {
    case PatchWriteFailure::None: return "none";
    case PatchWriteFailure::ThreadSnapshot: return "thread_snapshot";
    case PatchWriteFailure::ThreadEnumeration: return "thread_enumeration";
    case PatchWriteFailure::ThreadOpen: return "thread_open";
    case PatchWriteFailure::ThreadSuspend: return "thread_suspend";
    case PatchWriteFailure::ThreadContext: return "thread_context";
    case PatchWriteFailure::InstructionPointerInRange: return "instruction_pointer_in_range";
    case PatchWriteFailure::ExpectedBytesChanged: return "expected_bytes_changed";
    case PatchWriteFailure::Protect: return "virtual_protect";
    }
    return "unknown";
}

class ScopedPeerThreadSuspension final {
public:
    ~ScopedPeerThreadSuspension()
    {
        for (auto it = threads_.rbegin(); it != threads_.rend(); ++it) {
            ResumeThread(it->handle);
            CloseHandle(it->handle);
        }
    }

    bool Acquire(uintptr_t patchStart, size_t patchSize, PatchWriteFailure& failure, DWORD& threadId)
    {
        threads_.reserve(128);
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            failure = PatchWriteFailure::ThreadSnapshot;
            return false;
        }

        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        if (!Thread32First(snapshot, &entry)) {
            CloseHandle(snapshot);
            failure = PatchWriteFailure::ThreadEnumeration;
            return false;
        }

        const DWORD processId = GetCurrentProcessId();
        const DWORD currentThreadId = GetCurrentThreadId();
        do {
            if (entry.th32OwnerProcessID != processId || entry.th32ThreadID == currentThreadId) {
                continue;
            }
            HANDLE thread = OpenThread(
                THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                FALSE,
                entry.th32ThreadID);
            if (thread == nullptr) {
                // A thread can exit after it appears in the Toolhelp snapshot.
                // OpenThread reports that race as ERROR_INVALID_PARAMETER; it
                // is no longer a peer that can execute the patch range.
                if (GetLastError() == ERROR_INVALID_PARAMETER) {
                    continue;
                }
                CloseHandle(snapshot);
                failure = PatchWriteFailure::ThreadOpen;
                threadId = entry.th32ThreadID;
                return false;
            }
            if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
                CloseHandle(thread);
                CloseHandle(snapshot);
                failure = PatchWriteFailure::ThreadSuspend;
                threadId = entry.th32ThreadID;
                return false;
            }
            threads_.push_back({thread});

            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(thread, &context)) {
                CloseHandle(snapshot);
                failure = PatchWriteFailure::ThreadContext;
                threadId = entry.th32ThreadID;
                return false;
            }
#if defined(_M_X64)
            const uintptr_t instructionPointer = static_cast<uintptr_t>(context.Rip);
#else
            const uintptr_t instructionPointer = static_cast<uintptr_t>(context.Eip);
#endif
            if (patch_safety::InstructionPointerOverlapsPatch(
                    instructionPointer, patchStart, patchSize)) {
                CloseHandle(snapshot);
                failure = PatchWriteFailure::InstructionPointerInRange;
                threadId = entry.th32ThreadID;
                return false;
            }
        } while (Thread32Next(snapshot, &entry));

        const DWORD enumerationError = GetLastError();
        CloseHandle(snapshot);
        if (enumerationError != ERROR_NO_MORE_FILES) {
            failure = PatchWriteFailure::ThreadEnumeration;
            return false;
        }
        return true;
    }

private:
    struct SuspendedThread {
        HANDLE handle = nullptr;
    };
    std::vector<SuspendedThread> threads_;
};

inline bool WriteCodeBytes(void* target, const void* expected, const void* bytes, size_t size,
                    PatchWriteFailure& failure, DWORD& blockedThreadId)
{
    ScopedPeerThreadSuspension suspendedThreads;
    if (target == nullptr || expected == nullptr || bytes == nullptr || size == 0
        || !suspendedThreads.Acquire(
            reinterpret_cast<uintptr_t>(target), size, failure, blockedThreadId)) {
        return false;
    }
    if (std::memcmp(target, expected, size) != 0) {
        failure = PatchWriteFailure::ExpectedBytesChanged;
        return false;
    }
    DWORD oldProtect = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        failure = PatchWriteFailure::Protect;
        return false;
    }
    std::memcpy(target, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    DWORD ignored = 0;
    VirtualProtect(target, size, oldProtect, &ignored);
    return true;
}

} // namespace somavr::live_patch

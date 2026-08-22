#include "NativeMemoryAccess.h"

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <limits>

namespace somavr::native_memory {
namespace {

bool HasReadableProtection(DWORD protection)
{
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    switch (protection & 0xff) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

bool HasWritableProtection(DWORD protection)
{
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    switch (protection & 0xff) {
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

bool ValidateRange(const void* address, size_t size, bool writable)
{
    if (size == 0) {
        return true;
    }
    if (address == nullptr) {
        return false;
    }

    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    if (start > (std::numeric_limits<uintptr_t>::max)() - size) {
        return false;
    }
    const uintptr_t end = start + size;
    uintptr_t cursor = start;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == 0
            || memory.State != MEM_COMMIT
            || (writable
                ? !HasWritableProtection(memory.Protect)
                : !HasReadableProtection(memory.Protect))) {
            return false;
        }
        const uintptr_t regionStart = reinterpret_cast<uintptr_t>(memory.BaseAddress);
        if (regionStart > (std::numeric_limits<uintptr_t>::max)() - memory.RegionSize) {
            return false;
        }
        const uintptr_t regionEnd = regionStart + memory.RegionSize;
        if (regionEnd <= cursor) {
            return false;
        }
        cursor = regionEnd < end ? regionEnd : end;
    }
    return true;
}

bool ResolveFieldAddress(
    const void* object, size_t offset, size_t size, uintptr_t& address)
{
    if (object == nullptr) {
        return false;
    }
    const uintptr_t base = reinterpret_cast<uintptr_t>(object);
    const uintptr_t maximum = (std::numeric_limits<uintptr_t>::max)();
    if (base > maximum - offset) {
        return false;
    }
    address = base + offset;
    return size <= maximum - address;
}

} // namespace

bool IsReadableRange(const void* address, size_t size)
{
    return ValidateRange(address, size, false);
}

bool IsWritableRange(void* address, size_t size)
{
    return ValidateRange(address, size, true);
}

bool IsReadableFieldRange(const void* object, size_t offset, size_t size)
{
    uintptr_t address = 0;
    return ResolveFieldAddress(object, offset, size, address)
        && IsReadableRange(reinterpret_cast<const void*>(address), size);
}

bool IsWritableFieldRange(void* object, size_t offset, size_t size)
{
    uintptr_t address = 0;
    return ResolveFieldAddress(object, offset, size, address)
        && IsWritableRange(reinterpret_cast<void*>(address), size);
}

bool TryReadBytes(const void* source, void* destination, size_t size)
{
    if (destination == nullptr || !IsReadableRange(source, size)) {
        return false;
    }
    bool copied = false;
    __try {
        std::memcpy(destination, source, size);
        copied = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        copied = false;
    }
    return copied;
}

bool TryWriteBytes(void* destination, const void* source, size_t size)
{
    if (source == nullptr || !IsWritableRange(destination, size)) {
        return false;
    }
    bool copied = false;
    __try {
        std::memcpy(destination, source, size);
        copied = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        copied = false;
    }
    return copied;
}

bool TryReadFieldBytes(
    const void* object, size_t offset, void* destination, size_t size)
{
    uintptr_t address = 0;
    return ResolveFieldAddress(object, offset, size, address)
        && TryReadBytes(reinterpret_cast<const void*>(address), destination, size);
}

bool TryWriteFieldBytes(
    void* object, size_t offset, const void* source, size_t size)
{
    uintptr_t address = 0;
    return ResolveFieldAddress(object, offset, size, address)
        && TryWriteBytes(reinterpret_cast<void*>(address), source, size);
}

} // namespace somavr::native_memory

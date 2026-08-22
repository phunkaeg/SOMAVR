#include "NativeMemoryAccess.h"

#include <Windows.h>

#include <cstdint>
#include <iostream>
#include <limits>

namespace {

int Check(bool condition, const char* message)
{
    if (condition) {
        return 0;
    }
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    int failures = 0;
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    const size_t pageSize = systemInfo.dwPageSize;
    auto* pages = static_cast<std::byte*>(VirtualAlloc(
        nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    failures += Check(pages != nullptr, "allocate test pages");
    if (pages == nullptr) {
        return failures;
    }

    uint32_t source = 0x12345678;
    uint32_t destination = 0;
    failures += Check(
        somavr::native_memory::TryWrite(pages, source),
        "write committed read/write page");
    failures += Check(
        somavr::native_memory::TryRead(pages, destination) && destination == source,
        "read committed read/write page");
    const uint32_t fieldSource = 0xaabbccdd;
    uint32_t fieldDestination = 0;
    failures += Check(
        somavr::native_memory::TryWriteField(pages, 32, fieldSource),
        "write committed field");
    failures += Check(
        somavr::native_memory::IsReadableFieldRange(pages, 32, sizeof(fieldDestination))
            && somavr::native_memory::IsWritableFieldRange(pages, 32, sizeof(fieldDestination)),
        "validate committed field range");
    failures += Check(
        somavr::native_memory::TryReadField(pages, 32, fieldDestination)
            && fieldDestination == fieldSource,
        "read committed field");

    DWORD previousProtection = 0;
    failures += Check(
        VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &previousProtection) != FALSE,
        "protect second page");
    failures += Check(
        !somavr::native_memory::IsReadableRange(pages + pageSize - 2, sizeof(uint32_t)),
        "reject range crossing into no-access page");
    failures += Check(
        !somavr::native_memory::TryRead(pages + pageSize, destination),
        "reject no-access read");
    failures += Check(
        !somavr::native_memory::TryWrite(pages + pageSize, source),
        "reject no-access write");

    failures += Check(
        VirtualProtect(pages, pageSize, PAGE_READONLY, &previousProtection) != FALSE,
        "protect first page read-only");
    failures += Check(
        somavr::native_memory::TryRead(pages, destination) && destination == source,
        "allow read-only read");
    failures += Check(
        !somavr::native_memory::TryWrite(pages, source),
        "reject read-only write");
    failures += Check(
        !somavr::native_memory::TryRead<uint32_t>(nullptr, destination),
        "reject null read");
    auto* wrappingAddress = reinterpret_cast<void*>(
        (std::numeric_limits<uintptr_t>::max)() - 1);
    failures += Check(
        !somavr::native_memory::TryReadField(wrappingAddress, 4, destination),
        "reject wrapping field read");
    failures += Check(
        !somavr::native_memory::TryWriteField(wrappingAddress, 4, source),
        "reject wrapping field write");
    failures += Check(
        !somavr::native_memory::IsReadableFieldRange(
            wrappingAddress, 4, sizeof(destination)),
        "reject wrapping field range preflight");

    VirtualFree(pages, 0, MEM_RELEASE);
    if (failures == 0) {
        std::cout << "Native memory access tests passed\n";
    }
    return failures;
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace somavr::native_memory {

bool IsReadableRange(const void* address, size_t size);
bool IsWritableRange(void* address, size_t size);
bool IsReadableFieldRange(const void* object, size_t offset, size_t size);
bool IsWritableFieldRange(void* object, size_t offset, size_t size);
bool TryReadBytes(const void* source, void* destination, size_t size);
bool TryWriteBytes(void* destination, const void* source, size_t size);
bool TryReadFieldBytes(
    const void* object, size_t offset, void* destination, size_t size);
bool TryWriteFieldBytes(
    void* object, size_t offset, const void* source, size_t size);

template <typename T>
bool TryRead(const void* address, T& value)
{
    return TryReadBytes(address, &value, sizeof(value));
}

template <typename T>
bool TryReadField(const void* object, size_t offset, T& value)
{
    return TryReadFieldBytes(object, offset, &value, sizeof(value));
}

template <typename T>
bool TryWrite(void* address, const T& value)
{
    return TryWriteBytes(address, &value, sizeof(value));
}

template <typename T>
bool TryWriteField(void* object, size_t offset, const T& value)
{
    return TryWriteFieldBytes(object, offset, &value, sizeof(value));
}

} // namespace somavr::native_memory

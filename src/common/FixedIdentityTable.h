#pragma once

#include <array>
#include <cstddef>
#include <functional>

namespace somavr {

template <typename Key, typename Value, size_t Capacity>
class FixedIdentityTable {
    static_assert(Capacity > 0);

public:
    struct InsertResult {
        Value* value = nullptr;
        bool inserted = false;
    };

    InsertResult FindOrInsert(const Key& key)
    {
        const size_t start = std::hash<Key>{}(key) % Capacity;
        for (size_t probe = 0; probe < Capacity; ++probe) {
            Entry& entry = entries_[(start + probe) % Capacity];
            if (entry.occupied) {
                if (entry.key == key) {
                    return {&entry.value, false};
                }
                continue;
            }
            entry.key = key;
            entry.value = Value{};
            entry.occupied = true;
            ++size_;
            return {&entry.value, true};
        }
        return {};
    }

    Value* Find(const Key& key)
    {
        return const_cast<Value*>(
            static_cast<const FixedIdentityTable*>(this)->Find(key));
    }

    const Value* Find(const Key& key) const
    {
        const size_t start = std::hash<Key>{}(key) % Capacity;
        for (size_t probe = 0; probe < Capacity; ++probe) {
            const Entry& entry = entries_[(start + probe) % Capacity];
            if (!entry.occupied) return nullptr;
            if (entry.key == key) return &entry.value;
        }
        return nullptr;
    }

    void Clear()
    {
        for (Entry& entry : entries_) entry = Entry{};
        size_ = 0;
    }

    size_t Size() const { return size_; }
    constexpr size_t MaxSize() const { return Capacity; }

private:
    struct Entry {
        Key key{};
        Value value{};
        bool occupied = false;
    };

    std::array<Entry, Capacity> entries_{};
    size_t size_ = 0;
};

} // namespace somavr

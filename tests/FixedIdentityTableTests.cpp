#include "FixedIdentityTable.h"

#include <cstdint>
#include <iostream>

namespace {

int Check(bool condition, const char* message)
{
    if (condition) return 0;
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    int failures = 0;
    somavr::FixedIdentityTable<uint32_t, uint32_t, 4> table;

    auto first = table.FindOrInsert(1);
    failures += Check(first.value != nullptr && first.inserted, "insert first identity");
    *first.value = 101;

    auto collision = table.FindOrInsert(5);
    failures += Check(
        collision.value != nullptr && collision.inserted,
        "insert colliding identity");
    *collision.value = 505;
    failures += Check(
        table.Find(1) != nullptr && *table.Find(1) == 101
            && table.Find(5) != nullptr && *table.Find(5) == 505,
        "preserve colliding values");

    const auto duplicate = table.FindOrInsert(1);
    failures += Check(
        duplicate.value == first.value && !duplicate.inserted && table.Size() == 2,
        "reuse existing identity");

    table.FindOrInsert(9);
    table.FindOrInsert(13);
    failures += Check(table.Size() == table.MaxSize(), "fill fixed capacity");
    failures += Check(
        table.FindOrInsert(17).value == nullptr && table.Size() == 4,
        "fail closed when full");
    failures += Check(table.Find(99) == nullptr, "report missing identity");

    table.Clear();
    failures += Check(
        table.Size() == 0 && table.Find(1) == nullptr
            && table.FindOrInsert(17).value != nullptr,
        "clear and reuse table");

    if (failures == 0) std::cout << "Fixed identity table tests passed\n";
    return failures;
}

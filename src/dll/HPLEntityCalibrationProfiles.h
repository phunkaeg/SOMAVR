#pragma once

#include "Config.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

namespace somavr::entity_calibration {

enum class Family {
    PlayerHands,
    HudObject,
    SocketedHudObject,
    Flashlight,
    ReadObject,
};

struct Capabilities {
    bool playerHands = false;
    bool hudObject = false;
    bool socketedHudObject = false;
    bool flashlight = false;
    bool readObject = false;
};

struct Values {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float offsetZ = 0.0f;
    float pitchDegrees = 0.0f;
    float yawDegrees = 0.0f;
    float rollDegrees = 0.0f;
    float scale = 1.0f;
    float readDistanceScale = 1.0f;
    float readObjectScale = 1.0f;
    bool twoHand = false;
};

struct ResolvedProfile {
    bool valid = false;
    bool loadedFromDisk = false;
    uint64_t id = 0;
    Family family = Family::ReadObject;
    std::string key;
    Values values{};
};

class Store {
public:
    void Initialize(const Config& config, const std::filesystem::path& path);
    ResolvedProfile Resolve(const std::string& entityName, const Capabilities& capabilities);
    bool Update(const std::string& key, const Values& values);
    bool Save();
    void Reset();
    std::string SummaryString() const;

    static const char* FamilyName(Family family);

private:
    struct Entry {
        ResolvedProfile profile;
    };

    Family ResolveFamily(const Capabilities& capabilities) const;
    Values BaselineFor(Family family) const;
    bool LoadLocked();

    mutable std::mutex mutex_;
    Config baselineConfig_{};
    std::filesystem::path path_;
    std::unordered_map<std::string, Entry> entries_;
    uint64_t nextId_ = 1;
    uint64_t resolves_ = 0;
    uint64_t cacheHits_ = 0;
    uint64_t seeded_ = 0;
    uint64_t loaded_ = 0;
    uint64_t updates_ = 0;
    uint64_t saves_ = 0;
    uint64_t saveFailures_ = 0;
    bool initialized_ = false;
    bool dirty_ = false;
};

} // namespace somavr::entity_calibration

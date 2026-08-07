#include "HPLEntityCalibrationProfiles.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace somavr::entity_calibration {
namespace {

constexpr uint32_t kCompleteFieldMask = (1u << 11u) - 1u;

std::string Trim(std::string value)
{
    const auto whitespace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), whitespace));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), whitespace).base(), value.end());
    return value;
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool ParseFloat(const std::string& text, float& value)
{
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0' || !std::isfinite(parsed)) {
        return false;
    }
    value = parsed;
    return true;
}

bool ParseBool(const std::string& text, bool& value)
{
    const std::string normalized = Lower(Trim(text));
    if (normalized == "1" || normalized == "true" || normalized == "yes") {
        value = true;
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no") {
        value = false;
        return true;
    }
    return false;
}

bool ParseFamily(const std::string& text, Family& family)
{
    const std::string normalized = Lower(Trim(text));
    if (normalized == "player_hands") family = Family::PlayerHands;
    else if (normalized == "hud_object") family = Family::HudObject;
    else if (normalized == "socketed_hud_object") family = Family::SocketedHudObject;
    else if (normalized == "flashlight") family = Family::Flashlight;
    else if (normalized == "read_object") family = Family::ReadObject;
    else return false;
    return true;
}

bool ValuesPlausible(const Values& values)
{
    return std::isfinite(values.offsetX) && std::fabs(values.offsetX) <= 5.0f
        && std::isfinite(values.offsetY) && std::fabs(values.offsetY) <= 5.0f
        && std::isfinite(values.offsetZ) && std::fabs(values.offsetZ) <= 5.0f
        && std::isfinite(values.pitchDegrees) && std::fabs(values.pitchDegrees) <= 180.0f
        && std::isfinite(values.yawDegrees) && std::fabs(values.yawDegrees) <= 180.0f
        && std::isfinite(values.rollDegrees) && std::fabs(values.rollDegrees) <= 180.0f
        && std::isfinite(values.scale) && values.scale >= 0.05f && values.scale <= 10.0f
        && std::isfinite(values.readDistanceScale)
        && values.readDistanceScale >= 0.1f && values.readDistanceScale <= 10.0f
        && std::isfinite(values.readObjectScale)
        && values.readObjectScale >= 0.05f && values.readObjectScale <= 10.0f;
}

std::string SafeSectionKey(const std::string& key)
{
    std::string safe = key;
    std::replace(safe.begin(), safe.end(), ']', '_');
    std::replace(safe.begin(), safe.end(), '\r', '_');
    std::replace(safe.begin(), safe.end(), '\n', '_');
    return safe;
}

} // namespace

void Store::Initialize(const Config& config, const std::filesystem::path& path)
{
    std::lock_guard lock(mutex_);
    baselineConfig_ = config;
    path_ = path;
    entries_.clear();
    nextId_ = 1;
    resolves_ = 0;
    cacheHits_ = 0;
    seeded_ = 0;
    loaded_ = 0;
    updates_ = 0;
    saves_ = 0;
    saveFailures_ = 0;
    dirty_ = false;
    initialized_ = true;
    LoadLocked();
}

ResolvedProfile Store::Resolve(
    const std::string& entityName,
    const Capabilities& capabilities)
{
    std::lock_guard lock(mutex_);
    ++resolves_;
    if (!initialized_ || entityName.empty()) {
        return {};
    }
    const auto found = entries_.find(entityName);
    if (found != entries_.end()) {
        ++cacheHits_;
        return found->second.profile;
    }

    ResolvedProfile profile;
    profile.valid = true;
    profile.id = nextId_++;
    profile.family = ResolveFamily(capabilities);
    profile.key = entityName;
    profile.values = BaselineFor(profile.family);
    entries_.emplace(entityName, Entry{profile});
    ++seeded_;
    dirty_ = true;
    return profile;
}

bool Store::Update(const std::string& key, const Values& values)
{
    if (!ValuesPlausible(values)) {
        return false;
    }
    std::lock_guard lock(mutex_);
    const auto found = entries_.find(key);
    if (found == entries_.end()) {
        return false;
    }
    found->second.profile.values = values;
    ++updates_;
    dirty_ = true;
    return true;
}

bool Store::Save()
{
    std::lock_guard lock(mutex_);
    if (!initialized_ || !dirty_) {
        return true;
    }
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) {
        ++saveFailures_;
        return false;
    }

    std::vector<const Entry*> ordered;
    ordered.reserve(entries_.size());
    for (const auto& [key, entry] : entries_) {
        (void)key;
        ordered.push_back(&entry);
    }
    std::sort(ordered.begin(), ordered.end(), [](const Entry* left, const Entry* right) {
        return left->profile.key < right->profile.key;
    });

    const std::filesystem::path temporary = path_.wstring() + L".tmp";
    std::ofstream output(temporary, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        ++saveFailures_;
        return false;
    }
    output << "# SOMAVR exact-entity calibration profiles\n"
           << "# Generated from live HPL identity evidence. Profiles are telemetry-only until promoted.\n"
           << "Version=1\n\n";
    output << std::setprecision(9);
    for (const Entry* entry : ordered) {
        const ResolvedProfile& profile = entry->profile;
        const Values& values = profile.values;
        output << "[Profile:" << SafeSectionKey(profile.key) << "]\n"
               << "Family=" << FamilyName(profile.family) << "\n"
               << "OffsetX=" << values.offsetX << "\n"
               << "OffsetY=" << values.offsetY << "\n"
               << "OffsetZ=" << values.offsetZ << "\n"
               << "PitchDegrees=" << values.pitchDegrees << "\n"
               << "YawDegrees=" << values.yawDegrees << "\n"
               << "RollDegrees=" << values.rollDegrees << "\n"
               << "Scale=" << values.scale << "\n"
               << "ReadDistanceScale=" << values.readDistanceScale << "\n"
               << "ReadObjectScale=" << values.readObjectScale << "\n"
               << "TwoHand=" << (values.twoHand ? 1 : 0) << "\n\n";
    }
    output.flush();
    const bool streamOk = output.good();
    output.close();
    if (!streamOk) {
        std::filesystem::remove(temporary, ec);
        ++saveFailures_;
        return false;
    }

    std::filesystem::remove(path_, ec);
    ec.clear();
    std::filesystem::rename(temporary, path_, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        ++saveFailures_;
        return false;
    }
    dirty_ = false;
    ++saves_;
    return true;
}

void Store::Reset()
{
    std::lock_guard lock(mutex_);
    entries_.clear();
    path_.clear();
    initialized_ = false;
    dirty_ = false;
}

std::string Store::SummaryString() const
{
    std::lock_guard lock(mutex_);
    std::ostringstream out;
    out << "initialized=" << (initialized_ ? 1 : 0)
        << " path=" << path_.string()
        << " profiles=" << entries_.size()
        << " resolves=" << resolves_
        << " cacheHits=" << cacheHits_
        << " seeded=" << seeded_
        << " loaded=" << loaded_
        << " updates=" << updates_
        << " saves=" << saves_
        << " saveFailures=" << saveFailures_
        << " dirty=" << (dirty_ ? 1 : 0)
        << " policy=resolve_once_into_entity_identity_no_hot_path_map_lookup";
    return out.str();
}

const char* Store::FamilyName(Family family)
{
    switch (family) {
    case Family::PlayerHands: return "player_hands";
    case Family::HudObject: return "hud_object";
    case Family::SocketedHudObject: return "socketed_hud_object";
    case Family::Flashlight: return "flashlight";
    case Family::ReadObject: return "read_object";
    }
    return "read_object";
}

Family Store::ResolveFamily(const Capabilities& capabilities) const
{
    if (capabilities.playerHands) return Family::PlayerHands;
    if (capabilities.flashlight) return Family::Flashlight;
    if (capabilities.socketedHudObject) return Family::SocketedHudObject;
    if (capabilities.hudObject) return Family::HudObject;
    return Family::ReadObject;
}

Values Store::BaselineFor(Family family) const
{
    Values values;
    values.readDistanceScale = baselineConfig_.hplControllerReadObjectDistanceScale;
    values.readObjectScale = baselineConfig_.hplControllerReadObjectScale;
    switch (family) {
    case Family::PlayerHands:
        values.offsetX = baselineConfig_.hplHandRootOffsetX;
        values.offsetY = baselineConfig_.hplHandRootOffsetY;
        values.offsetZ = baselineConfig_.hplHandRootOffsetZ;
        values.pitchDegrees = baselineConfig_.hplHandRootPitchDegrees;
        values.yawDegrees = baselineConfig_.hplHandRootYawDegrees;
        values.rollDegrees = baselineConfig_.hplHandRootRollDegrees;
        values.scale = baselineConfig_.hplHandTargetScale;
        break;
    case Family::HudObject:
        values.offsetX = baselineConfig_.hplHudObjectOffsetX;
        values.offsetY = baselineConfig_.hplHudObjectOffsetY;
        values.offsetZ = baselineConfig_.hplHudObjectOffsetZ;
        values.pitchDegrees = baselineConfig_.hplHudObjectPitchDegrees;
        values.yawDegrees = baselineConfig_.hplHudObjectYawDegrees;
        values.rollDegrees = baselineConfig_.hplHudObjectRollDegrees;
        values.twoHand = baselineConfig_.hplControllerTwoHandHudObject;
        break;
    case Family::Flashlight:
        values.offsetX = baselineConfig_.hplFlashlightOffsetX;
        values.offsetY = baselineConfig_.hplFlashlightOffsetY;
        values.offsetZ = baselineConfig_.hplFlashlightOffsetZ;
        values.pitchDegrees = baselineConfig_.hplFlashlightPitchDegrees;
        values.yawDegrees = baselineConfig_.hplFlashlightYawDegrees;
        values.rollDegrees = baselineConfig_.hplFlashlightRollDegrees;
        break;
    case Family::SocketedHudObject:
    case Family::ReadObject:
        break;
    }
    return values;
}

bool Store::LoadLocked()
{
    std::ifstream input(path_);
    if (!input.is_open()) {
        return true;
    }

    struct Pending {
        std::string key;
        Family family = Family::ReadObject;
        Values values{};
        uint32_t fields = 0;
    } pending;
    const auto commit = [&]() {
        if (pending.key.empty() || pending.fields != kCompleteFieldMask
            || !ValuesPlausible(pending.values)) {
            pending = {};
            return;
        }
        ResolvedProfile profile;
        profile.valid = true;
        profile.loadedFromDisk = true;
        profile.id = nextId_++;
        profile.family = pending.family;
        profile.key = pending.key;
        profile.values = pending.values;
        entries_[profile.key] = Entry{profile};
        ++loaded_;
        pending = {};
    };

    std::string line;
    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            commit();
            constexpr char prefix[] = "Profile:";
            const std::string section = line.substr(1, line.size() - 2);
            if (section.rfind(prefix, 0) == 0) {
                pending.key = section.substr(sizeof(prefix) - 1);
            }
            continue;
        }
        if (pending.key.empty()) {
            continue;
        }
        const size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = Lower(Trim(line.substr(0, separator)));
        const std::string value = Trim(line.substr(separator + 1));
        bool parsed = false;
        uint32_t bit = 0;
        if (key == "family") { parsed = ParseFamily(value, pending.family); bit = 0; }
        else if (key == "offsetx") { parsed = ParseFloat(value, pending.values.offsetX); bit = 1; }
        else if (key == "offsety") { parsed = ParseFloat(value, pending.values.offsetY); bit = 2; }
        else if (key == "offsetz") { parsed = ParseFloat(value, pending.values.offsetZ); bit = 3; }
        else if (key == "pitchdegrees") { parsed = ParseFloat(value, pending.values.pitchDegrees); bit = 4; }
        else if (key == "yawdegrees") { parsed = ParseFloat(value, pending.values.yawDegrees); bit = 5; }
        else if (key == "rolldegrees") { parsed = ParseFloat(value, pending.values.rollDegrees); bit = 6; }
        else if (key == "scale") { parsed = ParseFloat(value, pending.values.scale); bit = 7; }
        else if (key == "readdistancescale") { parsed = ParseFloat(value, pending.values.readDistanceScale); bit = 8; }
        else if (key == "readobjectscale") { parsed = ParseFloat(value, pending.values.readObjectScale); bit = 9; }
        else if (key == "twohand") { parsed = ParseBool(value, pending.values.twoHand); bit = 10; }
        if (parsed) {
            pending.fields |= 1u << bit;
        }
    }
    commit();
    return true;
}

} // namespace somavr::entity_calibration

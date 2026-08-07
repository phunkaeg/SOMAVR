#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

#include <cstdint>
#include <string>

namespace somavr {

enum class HPLAuthoredInteractionEventType : uint8_t {
    None,
    MedicineCapRemove,
    MedicineDrink,
};

struct HPLAuthoredInteractionEvent {
    HPLAuthoredInteractionEventType type = HPLAuthoredInteractionEventType::None;
    uint64_t frame = 0;
    void* entity = nullptr;
};

void ConfigureHPLAuthoredInteractionBridge(const Config& config, OpenXRRuntime* openxr);
void ObserveHPLAuthoredInteractionEntity(
    const std::string& name,
    void* entity,
    const float* worldMatrix,
    uint64_t frame);
void InvalidateHPLAuthoredInteractionEntity(void* entity);
bool ConsumeHPLAuthoredInteractionEvent(HPLAuthoredInteractionEvent& event);
void LogHPLAuthoredInteractionSummary();
void ResetHPLAuthoredInteractionBridge();

} // namespace somavr

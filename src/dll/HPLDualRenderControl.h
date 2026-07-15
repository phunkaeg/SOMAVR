#pragma once

#include <cstdint>

namespace somavr {

struct Config;

struct HPLDualRenderControlStatus {
    bool configured = false;
    bool ready = false;
    bool enabled = false;
    bool defaultEnabled = false;
    uint64_t changes = 0;
    uint64_t rejections = 0;
};

void InitializeHPLDualRenderControl(const Config& config);
void SetHPLDualRenderControlReady(bool ready);
bool SetHPLContinuousDualRenderEnabled(bool enabled, const char* source);
bool IsHPLContinuousDualRenderEnabled();
HPLDualRenderControlStatus GetHPLDualRenderControlStatus();
void RemoveHPLDualRenderControl();

} // namespace somavr

#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

#include <cstdint>

namespace somavr {

bool InstallOpenGLHooks(const Config& config, OpenXRRuntime* openxr);
void RemoveOpenGLHooks();
void LogOpenGLProofSummary();
uint64_t GetOpenGLRenderFrameHint();

} // namespace somavr

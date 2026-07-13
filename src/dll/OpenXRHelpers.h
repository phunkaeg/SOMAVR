#pragma once

#include "OpenXRRuntime.h"

#include <openxr/openxr.h>

#include <cstdint>
#include <string>
#include <vector>

namespace somavr::xr_helpers {

std::string XrResultString(XrResult result);
std::string XrVersionString(XrVersion version);
const char* ViewConfigurationTypeName(XrViewConfigurationType type);
const char* EnvironmentBlendModeName(XrEnvironmentBlendMode mode);
const char* ReferenceSpaceTypeName(XrReferenceSpaceType type);
const char* SessionStateName(XrSessionState state);
std::string GlFormatString(int64_t format);

bool ExtensionPresent(const std::vector<XrExtensionProperties>& extensions, const char* name);
std::string ExtensionSample(const std::vector<XrExtensionProperties>& extensions);
std::string SwapchainFormatSample(const std::vector<int64_t>& formats);

OpenXREyeView ToEyeView(const XrView& view, uint64_t gameFrame);
XrPosef ToXrPose(const OpenXREyeView& view);
XrFovf ToXrFov(const OpenXREyeView& view);

} // namespace somavr::xr_helpers

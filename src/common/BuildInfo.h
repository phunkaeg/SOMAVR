#pragma once

#include <Windows.h>

#include <string>

namespace somavr {

std::string BuildIdentityString();
void LogBuildIdentity(HMODULE module);

} // namespace somavr

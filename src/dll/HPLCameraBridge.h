#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

struct HPLCameraBridgeStatus {
    bool installed = false;
    bool trackingEnabled = false;
    bool stereoEnabled = false;
    bool projectionCentered = false;
    bool roomscaleEnabled = true;
    int stereoRenderEye = -1;
    uint64_t stereoRenderPoseFrame = 0;
    bool headWorldRotationValid = false;
    void* activeCamera = nullptr;
    void* activeFrustum = nullptr;
    uint64_t headPoseFrame = 0;
    float headWorldRotationX = 0.0f;
    float headWorldRotationY = 0.0f;
    float headWorldRotationZ = 0.0f;
    float headWorldRotationW = 1.0f;
};

bool InstallHPLCameraBridge(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLCameraBridge();
void LogHPLCameraBridgeSummary();
HPLCameraBridgeStatus GetHPLCameraBridgeStatus();
bool RequestHPLRecenter(const char* source);
void NotifyHPLPlayerCameraChanged(void* previousCamera, void* currentCamera);

} // namespace somavr

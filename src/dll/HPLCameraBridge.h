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
    uint64_t calibrationGeneration = 0;
    bool projectionParametersValid = false;
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    int projectionType = -1;
    float worldUnitsPerMeter = 1.0f;
    float headWorldRotationX = 0.0f;
    float headWorldRotationY = 0.0f;
    float headWorldRotationZ = 0.0f;
    float headWorldRotationW = 1.0f;
    bool headWorldPositionValid = false;
    bool cameraWorldPositionValid = false;
    bool nativeCameraBasisValid = false;
    float cameraWorldPositionX = 0.0f;
    float cameraWorldPositionY = 0.0f;
    float cameraWorldPositionZ = 0.0f;
    float nativeCameraForwardX = 0.0f;
    float nativeCameraForwardY = 0.0f;
    float nativeCameraForwardZ = -1.0f;
    float nativeCameraUpX = 0.0f;
    float nativeCameraUpY = 1.0f;
    float nativeCameraUpZ = 0.0f;
    float headWorldPositionX = 0.0f;
    float headWorldPositionY = 0.0f;
    float headWorldPositionZ = 0.0f;
    float headWorldOffsetX = 0.0f;
    float headWorldOffsetY = 0.0f;
    float headWorldOffsetZ = 0.0f;
};

struct HPLTrackedPoseWorld {
    bool valid = false;
    bool orientationTracked = false;
    bool positionTracked = false;
    uint64_t gameFrame = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float forwardX = 0.0f;
    float forwardY = 0.0f;
    float forwardZ = -1.0f;
    float upX = 0.0f;
    float upY = 1.0f;
    float upZ = 0.0f;
};

bool InstallHPLCameraBridge(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLCameraBridge();
void LogHPLCameraBridgeSummary();
HPLCameraBridgeStatus GetHPLCameraBridgeStatus();
bool ResolveHPLTrackedPoseWorld(
    const OpenXRControllerPose& pose,
    uint64_t gameFrame,
    HPLTrackedPoseWorld& worldPose);
bool ResolveHPLReferenceVectorWorld(
    float x,
    float y,
    float z,
    bool applyWorldScale,
    float& worldX,
    float& worldY,
    float& worldZ);
bool RequestHPLRecenter(const char* source);
bool SetHPLRoomscaleEnabled(bool enabled, const char* source);
bool SetHPLProjectionCentered(bool enabled, const char* source);
void NotifyHPLPlayerCameraChanged(void* previousCamera, void* currentCamera);

} // namespace somavr

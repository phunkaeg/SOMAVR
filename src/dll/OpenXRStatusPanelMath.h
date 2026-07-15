#pragma once

#include <cstdint>
#include <vector>

namespace somavr::status_panel_math {

constexpr int kActionCount = 6;

struct PanelModel {
    bool visible = false;
    int selectedAction = 0;
    bool trackingEnabled = false;
    bool stereoEnabled = false;
    bool roomscaleEnabled = false;
    bool projectionCentered = false;
    bool hudVisible = false;
    bool reticleVisible = false;
    bool inputAvailable = false;
    bool controllerTracked = false;
    bool authoredCameraActive = false;
    int playerState = -1;
    uint64_t gameFrame = 0;
};

bool RasterizePanel(
    const PanelModel& model,
    int width,
    int height,
    std::vector<uint8_t>& rgbaPixels);

} // namespace somavr::status_panel_math

#include "OpenXRStatusPanelMath.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

namespace somavr::status_panel_math {
namespace {

using Glyph = std::array<uint8_t, 7>;

Glyph GetGlyph(char value)
{
    switch (value) {
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 15};
    case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {31, 4, 4, 4, 4, 4, 31};
    case 'J': return {7, 2, 2, 2, 18, 18, 12};
    case 'K': return {17, 18, 20, 24, 20, 18, 17};
    case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'N': return {17, 25, 21, 19, 17, 17, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4};
    case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '0': return {14, 17, 19, 21, 25, 17, 14};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2};
    case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {14, 16, 16, 30, 17, 17, 14};
    case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14};
    case '9': return {14, 17, 17, 15, 1, 1, 14};
    case ':': return {0, 4, 4, 0, 4, 4, 0};
    case '-': return {0, 0, 0, 31, 0, 0, 0};
    case '>': return {16, 8, 4, 2, 4, 8, 16};
    case '/': return {1, 2, 2, 4, 8, 8, 16};
    case '.': return {0, 0, 0, 0, 0, 12, 12};
    default: return {};
    }
}

struct Canvas {
    int width = 0;
    int height = 0;
    std::vector<uint8_t>* pixels = nullptr;

    void SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        const int glY = height - 1 - y;
        const size_t offset = (static_cast<size_t>(glY) * width + x) * 4;
        (*pixels)[offset + 0] = red;
        (*pixels)[offset + 1] = green;
        (*pixels)[offset + 2] = blue;
        (*pixels)[offset + 3] = alpha;
    }

    void FillRect(int x, int y, int rectWidth, int rectHeight, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    {
        for (int row = 0; row < rectHeight; ++row) {
            for (int column = 0; column < rectWidth; ++column) {
                SetPixel(x + column, y + row, red, green, blue, alpha);
            }
        }
    }

    void Text(int x, int y, std::string_view text, int scale, uint8_t red, uint8_t green, uint8_t blue)
    {
        for (char value : text) {
            const Glyph glyph = GetGlyph(value);
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < 5; ++column) {
                    if ((glyph[row] & (1 << (4 - column))) != 0) {
                        FillRect(x + column * scale, y + row * scale, scale, scale, red, green, blue, 255);
                    }
                }
            }
            x += 6 * scale;
        }
    }
};

const char* OnOff(bool enabled)
{
    return enabled ? "ON" : "OFF";
}

} // namespace

bool RasterizePanel(
    const PanelModel& model,
    int width,
    int height,
    std::vector<uint8_t>& rgbaPixels)
{
    if (width < 512 || height < 256 || width > 4096 || height > 4096) return false;
    rgbaPixels.assign(static_cast<size_t>(width) * height * 4, 0);
    if (!model.visible) return true;

    Canvas canvas{width, height, &rgbaPixels};
    const int margin = std::max(width / 32, 16);
    const int scale = std::clamp(std::min(width / 300, height / 135), 2, 5);
    const int lineHeight = 10 * scale;
    canvas.FillRect(margin, margin, width - margin * 2, height - margin * 2, 8, 13, 18, 238);
    canvas.FillRect(margin, margin, width - margin * 2, scale, 44, 205, 181, 255);
    canvas.Text(margin + 5 * scale, margin + 5 * scale, "SOMAVR VR STATUS", scale, 238, 247, 247);

    char line[96] = {};
    int y = margin + 18 * scale;
    std::snprintf(line, sizeof(line), "TRACKING: %s   STEREO: %s   INPUT: %s",
        OnOff(model.trackingEnabled), OnOff(model.stereoEnabled), OnOff(model.inputAvailable));
    canvas.Text(margin + 5 * scale, y, line, scale, 139, 229, 211);
    y += lineHeight;
    std::snprintf(line, sizeof(line), "CONTROLLERS: %s   PLAYER STATE: %d   FRAME: %llu",
        model.controllerTracked ? "TRACKED" : "LOST", model.playerState,
        static_cast<unsigned long long>(model.gameFrame));
    canvas.Text(margin + 5 * scale, y, line, scale, 179, 193, 203);
    y += lineHeight;
    std::snprintf(line, sizeof(line), "AUTHORED CAMERA: %s", OnOff(model.authoredCameraActive));
    canvas.Text(margin + 5 * scale, y, line, scale, 179, 193, 203);
    y += lineHeight + 2 * scale;

    const std::string_view dualRenderAction = !model.dualRenderReady
        ? "SAME FRAME STEREO: UNAVAILABLE"
        : model.continuousDualRender ? "SAME FRAME STEREO: ON" : "SAME FRAME STEREO: OFF";
    const std::array<std::string_view, kActionCount> actions = {
        "RECENTER VR",
        model.roomscaleEnabled ? "ROOMSCALE: ON" : "ROOMSCALE: OFF",
        model.projectionCentered ? "CENTERED PROJECTION: ON" : "CENTERED PROJECTION: OFF",
        dualRenderAction,
        model.hudVisible ? "HUD LAYER: ON" : "HUD LAYER: OFF",
        model.reticleVisible ? "INTERACTION RETICLE: ON" : "INTERACTION RETICLE: OFF",
        "CLOSE",
    };
    const int selected = std::clamp(model.selectedAction, 0, kActionCount - 1);
    for (int action = 0; action < kActionCount; ++action) {
        const bool active = action == selected;
        if (active) {
            canvas.FillRect(margin + 3 * scale, y - scale, width - margin * 2 - 6 * scale,
                9 * scale, 22, 73, 76, 255);
        }
        canvas.Text(margin + 5 * scale, y, active ? ">" : " ", scale,
            active ? 255 : 179, active ? 202 : 193, active ? 83 : 203);
        canvas.Text(margin + 12 * scale, y, actions[action], scale,
            active ? 255 : 220, active ? 232 : 228, active ? 163 : 232);
        y += lineHeight;
    }

    canvas.Text(margin + 5 * scale, height - margin - 10 * scale,
        "F1 OR MENU PLUS SECONDARY TO CLOSE", scale, 139, 229, 211);
    return true;
}

} // namespace somavr::status_panel_math

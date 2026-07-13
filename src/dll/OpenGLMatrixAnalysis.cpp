#include "OpenGLMatrixAnalysis.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace somavr::gl_matrix {
namespace {

bool Nearly(float value, float target, float epsilon)
{
    return std::fabs(value - target) <= epsilon;
}

} // namespace

MatrixSummary SummarizeMatrix(const GLfloat* values)
{
    MatrixSummary summary = {};
    if (values == nullptr) {
        return summary;
    }

    summary.valid = true;
    summary.m00 = values[0];
    summary.m11 = values[5];
    summary.m22 = values[10];
    summary.m23 = values[11];
    summary.m32 = values[14];

    const bool finite =
        std::isfinite(summary.m00)
        && std::isfinite(summary.m11)
        && std::isfinite(summary.m22)
        && std::isfinite(summary.m23)
        && std::isfinite(summary.m32);
    const bool plausibleScale =
        finite
        && std::fabs(summary.m00) > 0.001f
        && std::fabs(summary.m00) < 1000.0f
        && std::fabs(summary.m11) > 0.001f
        && std::fabs(summary.m11) < 1000.0f;
    const bool perspectiveSentinel =
        Nearly(summary.m23, -1.0f, 0.05f)
        || Nearly(summary.m32, -1.0f, 0.05f)
        || Nearly(summary.m23, 1.0f, 0.05f)
        || Nearly(summary.m32, 1.0f, 0.05f);

    summary.projectionLike = plausibleScale && perspectiveSentinel;
    if (plausibleScale) {
        summary.fovYDegrees = (2.0f * std::atan(1.0f / std::fabs(summary.m11))) * 57.2957795f;
        summary.aspect = std::fabs(summary.m11 / summary.m00);
    }
    return summary;
}

std::string MatrixSummaryText(const MatrixSummary& summary)
{
    if (!summary.valid) {
        return "valid=0";
    }

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss.precision(4);
    oss << "valid=1 projectionLike=" << (summary.projectionLike ? 1 : 0)
        << " m00=" << summary.m00
        << " m11=" << summary.m11
        << " m22=" << summary.m22
        << " m23=" << summary.m23
        << " m32=" << summary.m32
        << " fovYDeg=" << summary.fovYDegrees
        << " aspect=" << summary.aspect;
    return oss.str();
}

std::string MatrixValuesText(const GLfloat* values)
{
    if (values == nullptr) {
        return {};
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    for (size_t i = 0; i < 16; ++i) {
        if (i != 0) {
            oss << ',';
        }
        oss << values[i];
    }
    return oss.str();
}

} // namespace somavr::gl_matrix

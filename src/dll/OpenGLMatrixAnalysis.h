#pragma once

#include <Windows.h>
#include <gl/GL.h>

#include <string>

namespace somavr::gl_matrix {

struct MatrixSummary {
    bool valid = false;
    bool projectionLike = false;
    float m00 = 0.0f;
    float m11 = 0.0f;
    float m22 = 0.0f;
    float m23 = 0.0f;
    float m32 = 0.0f;
    float fovYDegrees = 0.0f;
    float aspect = 0.0f;
};

MatrixSummary SummarizeMatrix(const GLfloat* values);
std::string MatrixSummaryText(const MatrixSummary& summary);
std::string MatrixValuesText(const GLfloat* values);

} // namespace somavr::gl_matrix

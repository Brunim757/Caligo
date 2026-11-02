#pragma once

#include <caligo/render/RhiTypes.h>

namespace caligo::render::detail {

struct FullscreenVertex {
  float position[2];
  float uv[2];
};

inline constexpr FullscreenVertex kFullscreenVertices[] = {
    {-1.0f, -1.0f, 0.0f, 1.0f},
    {1.0f, -1.0f, 1.0f, 1.0f},
    {-1.0f, 1.0f, 0.0f, 0.0f},
    {1.0f, 1.0f, 1.0f, 0.0f},
};

inline constexpr RhiVertexElement kFullscreenElements[] = {
    {"POSITION", 0, RhiVertexFormat::Float32x2, 0},
    {"TEXCOORD", 0, RhiVertexFormat::Float32x2, 8},
};

}
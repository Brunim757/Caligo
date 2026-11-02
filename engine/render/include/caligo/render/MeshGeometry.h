#pragma once

#include <cstdint>

namespace caligo::render {

class RhiBuffer;

struct MeshGeometry {
  const RhiBuffer* vertices = nullptr;
  uint32_t vertexStride = 0;
  const RhiBuffer* indices = nullptr;
  uint32_t indexCount = 0;
};

}

#pragma once

#include <caligo/core/Math.h>
#include <caligo/core/Result.h>
#include <cstdint>
#include <memory>
#include <span>

namespace caligo::render {

class RhiDevice;
class RhiBuffer;

struct TerrainSurface {
  uint32_t resolution = 0;
  float extent = 0.0f;
  std::span<const float> heights;
  std::span<const float> layerWeights;
  std::span<const math::Vec3> layerColors;
  math::Mat4 world{};
};

struct TerrainGeometry {
  const RhiBuffer* vertices = nullptr;
  uint32_t vertexStride = 0;
  const RhiBuffer* indices = nullptr;
  uint32_t indexCount = 0;
  math::Mat4 world{};
  math::Vec3 boundsMinimum{};
  math::Vec3 boundsMaximum{};
};

class TerrainMesh final {
public:
  struct Impl;

  TerrainMesh();
  ~TerrainMesh();

  TerrainMesh(const TerrainMesh&) = delete;
  TerrainMesh& operator=(const TerrainMesh&) = delete;
  TerrainMesh(TerrainMesh&&) = delete;
  TerrainMesh& operator=(TerrainMesh&&) = delete;

  [[nodiscard]] Result<void> Update(RhiDevice& device, const TerrainSurface& surface);
  [[nodiscard]] const TerrainGeometry& Geometry() const noexcept;
  [[nodiscard]] bool HasSurface() const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}

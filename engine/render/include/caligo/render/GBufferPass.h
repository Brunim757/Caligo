#pragma once

#include <caligo/core/Math.h>
#include <caligo/core/Result.h>
#include <caligo/render/MeshGeometry.h>
#include <cstdint>
#include <memory>
#include <span>

namespace caligo::render {

class RhiDevice;
class RhiTexture;
struct RenderItem;

class GBufferPass final {
public:
  struct Impl;

  GBufferPass();
  ~GBufferPass();

  GBufferPass(const GBufferPass&) = delete;
  GBufferPass& operator=(const GBufferPass&) = delete;
  GBufferPass(GBufferPass&&) = delete;
  GBufferPass& operator=(GBufferPass&&) = delete;

  [[nodiscard]] Result<void> Render(RhiDevice& device, const MeshGeometry& mesh,
                                    std::span<const RenderItem> items,
                                    const math::Mat4& viewProjection, const RhiTexture& albedo,
                                    const RhiTexture& normal, const RhiTexture& depth);

private:
  std::unique_ptr<Impl> m_impl;
};

}

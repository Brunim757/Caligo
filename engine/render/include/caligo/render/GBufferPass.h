#pragma once

#include <caligo/core/Math.h>
#include <caligo/core/Result.h>
#include <caligo/render/MeshGeometry.h>
#include <cstdint>
#include <memory>
#include <span>

namespace caligo::render {

class RhiDevice;
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
                                    const math::Mat4& viewProjection, uint32_t width,
                                    uint32_t height);
  [[nodiscard]] void* AlbedoView() const noexcept;
  [[nodiscard]] void* NormalView() const noexcept;
  [[nodiscard]] void* DepthView() const noexcept;
  [[nodiscard]] void* DepthStencilView() const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}

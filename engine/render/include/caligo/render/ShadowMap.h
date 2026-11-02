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

class ShadowMap final {
public:
  struct Impl;

  ShadowMap();
  ~ShadowMap();

  ShadowMap(const ShadowMap&) = delete;
  ShadowMap& operator=(const ShadowMap&) = delete;
  ShadowMap(ShadowMap&&) = delete;
  ShadowMap& operator=(ShadowMap&&) = delete;

  [[nodiscard]] Result<void> Render(RhiDevice& device, const MeshGeometry& mesh,
                                    std::span<const RenderItem> items,
                                    const math::Vec3& lightDirection);
  [[nodiscard]] void* ResourceView() const noexcept;
  [[nodiscard]] void* Sampler() const noexcept;
  [[nodiscard]] const math::Mat4& LightViewProjection() const noexcept;
  [[nodiscard]] bool HasContent() const noexcept;
  [[nodiscard]] float TexelSize() const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}

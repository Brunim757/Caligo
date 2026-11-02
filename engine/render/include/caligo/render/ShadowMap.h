#pragma once

#include <caligo/core/Math.h>
#include <caligo/core/Result.h>
#include <cstdint>
#include <memory>
#include <span>

namespace caligo::render {

class RhiBuffer;
class RhiDevice;
struct RenderItem;

struct ShadowMesh {
  const RhiBuffer* vertices = nullptr;
  uint32_t vertexStride = 0;
  const RhiBuffer* indices = nullptr;
  uint32_t indexCount = 0;
};

class ShadowMap final {
public:
  struct Impl;

  ShadowMap();
  ~ShadowMap();

  ShadowMap(const ShadowMap&) = delete;
  ShadowMap& operator=(const ShadowMap&) = delete;
  ShadowMap(ShadowMap&&) = delete;
  ShadowMap& operator=(ShadowMap&&) = delete;

  [[nodiscard]] Result<void> Render(RhiDevice& device, const ShadowMesh& mesh,
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

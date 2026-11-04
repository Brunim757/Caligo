#pragma once

#include <caligo/core/Math.h>
#include <caligo/core/Result.h>
#include <caligo/render/TerrainMesh.h>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace caligo::render {

class RhiDevice;

struct RenderCamera {
  math::Mat4 view{};
  math::Mat4 projection{};
};

struct RenderLight {
  math::Vec3 direction{0.4f, 1.0f, -0.35f};
  math::Vec3 color{1.0f, 0.98f, 0.94f};
};

struct RenderItem {
  math::Mat4 world{};
  math::Vec3 color{0.72f, 0.75f, 0.80f};
};

struct RenderEnvironment {
  math::Vec3 skyZenith{0.070f, 0.130f, 0.280f};
  math::Vec3 skyHorizon{0.460f, 0.560f, 0.700f};
  math::Vec3 skyGround{0.045f, 0.045f, 0.055f};
  math::Vec3 ambientSky{0.210f, 0.240f, 0.300f};
  math::Vec3 ambientGround{0.055f, 0.055f, 0.065f};
  math::Vec3 sunColor{1.0f, 0.97f, 0.90f};
  float sunIntensity = 14.0f;
  float sunAngularRadiusDegrees = 0.55f;
};

class SceneRenderer final {
public:
  struct Impl;

  SceneRenderer();
  ~SceneRenderer();

  SceneRenderer(const SceneRenderer&) = delete;
  SceneRenderer& operator=(const SceneRenderer&) = delete;
  SceneRenderer(SceneRenderer&&) = delete;
  SceneRenderer& operator=(SceneRenderer&&) = delete;

  [[nodiscard]] Result<void> Render(RhiDevice& device, std::span<const RenderItem> items,
                                    const RenderCamera& camera, const RenderLight& light,
                                    uint32_t width, uint32_t height,
                                    const RenderEnvironment& environment = {},
                                    std::span<const TerrainSurface> terrains = {});
  [[nodiscard]] void* ColorView() const noexcept;
  [[nodiscard]] Result<std::vector<uint8_t>> ReadColor() const;
  [[nodiscard]] uint32_t Width() const noexcept;
  [[nodiscard]] uint32_t Height() const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}
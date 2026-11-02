#pragma once

#include <caligo/core/Math.h>
#include <caligo/core/Result.h>
#include <cstdint>
#include <memory>

namespace caligo::render {

class RhiDevice;

struct LightingInputs {
  void* albedo = nullptr;
  void* normal = nullptr;
  void* depth = nullptr;
  void* shadowMap = nullptr;
  void* shadowSampler = nullptr;
};

struct LightingSettings {
  math::Mat4 inverseViewProjection{};
  math::Mat4 lightViewProjection{};
  math::Vec3 lightDirection{0.4f, 1.0f, -0.35f};
  math::Vec3 lightColor{1.0f, 0.98f, 0.94f};
  math::Vec3 background{0.020f, 0.025f, 0.035f};
  float shadowTexel = 0.0f;
  float shadowBias = 0.0f;
};

class LightingPass final {
public:
  struct Impl;

  LightingPass();
  ~LightingPass();

  LightingPass(const LightingPass&) = delete;
  LightingPass& operator=(const LightingPass&) = delete;
  LightingPass(LightingPass&&) = delete;
  LightingPass& operator=(LightingPass&&) = delete;

  [[nodiscard]] Result<void> Render(RhiDevice& device, const LightingInputs& inputs,
                                    const LightingSettings& settings, void* outputView,
                                    uint32_t width, uint32_t height);

private:
  std::unique_ptr<Impl> m_impl;
};

}

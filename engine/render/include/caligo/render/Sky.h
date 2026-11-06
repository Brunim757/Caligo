#pragma once

#include <caligo/render/SceneRenderer.h>
#include <cstddef>
#include <span>

namespace caligo::render {

struct SkyState {
  RenderEnvironment environment;
  RenderLight light;
};

struct SkyPreset {
  const char* name;
  float hours;
};

[[nodiscard]] SkyState EvaluateSky(float hours) noexcept;
[[nodiscard]] std::span<const SkyPreset> SkyPresets() noexcept;
[[nodiscard]] std::size_t NearestSkyPreset(float hours) noexcept;

}
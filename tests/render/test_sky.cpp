#include <caligo/render/Sky.h>
#include <cmath>
#include <doctest/doctest.h>
#include <span>

TEST_CASE("a hora do dia posiciona o sol") {
  CHECK(caligo::render::EvaluateSky(12.0f).light.direction.y > 0.99f);
  CHECK(caligo::render::EvaluateSky(0.0f).light.direction.y < -0.99f);
  CHECK(std::abs(caligo::render::EvaluateSky(6.0f).light.direction.y) < 0.02f);
  CHECK(std::abs(caligo::render::EvaluateSky(18.0f).light.direction.y) < 0.02f);
}

TEST_CASE("a luz direta acompanha a altura do sol") {
  const caligo::render::SkyState noon = caligo::render::EvaluateSky(12.0f);
  const caligo::render::SkyState midnight = caligo::render::EvaluateSky(0.0f);
  CHECK(noon.light.color.x > midnight.light.color.x);
  CHECK(noon.environment.sunIntensity > midnight.environment.sunIntensity);
}

TEST_CASE("o céu do dia é azul e o do amanhecer é quente") {
  const caligo::render::RenderEnvironment day = caligo::render::EvaluateSky(12.0f).environment;
  CHECK(day.skyZenith.z > day.skyZenith.x);
  CHECK(day.skyHorizon.z > day.skyHorizon.x);
  const caligo::render::RenderEnvironment dawn = caligo::render::EvaluateSky(6.5f).environment;
  CHECK(dawn.skyHorizon.x > dawn.skyHorizon.z);
}

TEST_CASE("a hora do dia aceita o ciclo completo") {
  const caligo::render::SkyState midnight = caligo::render::EvaluateSky(24.0f);
  const caligo::render::SkyState zero = caligo::render::EvaluateSky(0.0f);
  CHECK(midnight.light.direction.y == doctest::Approx(zero.light.direction.y));
  CHECK(midnight.environment.skyZenith.x == doctest::Approx(zero.environment.skyZenith.x));
  const caligo::render::SkyState evening = caligo::render::EvaluateSky(18.0f);
  const caligo::render::SkyState before = caligo::render::EvaluateSky(-6.0f);
  CHECK(evening.light.direction.x == doctest::Approx(before.light.direction.x));
}

TEST_CASE("os presets de céu cobrem o dia") {
  const std::span<const caligo::render::SkyPreset> presets = caligo::render::SkyPresets();
  REQUIRE(presets.size() >= 4);
  for (const caligo::render::SkyPreset& preset : presets) {
    CHECK(preset.name != nullptr);
    CHECK(preset.hours >= 0.0f);
    CHECK(preset.hours <= 24.0f);
  }
  CHECK(caligo::render::NearestSkyPreset(12.1f) < presets.size());
  CHECK(caligo::render::NearestSkyPreset(12.1f) == caligo::render::NearestSkyPreset(12.0f));
}
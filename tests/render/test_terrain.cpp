#include <windows.h>

#include <caligo/core/Math.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/SceneRenderer.h>
#include <caligo/render/TerrainMesh.h>
#include <cmath>
#include <cstdint>
#include <doctest/doctest.h>
#include <span>
#include <vector>

namespace {

constexpr uint32_t kSize = 64;
constexpr int kTolerance = 4;
constexpr caligo::math::Vec3 kFlatSky{0.020f, 0.025f, 0.035f};
constexpr caligo::math::Vec3 kFlatAmbient{0.22f, 0.22f, 0.22f};

struct Rgb {
  int red = 0;
  int green = 0;
  int blue = 0;
};

struct TerrainData {
  uint32_t resolution = 0;
  float extent = 0.0f;
  std::vector<float> heights;
  std::vector<float> weights;
  std::vector<caligo::math::Vec3> colors;
};

float Aces(float value) {
  const float numerator = value * (2.51f * value + 0.03f);
  const float denominator = value * (2.43f * value + 0.59f) + 0.14f;
  return caligo::math::Clamp(numerator / denominator, 0.0f, 1.0f);
}

float Encode(float value) {
  return std::pow(Aces(value), 1.0f / 2.2f);
}

int ToByte(float value) {
  return static_cast<int>(std::lround(static_cast<double>(value) * 255.0));
}

Rgb LitColor(const caligo::math::Vec3& base, const caligo::math::Vec3& light) {
  return Rgb{ToByte(Encode(base.x * 0.22f + base.x * light.x)),
             ToByte(Encode(base.y * 0.22f + base.y * light.y)),
             ToByte(Encode(base.z * 0.22f + base.z * light.z))};
}

Rgb Sample(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y) {
  const size_t index = (static_cast<size_t>(y) * kSize + x) * 4;
  return Rgb{pixels[index], pixels[index + 1], pixels[index + 2]};
}

void CheckColor(const Rgb& actual, const Rgb& expected) {
  CHECK(std::abs(actual.red - expected.red) <= kTolerance);
  CHECK(std::abs(actual.green - expected.green) <= kTolerance);
  CHECK(std::abs(actual.blue - expected.blue) <= kTolerance);
}

caligo::render::RenderEnvironment FlatEnvironment() {
  caligo::render::RenderEnvironment environment;
  environment.skyZenith = kFlatSky;
  environment.skyHorizon = kFlatSky;
  environment.skyGround = kFlatSky;
  environment.ambientSky = kFlatAmbient;
  environment.ambientGround = kFlatAmbient;
  environment.sunColor = caligo::math::Vec3{0.0f, 0.0f, 0.0f};
  environment.sunIntensity = 0.0f;
  environment.sunAngularRadiusDegrees = 0.0f;
  return environment;
}

caligo::render::TerrainSurface MakeSurface(const TerrainData& data) {
  caligo::render::TerrainSurface surface;
  surface.resolution = data.resolution;
  surface.extent = data.extent;
  surface.heights = data.heights;
  surface.layerWeights = data.weights;
  surface.layerColors = data.colors;
  surface.world = caligo::math::Identity();
  return surface;
}

caligo::render::RenderCamera TopDownCamera() {
  caligo::render::RenderCamera camera;
  camera.view = caligo::math::LookAt(caligo::math::Vec3{0.0f, 12.0f, 0.0f},
                                     caligo::math::Vec3{0.0f, 0.0f, 0.0f},
                                     caligo::math::Vec3{0.0f, 0.0f, 1.0f});
  camera.projection = caligo::math::Perspective(55.0f, 1.0f, 0.1f, 100.0f);
  return camera;
}

TerrainData FlatTerrain(uint32_t resolution, float extent, uint32_t layers) {
  TerrainData data;
  data.resolution = resolution;
  data.extent = extent;
  data.heights.assign(static_cast<size_t>(resolution) * resolution, 0.0f);
  data.weights.assign(static_cast<size_t>(resolution) * resolution * layers, 0.0f);
  for (size_t sample = 0; sample < data.heights.size(); ++sample) {
    data.weights[sample * layers] = 1.0f;
  }
  return data;
}

caligo::render::RenderLight WhiteLight(const caligo::math::Vec3& direction) {
  caligo::render::RenderLight light;
  light.direction = direction;
  light.color = caligo::math::Vec3{1.0f, 1.0f, 1.0f};
  return light;
}

}

TEST_CASE("o terreno desenha a cor da camada") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  TerrainData data = FlatTerrain(5, 20.0f, 2);
  data.colors = {caligo::math::Vec3{0.55f, 0.28f, 0.14f}, caligo::math::Vec3{0.18f, 0.32f, 0.65f}};
  const caligo::render::TerrainSurface surface = MakeSurface(data);
  const caligo::render::RenderLight light = WhiteLight(caligo::math::Vec3{0.0f, 1.0f, 0.0f});

  caligo::render::SceneRenderer renderer;
  REQUIRE(renderer
              .Render(*device.Value(), std::span<const caligo::render::RenderItem>{},
                      TopDownCamera(), light, kSize, kSize, FlatEnvironment(),
                      std::span<const caligo::render::TerrainSurface>(&surface, 1))
              .HasValue());
  const caligo::Result<std::vector<uint8_t>> frame = renderer.ReadColor();
  REQUIRE(frame.HasValue());

  CheckColor(Sample(frame.Value(), 32, 32), LitColor(data.colors[0], light.color));
  CheckColor(Sample(frame.Value(), 20, 44), LitColor(data.colors[0], light.color));
}

TEST_CASE("o terreno mistura as camadas pintadas") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  TerrainData data = FlatTerrain(5, 20.0f, 2);
  data.colors = {caligo::math::Vec3{0.55f, 0.28f, 0.14f}, caligo::math::Vec3{0.18f, 0.32f, 0.65f}};
  const caligo::render::RenderLight light = WhiteLight(caligo::math::Vec3{0.0f, 1.0f, 0.0f});

  caligo::render::SceneRenderer renderer;
  const caligo::render::TerrainSurface first = MakeSurface(data);
  REQUIRE(renderer
              .Render(*device.Value(), std::span<const caligo::render::RenderItem>{},
                      TopDownCamera(), light, kSize, kSize, FlatEnvironment(),
                      std::span<const caligo::render::TerrainSurface>(&first, 1))
              .HasValue());
  const caligo::Result<std::vector<uint8_t>> firstFrame = renderer.ReadColor();
  REQUIRE(firstFrame.HasValue());

  for (size_t sample = 0; sample < data.heights.size(); ++sample) {
    data.weights[sample * 2] = 0.0f;
    data.weights[sample * 2 + 1] = 1.0f;
  }
  const caligo::render::TerrainSurface second = MakeSurface(data);
  REQUIRE(renderer
              .Render(*device.Value(), std::span<const caligo::render::RenderItem>{},
                      TopDownCamera(), light, kSize, kSize, FlatEnvironment(),
                      std::span<const caligo::render::TerrainSurface>(&second, 1))
              .HasValue());
  const caligo::Result<std::vector<uint8_t>> secondFrame = renderer.ReadColor();
  REQUIRE(secondFrame.HasValue());

  const Rgb before = Sample(firstFrame.Value(), 32, 32);
  const Rgb after = Sample(secondFrame.Value(), 32, 32);
  CHECK(after.blue > before.blue + 20);
  CHECK(after.red < before.red);
  CheckColor(after, LitColor(data.colors[1], light.color));
}

TEST_CASE("o relevo varia o sombreamento do terreno") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  TerrainData data = FlatTerrain(9, 20.0f, 1);
  data.colors = {caligo::math::Vec3{0.45f, 0.45f, 0.45f}};
  for (uint32_t z = 0; z < data.resolution; ++z) {
    for (uint32_t x = 0; x < data.resolution; ++x) {
      const float wave =
          std::sin(3.14159265f * static_cast<float>(x) / static_cast<float>(data.resolution - 1));
      data.heights[static_cast<size_t>(z) * data.resolution + x] = wave * 4.0f;
    }
  }
  const caligo::render::TerrainSurface surface = MakeSurface(data);
  const caligo::render::RenderLight light =
      WhiteLight(caligo::math::Normalize(caligo::math::Vec3{0.7f, 0.7f, 0.0f}));

  caligo::render::SceneRenderer renderer;
  REQUIRE(renderer
              .Render(*device.Value(), std::span<const caligo::render::RenderItem>{},
                      TopDownCamera(), light, kSize, kSize, FlatEnvironment(),
                      std::span<const caligo::render::TerrainSurface>(&surface, 1))
              .HasValue());
  const caligo::Result<std::vector<uint8_t>> frame = renderer.ReadColor();
  REQUIRE(frame.HasValue());

  int minimum = 255;
  int maximum = 0;
  for (uint32_t x = 4; x < kSize - 4; ++x) {
    const Rgb value = Sample(frame.Value(), x, 32);
    minimum = std::min(minimum, value.red);
    maximum = std::max(maximum, value.red);
  }
  CHECK(maximum - minimum > 10);
}

TEST_CASE("o terreno recebe a sombra do objeto") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  TerrainData data = FlatTerrain(17, 24.0f, 1);
  data.colors = {caligo::math::Vec3{0.6f, 0.6f, 0.6f}};
  const caligo::render::TerrainSurface surface = MakeSurface(data);
  const caligo::render::RenderLight light =
      WhiteLight(caligo::math::Normalize(caligo::math::Vec3{0.35f, 1.0f, 0.0f}));

  caligo::render::RenderCamera camera;
  camera.view = caligo::math::LookAt(caligo::math::Vec3{0.0f, 6.0f, 9.0f},
                                     caligo::math::Vec3{0.0f, 0.0f, 0.0f},
                                     caligo::math::Vec3{0.0f, 1.0f, 0.0f});
  camera.projection = caligo::math::Perspective(55.0f, 1.0f, 0.1f, 100.0f);

  caligo::render::SceneRenderer renderer;
  REQUIRE(renderer
              .Render(*device.Value(), std::span<const caligo::render::RenderItem>{}, camera, light,
                      kSize, kSize, caligo::render::RenderEnvironment{},
                      std::span<const caligo::render::TerrainSurface>(&surface, 1))
              .HasValue());
  const caligo::Result<std::vector<uint8_t>> litFrame = renderer.ReadColor();
  REQUIRE(litFrame.HasValue());

  std::vector<caligo::render::RenderItem> items(1);
  items[0].world = caligo::math::ComposeTransform(caligo::math::Vec3{0.0f, 3.0f, 0.0f},
                                                  caligo::math::IdentityRotation(),
                                                  caligo::math::Vec3{3.0f, 1.0f, 3.0f});
  items[0].color = caligo::math::Vec3{0.5f, 0.30f, 0.30f};
  REQUIRE(renderer
              .Render(*device.Value(), items, camera, light, kSize, kSize,
                      caligo::render::RenderEnvironment{},
                      std::span<const caligo::render::TerrainSurface>(&surface, 1))
              .HasValue());
  const caligo::Result<std::vector<uint8_t>> shadowFrame = renderer.ReadColor();
  REQUIRE(shadowFrame.HasValue());
  REQUIRE(litFrame.Value().size() == shadowFrame.Value().size());

  int darkened = 0;
  for (size_t offset = 0; offset + 2 < litFrame.Value().size(); offset += 4) {
    const int litLuminance =
        litFrame.Value()[offset] + litFrame.Value()[offset + 1] + litFrame.Value()[offset + 2];
    const int shadowLuminance = shadowFrame.Value()[offset] + shadowFrame.Value()[offset + 1] +
                                shadowFrame.Value()[offset + 2];
    if (litLuminance - shadowLuminance >= 60) {
      ++darkened;
    }
  }
  CHECK(darkened > 40);
}

TEST_CASE("a malha do terreno gera a grade de triângulos") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  TerrainData data = FlatTerrain(5, 20.0f, 1);
  data.colors = {caligo::math::Vec3{0.4f, 0.5f, 0.3f}};
  const caligo::render::TerrainSurface surface = MakeSurface(data);

  caligo::render::TerrainMesh mesh;
  REQUIRE(mesh.Update(*device.Value(), surface).HasValue());
  CHECK(mesh.HasSurface());
  CHECK(mesh.Geometry().indexCount == 4u * 4u * 6u);
  CHECK(mesh.Geometry().boundsMinimum.y == doctest::Approx(0.0f));
  CHECK(mesh.Geometry().boundsMinimum.x == doctest::Approx(-10.0f));
  CHECK(mesh.Geometry().boundsMaximum.x == doctest::Approx(10.0f));

  caligo::render::TerrainSurface invalid;
  invalid.resolution = 1;
  REQUIRE(mesh.Update(*device.Value(), invalid).HasValue());
  CHECK_FALSE(mesh.HasSurface());
}

#include <windows.h>

#include <algorithm>
#include <caligo/core/Math.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/SceneRenderer.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <span>
#include <vector>

namespace {

constexpr uint32_t kSize = 64;
constexpr uint32_t kSmallSize = 32;
constexpr int kTolerance = 4;

struct Rgb {
  int red = 0;
  int green = 0;
  int blue = 0;
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

Rgb BackgroundColor() {
  return Rgb{ToByte(Encode(0.020f)), ToByte(Encode(0.025f)), ToByte(Encode(0.035f))};
}

Rgb Sample(const std::vector<uint8_t>& pixels, uint32_t size, uint32_t x, uint32_t y) {
  const size_t index = (static_cast<size_t>(y) * size + x) * 4;
  return Rgb{pixels[index], pixels[index + 1], pixels[index + 2]};
}

void CheckColor(const Rgb& actual, const Rgb& expected) {
  CHECK(std::abs(actual.red - expected.red) <= kTolerance);
  CHECK(std::abs(actual.green - expected.green) <= kTolerance);
  CHECK(std::abs(actual.blue - expected.blue) <= kTolerance);
}

std::vector<caligo::render::RenderItem> BuildScene(const caligo::math::Vec3& frontColor,
                                                   const caligo::math::Vec3& backColor) {
  std::vector<caligo::render::RenderItem> items(2);
  items[0].world = caligo::math::Identity();
  items[0].color = frontColor;
  items[1].world = caligo::math::ComposeTransform(caligo::math::Vec3{0.0f, 0.0f, -2.0f},
                                                  caligo::math::IdentityRotation(),
                                                  caligo::math::Vec3{1.0f, 1.0f, 1.0f});
  items[1].color = backColor;
  return items;
}

std::vector<uint8_t> RunCameraSequence(caligo::render::RhiDevice& device,
                                       const std::vector<caligo::render::RenderItem>& items) {
  caligo::render::RenderCamera camera;
  camera.projection = caligo::math::Perspective(55.0f, 1.0f, 0.1f, 100.0f);

  caligo::render::RenderLight light;
  light.direction = caligo::math::Vec3{0.0f, 0.0f, 1.0f};
  light.color = caligo::math::Vec3{1.0f, 0.98f, 0.94f};

  caligo::render::SceneRenderer renderer;
  std::vector<uint8_t> last;
  for (uint32_t frame = 0; frame < 60; ++frame) {
    const float offset = std::sin(static_cast<float>(frame) * 0.05f) * 0.3f;
    camera.view = caligo::math::LookAt(caligo::math::Vec3{offset, 0.0f, 4.0f},
                                       caligo::math::Vec3{0.0f, 0.0f, 0.0f},
                                       caligo::math::Vec3{0.0f, 1.0f, 0.0f});
    const caligo::Result<void> rendered =
        renderer.Render(device, items, camera, light, kSize, kSize);
    if (!rendered) {
      return {};
    }
    const caligo::Result<std::vector<uint8_t>> pixels = renderer.ReadColor();
    if (!pixels) {
      return {};
    }
    last = pixels.Value();
  }
  return last;
}

}

TEST_CASE("o renderizador de cena desenha cubos sólidos e iluminados") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());
  REQUIRE(device.Value()->IsWarp());

  const caligo::math::Vec3 frontColor{0.72f, 0.75f, 0.80f};
  const caligo::math::Vec3 backColor{1.0f, 0.10f, 0.10f};
  const std::vector<caligo::render::RenderItem> items = BuildScene(frontColor, backColor);

  caligo::render::RenderCamera camera;
  camera.view = caligo::math::LookAt(caligo::math::Vec3{0.0f, 0.0f, 4.0f},
                                     caligo::math::Vec3{0.0f, 0.0f, 0.0f},
                                     caligo::math::Vec3{0.0f, 1.0f, 0.0f});
  camera.projection = caligo::math::Perspective(55.0f, 1.0f, 0.1f, 100.0f);

  caligo::render::RenderLight light;
  light.direction = caligo::math::Vec3{0.0f, 0.0f, 1.0f};
  light.color = caligo::math::Vec3{1.0f, 0.98f, 0.94f};

  caligo::render::SceneRenderer renderer;
  const caligo::Result<void> rendered =
      renderer.Render(*device.Value(), items, camera, light, kSize, kSize);
  REQUIRE(rendered.HasValue());
  CHECK(renderer.Width() == kSize);
  CHECK(renderer.Height() == kSize);
  REQUIRE(renderer.ColorView() != nullptr);

  const caligo::Result<std::vector<uint8_t>> frame = renderer.ReadColor();
  REQUIRE(frame.HasValue());
  REQUIRE(frame.Value().size() == static_cast<size_t>(kSize) * kSize * 4);

  const Rgb lit = LitColor(frontColor, light.color);
  const Rgb background = BackgroundColor();
  CheckColor(Sample(frame.Value(), kSize, 28, 28), lit);
  CheckColor(Sample(frame.Value(), kSize, 35, 35), lit);
  CheckColor(Sample(frame.Value(), kSize, 3, 3), background);
  CheckColor(Sample(frame.Value(), kSize, 60, 3), background);
}

TEST_CASE("o renderizador de cena respeita a profundidade e é determinístico") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  const caligo::math::Vec3 frontColor{0.72f, 0.75f, 0.80f};
  const caligo::math::Vec3 backColor{1.0f, 0.10f, 0.10f};
  const std::vector<caligo::render::RenderItem> items = BuildScene(frontColor, backColor);

  caligo::render::RenderCamera camera;
  camera.view = caligo::math::LookAt(caligo::math::Vec3{0.0f, 0.0f, 4.0f},
                                     caligo::math::Vec3{0.0f, 0.0f, 0.0f},
                                     caligo::math::Vec3{0.0f, 1.0f, 0.0f});
  camera.projection = caligo::math::Perspective(55.0f, 1.0f, 0.1f, 100.0f);

  caligo::render::RenderLight light;
  light.direction = caligo::math::Vec3{0.0f, 0.0f, 1.0f};
  light.color = caligo::math::Vec3{1.0f, 0.98f, 0.94f};

  caligo::render::SceneRenderer firstRenderer;
  REQUIRE(firstRenderer.Render(*device.Value(), items, camera, light, kSize, kSize).HasValue());
  const caligo::Result<std::vector<uint8_t>> first = firstRenderer.ReadColor();
  REQUIRE(first.HasValue());

  const Rgb occluded = Sample(first.Value(), kSize, 28, 28);
  CheckColor(occluded, LitColor(frontColor, light.color));

  caligo::render::SceneRenderer secondRenderer;
  REQUIRE(secondRenderer.Render(*device.Value(), items, camera, light, kSize, kSize).HasValue());
  const caligo::Result<std::vector<uint8_t>> second = secondRenderer.ReadColor();
  REQUIRE(second.HasValue());
  REQUIRE(first.Value().size() == second.Value().size());
  CHECK(first.Value() == second.Value());

  REQUIRE(firstRenderer.Render(*device.Value(), items, camera, light, kSmallSize, kSmallSize)
              .HasValue());
  CHECK(firstRenderer.Width() == kSmallSize);
  CHECK(firstRenderer.Height() == kSmallSize);
  const caligo::Result<std::vector<uint8_t>> resized = firstRenderer.ReadColor();
  REQUIRE(resized.HasValue());
  REQUIRE(resized.Value().size() == static_cast<size_t>(kSmallSize) * kSmallSize * 4);
  CheckColor(Sample(resized.Value(), kSmallSize, 14, 14), LitColor(frontColor, light.color));
}

TEST_CASE("o renderizador de cena projeta sombra direcional no chão") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  caligo::render::RenderCamera camera;
  camera.view = caligo::math::LookAt(caligo::math::Vec3{0.0f, 6.0f, 9.0f},
                                     caligo::math::Vec3{0.0f, 0.0f, 0.0f},
                                     caligo::math::Vec3{0.0f, 1.0f, 0.0f});
  camera.projection = caligo::math::Perspective(55.0f, 1.0f, 0.1f, 100.0f);

  caligo::render::RenderLight light;
  light.direction = caligo::math::Normalize(caligo::math::Vec3{0.35f, 1.0f, 0.0f});
  light.color = caligo::math::Vec3{1.0f, 0.98f, 0.94f};

  const caligo::math::Mat4 floorWorld = caligo::math::ComposeTransform(
      caligo::math::Vec3{0.0f, 0.0f, 0.0f}, caligo::math::IdentityRotation(),
      caligo::math::Vec3{8.0f, 0.2f, 8.0f});

  std::vector<caligo::render::RenderItem> lit(1);
  lit[0].world = floorWorld;
  lit[0].color = caligo::math::Vec3{0.6f, 0.6f, 0.6f};

  std::vector<caligo::render::RenderItem> occluded = lit;
  caligo::render::RenderItem occluder;
  occluder.world = caligo::math::ComposeTransform(caligo::math::Vec3{0.0f, 3.0f, 0.0f},
                                                  caligo::math::IdentityRotation(),
                                                  caligo::math::Vec3{1.5f, 1.0f, 1.5f});
  occluder.color = caligo::math::Vec3{0.5f, 0.30f, 0.30f};
  occluded.push_back(occluder);

  caligo::render::SceneRenderer renderer;
  REQUIRE(renderer.Render(*device.Value(), lit, camera, light, kSize, kSize).HasValue());
  const caligo::Result<std::vector<uint8_t>> litFrame = renderer.ReadColor();
  REQUIRE(litFrame.HasValue());

  REQUIRE(renderer.Render(*device.Value(), occluded, camera, light, kSize, kSize).HasValue());
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

TEST_CASE("o pos-processamento espalha o brilho de objetos intensos") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  caligo::render::RenderCamera camera;
  camera.view = caligo::math::LookAt(caligo::math::Vec3{0.0f, 0.0f, 4.0f},
                                     caligo::math::Vec3{0.0f, 0.0f, 0.0f},
                                     caligo::math::Vec3{0.0f, 1.0f, 0.0f});
  camera.projection = caligo::math::Perspective(55.0f, 1.0f, 0.1f, 100.0f);

  caligo::render::RenderLight light;
  light.direction = caligo::math::Vec3{0.0f, 0.0f, 1.0f};
  light.color = caligo::math::Vec3{1.0f, 0.98f, 0.94f};

  caligo::render::SceneRenderer renderer;
  REQUIRE(renderer
              .Render(*device.Value(), std::span<const caligo::render::RenderItem>{}, camera, light,
                      kSize, kSize)
              .HasValue());
  const caligo::Result<std::vector<uint8_t>> backgroundFrame = renderer.ReadColor();
  REQUIRE(backgroundFrame.HasValue());

  std::vector<caligo::render::RenderItem> bright(1);
  bright[0].world = caligo::math::Identity();
  bright[0].color = caligo::math::Vec3{20.0f, 20.0f, 20.0f};
  REQUIRE(renderer.Render(*device.Value(), bright, camera, light, kSize, kSize).HasValue());
  const caligo::Result<std::vector<uint8_t>> bloomFrame = renderer.ReadColor();
  REQUIRE(bloomFrame.HasValue());

  const Rgb backgroundPixel = Sample(backgroundFrame.Value(), kSize, 32, 18);
  const Rgb bloomPixel = Sample(bloomFrame.Value(), kSize, 32, 18);
  const int backgroundLuminance =
      backgroundPixel.red + backgroundPixel.green + backgroundPixel.blue;
  const int bloomLuminance = bloomPixel.red + bloomPixel.green + bloomPixel.blue;
  CHECK(backgroundLuminance < 300);
  CHECK(bloomLuminance > backgroundLuminance + 30);
}

TEST_CASE("o TAA mantém a imagem estável por 300 quadros") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  const std::vector<caligo::render::RenderItem> items =
      BuildScene(caligo::math::Vec3{0.72f, 0.75f, 0.80f}, caligo::math::Vec3{1.0f, 0.10f, 0.10f});

  caligo::render::RenderCamera camera;
  camera.view = caligo::math::LookAt(caligo::math::Vec3{0.0f, 0.0f, 4.0f},
                                     caligo::math::Vec3{0.0f, 0.0f, 0.0f},
                                     caligo::math::Vec3{0.0f, 1.0f, 0.0f});
  camera.projection = caligo::math::Perspective(55.0f, 1.0f, 0.1f, 100.0f);

  caligo::render::RenderLight light;
  light.direction = caligo::math::Vec3{0.0f, 0.0f, 1.0f};
  light.color = caligo::math::Vec3{1.0f, 0.98f, 0.94f};

  caligo::render::SceneRenderer renderer;
  std::vector<uint8_t> previous;
  int64_t totalDifference = 0;
  int64_t samples = 0;
  int maximumDifference = 0;
  for (uint32_t frame = 0; frame < 300; ++frame) {
    REQUIRE(renderer.Render(*device.Value(), items, camera, light, kSize, kSize).HasValue());
    if (frame < 289) {
      continue;
    }
    const caligo::Result<std::vector<uint8_t>> pixels = renderer.ReadColor();
    REQUIRE(pixels.HasValue());
    if (!previous.empty()) {
      for (size_t index = 0; index < previous.size(); ++index) {
        const int difference =
            std::abs(static_cast<int>(pixels.Value()[index]) - static_cast<int>(previous[index]));
        maximumDifference = std::max(maximumDifference, difference);
        totalDifference += difference;
        ++samples;
      }
    }
    previous = pixels.Value();
  }
  REQUIRE(samples > 0);
  CHECK(static_cast<double>(totalDifference) / static_cast<double>(samples) < 4.0);
  CHECK(maximumDifference < 100);
}

TEST_CASE("o TAA é determinístico com a câmera em movimento") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());

  const std::vector<caligo::render::RenderItem> items =
      BuildScene(caligo::math::Vec3{0.72f, 0.75f, 0.80f}, caligo::math::Vec3{1.0f, 0.10f, 0.10f});

  const std::vector<uint8_t> first = RunCameraSequence(*device.Value(), items);
  const std::vector<uint8_t> second = RunCameraSequence(*device.Value(), items);
  REQUIRE(!first.empty());
  REQUIRE(first.size() == second.size());
  CHECK(first == second);
}

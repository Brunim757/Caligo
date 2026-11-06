#include <algorithm>
#include <caligo/render/Sky.h>
#include <cmath>
#include <iterator>

namespace caligo::render {

namespace {

struct SkyKey {
  float elevation;
  math::Vec3 zenith;
  math::Vec3 horizon;
  math::Vec3 ground;
  math::Vec3 ambientSky;
  math::Vec3 ambientGround;
  math::Vec3 sunColor;
  math::Vec3 directColor;
  float sunIntensity;
};

constexpr SkyKey kKeys[] = {
    {-1.00f,
     {0.004f, 0.006f, 0.014f},
     {0.010f, 0.014f, 0.028f},
     {0.002f, 0.002f, 0.004f},
     {0.012f, 0.015f, 0.026f},
     {0.003f, 0.003f, 0.005f},
     {0.62f, 0.72f, 1.00f},
     {0.020f, 0.030f, 0.070f},
     1.20f},
    {-0.20f,
     {0.010f, 0.014f, 0.034f},
     {0.040f, 0.048f, 0.090f},
     {0.004f, 0.004f, 0.008f},
     {0.030f, 0.038f, 0.060f},
     {0.008f, 0.008f, 0.012f},
     {0.85f, 0.82f, 0.95f},
     {0.050f, 0.060f, 0.120f},
     2.00f},
    {-0.04f,
     {0.060f, 0.075f, 0.160f},
     {0.560f, 0.300f, 0.150f},
     {0.020f, 0.018f, 0.020f},
     {0.120f, 0.105f, 0.140f},
     {0.030f, 0.026f, 0.028f},
     {1.00f, 0.58f, 0.30f},
     {0.320f, 0.190f, 0.100f},
     4.00f},
    {0.12f,
     {0.090f, 0.140f, 0.290f},
     {0.720f, 0.520f, 0.330f},
     {0.035f, 0.032f, 0.030f},
     {0.190f, 0.190f, 0.210f},
     {0.050f, 0.045f, 0.042f},
     {1.00f, 0.78f, 0.55f},
     {0.850f, 0.620f, 0.380f},
     8.00f},
    {0.45f,
     {0.070f, 0.130f, 0.280f},
     {0.460f, 0.560f, 0.700f},
     {0.045f, 0.045f, 0.055f},
     {0.210f, 0.240f, 0.300f},
     {0.055f, 0.055f, 0.065f},
     {1.00f, 0.97f, 0.90f},
     {1.000f, 0.960f, 0.880f},
     14.00f},
    {1.00f,
     {0.060f, 0.120f, 0.270f},
     {0.430f, 0.540f, 0.690f},
     {0.045f, 0.045f, 0.055f},
     {0.220f, 0.250f, 0.320f},
     {0.060f, 0.060f, 0.070f},
     {1.00f, 0.98f, 0.92f},
     {1.000f, 0.970f, 0.900f},
     15.00f},
};

constexpr SkyPreset kPresets[] = {
    {"Amanhecer", 6.5f}, {"Manhã", 9.0f},       {"Meio-dia", 12.0f},
    {"Tarde", 15.5f},    {"Entardecer", 18.0f}, {"Noite", 0.0f},
};

math::Vec3 Mix(const math::Vec3& from, const math::Vec3& to, float factor) noexcept {
  return math::Vec3{math::Lerp(from.x, to.x, factor), math::Lerp(from.y, to.y, factor),
                    math::Lerp(from.z, to.z, factor)};
}

float NormalizeHours(float hours) noexcept {
  const float wrapped = std::fmod(hours, 24.0f);
  return wrapped < 0.0f ? wrapped + 24.0f : wrapped;
}

}

SkyState EvaluateSky(float hours) noexcept {
  const float normalized = NormalizeHours(hours);
  const float theta = normalized / 24.0f * (2.0f * math::kPi) - math::kPi * 0.5f;
  const float elevation = std::sin(theta);

  std::size_t index = 0;
  while (index + 2 < std::size(kKeys) && elevation > kKeys[index + 1].elevation) {
    ++index;
  }
  const SkyKey& from = kKeys[index];
  const SkyKey& to = kKeys[index + 1];
  const float span = to.elevation - from.elevation;
  const float factor =
      span > math::kEpsilon ? math::Clamp((elevation - from.elevation) / span, 0.0f, 1.0f) : 0.0f;

  SkyState state;
  state.environment.skyZenith = Mix(from.zenith, to.zenith, factor);
  state.environment.skyHorizon = Mix(from.horizon, to.horizon, factor);
  state.environment.skyGround = Mix(from.ground, to.ground, factor);
  state.environment.ambientSky = Mix(from.ambientSky, to.ambientSky, factor);
  state.environment.ambientGround = Mix(from.ambientGround, to.ambientGround, factor);
  state.environment.sunColor = Mix(from.sunColor, to.sunColor, factor);
  state.environment.sunIntensity = math::Lerp(from.sunIntensity, to.sunIntensity, factor);

  const float horizontal = std::cos(theta);
  state.light.direction =
      math::Normalize(math::Vec3{horizontal * 0.94f, std::sin(theta), horizontal * 0.34f});
  state.light.color = Mix(from.directColor, to.directColor, factor);
  return state;
}

std::span<const SkyPreset> SkyPresets() noexcept {
  return kPresets;
}

std::size_t NearestSkyPreset(float hours) noexcept {
  const float normalized = NormalizeHours(hours);
  std::size_t nearest = 0;
  float best = 24.0f;
  for (std::size_t index = 0; index < std::size(kPresets); ++index) {
    float difference = std::abs(normalized - kPresets[index].hours);
    difference = std::min(difference, 24.0f - difference);
    if (difference < best) {
      best = difference;
      nearest = index;
    }
  }
  return nearest;
}

}
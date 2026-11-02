#include <algorithm>
#include <caligo/render/ImageDiff.h>

namespace caligo::render {

ImageDiffResult CompareImages(std::span<const uint8_t> expected, std::span<const uint8_t> actual,
                              uint32_t tolerance) noexcept {
  ImageDiffResult result;
  const size_t channels = std::min(expected.size(), actual.size());
  result.totalPixels = static_cast<uint64_t>(channels / 4);

  if (expected.size() != actual.size() || channels == 0) {
    result.maxChannelDelta = 255;
    result.mismatchedPixels = result.totalPixels;
    return result;
  }

  for (uint64_t pixel = 0; pixel < result.totalPixels; ++pixel) {
    bool mismatched = false;
    for (size_t channel = 0; channel < 4; ++channel) {
      const size_t index = static_cast<size_t>(pixel) * 4 + channel;
      const uint32_t delta = static_cast<uint32_t>(
          std::abs(static_cast<int>(expected[index]) - static_cast<int>(actual[index])));
      result.maxChannelDelta = std::max(result.maxChannelDelta, delta);
      if (delta > tolerance) {
        mismatched = true;
      }
    }
    if (mismatched) {
      ++result.mismatchedPixels;
    }
  }
  return result;
}

}

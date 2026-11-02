#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace caligo::render {

struct ImageDiffResult {
  uint32_t maxChannelDelta = 0;
  uint64_t mismatchedPixels = 0;
  uint64_t totalPixels = 0;
};

[[nodiscard]] ImageDiffResult CompareImages(std::span<const uint8_t> expected,
                                            std::span<const uint8_t> actual,
                                            uint32_t tolerance) noexcept;

}

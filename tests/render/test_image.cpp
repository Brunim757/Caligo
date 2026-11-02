#include <caligo/render/ImageDiff.h>
#include <cstdint>
#include <doctest/doctest.h>
#include <vector>

TEST_CASE("imagens idênticas não têm diferença") {
  const std::vector<uint8_t> pixels{0, 1, 2, 3, 4, 5, 6, 7};
  const caligo::render::ImageDiffResult result = caligo::render::CompareImages(pixels, pixels, 0);
  CHECK(result.maxChannelDelta == 0);
  CHECK(result.mismatchedPixels == 0);
  CHECK(result.totalPixels == 2);
}

TEST_CASE("diferença dentro da tolerância é aceita") {
  const std::vector<uint8_t> expected{10, 20, 30, 40};
  const std::vector<uint8_t> actual{12, 18, 30, 42};
  const caligo::render::ImageDiffResult result = caligo::render::CompareImages(expected, actual, 2);
  CHECK(result.maxChannelDelta == 2);
  CHECK(result.mismatchedPixels == 0);
  CHECK(result.totalPixels == 1);
}

TEST_CASE("diferença acima da tolerância é contada") {
  const std::vector<uint8_t> expected{10, 20, 30, 40};
  const std::vector<uint8_t> actual{13, 20, 30, 40};
  const caligo::render::ImageDiffResult result = caligo::render::CompareImages(expected, actual, 2);
  CHECK(result.maxChannelDelta == 3);
  CHECK(result.mismatchedPixels == 1);
}

TEST_CASE("tamanhos diferentes não podem coincidir") {
  const std::vector<uint8_t> expected{0, 0, 0, 0};
  const std::vector<uint8_t> actual{0, 0, 0, 0, 0, 0, 0, 0};
  const caligo::render::ImageDiffResult result =
      caligo::render::CompareImages(expected, actual, 255);
  CHECK(result.mismatchedPixels == result.totalPixels);
}

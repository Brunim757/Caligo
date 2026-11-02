#include <windows.h>

#include <caligo/platform/File.h>
#include <caligo/render/ImageDiff.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiShader.h>
#include <caligo/render/RhiTexture.h>
#include <d3d11.h>
#include <doctest/doctest.h>
#include <filesystem>
#include <span>
#include <vector>

namespace {

constexpr uint32_t kImageSize = 64;
constexpr uint32_t kTolerance = 2;

constexpr char kGradientShader[] = R"(
struct GradientOutput {
  float4 position : SV_POSITION;
};

GradientOutput VsMain(uint vertexId : SV_VertexID) {
  float2 corner;
  if (vertexId == 0u) {
    corner = float2(-1.0, -1.0);
  } else if (vertexId == 1u) {
    corner = float2(-1.0, 3.0);
  } else {
    corner = float2(3.0, -1.0);
  }
  GradientOutput output;
  output.position = float4(corner, 0.0, 1.0);
  return output;
}

float4 PsMain(GradientOutput input) : SV_TARGET {
  float2 uv = input.position.xy * 0.015625;
  return float4(uv.x, uv.y, 0.25, 1.0);
}
)";

}

TEST_CASE("o RHI desenha um quadro fora da tela em WARP") {
  auto device = caligo::render::RhiDevice::Create(true);
  REQUIRE(device.HasValue());
  CHECK(device.Value()->IsWarp());

  const caligo::render::RhiTextureDesc targetDesc{
      kImageSize, kImageSize, caligo::render::RhiFormat::Rgba8Unorm, true, false};
  auto target = caligo::render::RhiTexture::Create(*device.Value(), targetDesc);
  REQUIRE(target.HasValue());

  const caligo::render::RhiShaderDesc shaderDesc{kGradientShader};
  auto shader = caligo::render::RhiShader::Create(*device.Value(), shaderDesc);
  REQUIRE(shader.HasValue());

  ID3D11DeviceContext* context = static_cast<ID3D11DeviceContext*>(device.Value()->ContextHandle());
  ID3D11RenderTargetView* targetView =
      static_cast<ID3D11RenderTargetView*>(target.Value()->RenderTargetView());
  REQUIRE(context != nullptr);
  REQUIRE(targetView != nullptr);

  const float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  context->ClearRenderTargetView(targetView, clear);
  ID3D11RenderTargetView* targets[] = {targetView};
  context->OMSetRenderTargets(1, targets, nullptr);

  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(kImageSize);
  viewport.Height = static_cast<float>(kImageSize);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  context->RSSetViewports(1, &viewport);

  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->IASetInputLayout(nullptr);
  context->VSSetShader(static_cast<ID3D11VertexShader*>(shader.Value()->VertexShaderHandle()),
                       nullptr, 0);
  context->PSSetShader(static_cast<ID3D11PixelShader*>(shader.Value()->PixelShaderHandle()),
                       nullptr, 0);
  context->Draw(3, 0);
  context->OMSetRenderTargets(0, nullptr, nullptr);
  context->Flush();

  const caligo::Result<std::vector<uint8_t>> pixels = target.Value()->ReadPixels();
  REQUIRE(pixels.HasValue());
  REQUIRE(pixels.Value().size() == static_cast<size_t>(kImageSize) * kImageSize * 4);

  const std::filesystem::path goldenPath =
      std::filesystem::path(CALIGO_GOLDEN_DIR) / "gradient.rgba8";
  const caligo::Result<caligo::platform::ByteBuffer> golden =
      caligo::platform::ReadBinaryFile(goldenPath);
  REQUIRE(golden.HasValue());
  REQUIRE(golden.Value().size() == pixels.Value().size());

  const std::span<const uint8_t> expected(reinterpret_cast<const uint8_t*>(golden.Value().data()),
                                          golden.Value().size());
  const caligo::render::ImageDiffResult diff =
      caligo::render::CompareImages(expected, pixels.Value(), kTolerance);
  CHECK(diff.totalPixels == static_cast<uint64_t>(kImageSize) * kImageSize);
  CHECK(diff.maxChannelDelta <= kTolerance);
  CHECK(diff.mismatchedPixels == 0);
}

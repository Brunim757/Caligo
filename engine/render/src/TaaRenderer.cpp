#include <windows.h>

#include "FullscreenQuad.h"
#include "RenderPassUtil.h"
#include <caligo/render/RhiBuffer.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiSampler.h>
#include <caligo/render/RhiTexture.h>
#include <caligo/render/TaaRenderer.h>
#include <d3d11.h>
#include <iterator>
#include <utility>

namespace caligo::render {

namespace {

constexpr float kBlendFactor = 0.1f;

constexpr char kTaaShader[] = R"(
cbuffer TaaConstants : register(b0) {
  row_major float4x4 inverseViewProjection;
  row_major float4x4 previousViewProjection;
  float2 texelSize;
  float blendFactor;
  float hasHistory;
};

Texture2D currentColor : register(t0);
Texture2D<float> currentDepth : register(t1);
Texture2D historyColor : register(t2);
SamplerState linearSampler : register(s0);

struct PostInput {
  float2 position : POSITION;
  float2 uv : TEXCOORD0;
};

struct PostOutput {
  float4 position : SV_POSITION;
  float2 uv : TEXCOORD0;
};

PostOutput VsMain(PostInput input) {
  PostOutput output;
  output.position = float4(input.position, 0.0, 1.0);
  output.uv = input.uv;
  return output;
}

float3 NeighborhoodClamp(float2 uv, float3 history) {
  float3 minimum = currentColor.Sample(linearSampler, uv).rgb;
  float3 maximum = minimum;
  [unroll]
  for (int row = -1; row <= 1; ++row) {
    [unroll]
    for (int column = -1; column <= 1; ++column) {
      float3 sample = currentColor.Sample(linearSampler, uv + float2(column, row) * texelSize).rgb;
      minimum = min(minimum, sample);
      maximum = max(maximum, sample);
    }
  }
  return clamp(history, minimum, maximum);
}

float4 PsMain(PostOutput input) : SV_TARGET {
  float2 uv = input.uv;
  float3 current = currentColor.Sample(linearSampler, uv).rgb;
  if (hasHistory < 0.5) {
    return float4(current, 1.0);
  }

  int2 pixel = int2(input.position.xy);
  float depth = currentDepth.Load(int3(pixel, 0));
  float4 clip = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
  float4 world = mul(clip, inverseViewProjection);
  if (abs(world.w) < 1.0e-6) {
    return float4(current, 1.0);
  }
  world /= world.w;
  float4 previousClip = mul(world, previousViewProjection);
  if (previousClip.w <= 0.0) {
    return float4(current, 1.0);
  }
  float2 previousUv = float2(previousClip.x / previousClip.w * 0.5 + 0.5,
                             0.5 - previousClip.y / previousClip.w * 0.5);
  if (previousUv.x < 0.0 || previousUv.x > 1.0 || previousUv.y < 0.0 || previousUv.y > 1.0) {
    return float4(current, 1.0);
  }

  float3 history = historyColor.Sample(linearSampler, previousUv).rgb;
  history = NeighborhoodClamp(uv, history);
  return float4(lerp(history, current, blendFactor), 1.0);
}
)";

struct TaaConstants {
  math::Mat4 inverseViewProjection{};
  math::Mat4 previousViewProjection{};
  float texelSize[2]{0.0f, 0.0f};
  float blendFactor = kBlendFactor;
  float hasHistory = 0.0f;
};

Result<void> CreateResources(RhiDevice& device, TaaRenderer::Impl& impl);

}

struct TaaRenderer::Impl {
  std::unique_ptr<RhiShader> shader;
  std::unique_ptr<RhiInputLayout> layout;
  std::unique_ptr<RhiPipeline> pipeline;
  std::unique_ptr<RhiBuffer> quadVertices;
  std::unique_ptr<RhiBuffer> constants;
  std::unique_ptr<RhiSampler> sampler;
  std::unique_ptr<RhiTexture> historyFirst;
  std::unique_ptr<RhiTexture> historySecond;
  math::Mat4 previousViewProjection{};
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t historyIndex = 0;
  bool hasHistory = false;
  bool initialized = false;
};

namespace {

Result<void> CreateResources(RhiDevice& device, TaaRenderer::Impl& impl) {
  Result<void> created = detail::CreateShader(device, kTaaShader, impl.shader);
  if (!created) {
    return created.GetError();
  }

  auto layout = RhiInputLayout::Create(device, *impl.shader, detail::kFullscreenElements);
  if (!layout) {
    return layout.GetError();
  }
  impl.layout = std::move(layout).Value();

  RhiPipelineDesc pipelineDescription;
  pipelineDescription.cullBack = false;
  pipelineDescription.depthTest = false;
  pipelineDescription.depthWrite = false;
  created = detail::CreatePipeline(device, pipelineDescription, impl.pipeline);
  if (!created) {
    return created.GetError();
  }

  auto quadVertices = RhiBuffer::Create(device, RhiBufferKind::Vertex, detail::kFullscreenVertices,
                                        static_cast<uint32_t>(sizeof(detail::kFullscreenVertices)));
  if (!quadVertices) {
    return quadVertices.GetError();
  }
  impl.quadVertices = std::move(quadVertices).Value();

  auto constants = RhiBuffer::Create(device, RhiBufferKind::Constant, nullptr,
                                     static_cast<uint32_t>(sizeof(TaaConstants)));
  if (!constants) {
    return constants.GetError();
  }
  impl.constants = std::move(constants).Value();

  RhiSamplerDesc samplerDescription;
  samplerDescription.filter = RhiFilter::Linear;
  samplerDescription.address = RhiAddressMode::Clamp;
  auto sampler = RhiSampler::Create(device, samplerDescription);
  if (!sampler) {
    return sampler.GetError();
  }
  impl.sampler = std::move(sampler).Value();

  impl.initialized = true;
  return {};
}

Result<void> EnsureHistory(RhiDevice& device, TaaRenderer::Impl& impl, uint32_t width,
                           uint32_t height) {
  if (impl.historyFirst != nullptr && impl.width == width && impl.height == height) {
    return {};
  }

  RhiTextureDesc description;
  description.width = width;
  description.height = height;
  description.format = RhiFormat::Rgba16Float;
  description.renderTarget = true;
  auto first = RhiTexture::Create(device, description);
  if (!first) {
    return first.GetError();
  }
  auto second = RhiTexture::Create(device, description);
  if (!second) {
    return second.GetError();
  }
  impl.historyFirst = std::move(first).Value();
  impl.historySecond = std::move(second).Value();
  impl.width = width;
  impl.height = height;
  impl.historyIndex = 0;
  impl.hasHistory = false;
  return {};
}

}

TaaRenderer::TaaRenderer() : m_impl(std::make_unique<Impl>()) {}

TaaRenderer::~TaaRenderer() = default;

Result<void> TaaRenderer::Apply(RhiDevice& device, void* colorView, void* depthView,
                                const math::Mat4& inverseViewProjection,
                                const math::Mat4& viewProjection, uint32_t width, uint32_t height) {
  if (colorView == nullptr || depthView == nullptr || width == 0 || height == 0) {
    return Error{"taa needs live views and a positive size", 1};
  }
  if (!m_impl->initialized) {
    const Result<void> created = CreateResources(device, *m_impl);
    if (!created) {
      return created.GetError();
    }
  }
  const Result<void> history = EnsureHistory(device, *m_impl, width, height);
  if (!history) {
    return history.GetError();
  }

  auto* context = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  const uint32_t previousIndex = m_impl->historyIndex;
  const uint32_t targetIndex = 1 - previousIndex;
  RhiTexture* target = targetIndex == 0 ? m_impl->historyFirst.get() : m_impl->historySecond.get();
  RhiTexture* previous =
      previousIndex == 0 ? m_impl->historyFirst.get() : m_impl->historySecond.get();
  auto* targetView = static_cast<ID3D11RenderTargetView*>(target->RenderTargetView());
  if (context == nullptr || targetView == nullptr) {
    return Error{"taa has no live target", 2};
  }

  ID3D11ShaderResourceView* nullViews[3] = {nullptr, nullptr, nullptr};
  context->PSSetShaderResources(0, 3, nullViews);
  context->OMSetRenderTargets(1, &targetView, nullptr);
  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(width);
  viewport.Height = static_cast<float>(height);
  viewport.MaxDepth = 1.0f;
  context->RSSetViewports(1, &viewport);

  detail::BindPipeline(context, *m_impl->shader, *m_impl->layout, *m_impl->pipeline);
  ID3D11Buffer* vertexBuffer = static_cast<ID3D11Buffer*>(m_impl->quadVertices->Handle());
  UINT stride = static_cast<UINT>(sizeof(detail::FullscreenVertex));
  UINT offset = 0;
  context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  ID3D11Buffer* constantBuffer = static_cast<ID3D11Buffer*>(m_impl->constants->Handle());
  context->PSSetConstantBuffers(0, 1, &constantBuffer);
  ID3D11SamplerState* sampler = static_cast<ID3D11SamplerState*>(m_impl->sampler->Handle());
  context->PSSetSamplers(0, 1, &sampler);

  TaaConstants constants;
  constants.inverseViewProjection = inverseViewProjection;
  constants.previousViewProjection = m_impl->previousViewProjection;
  constants.texelSize[0] = 1.0f / static_cast<float>(width);
  constants.texelSize[1] = 1.0f / static_cast<float>(height);
  constants.hasHistory = m_impl->hasHistory ? 1.0f : 0.0f;
  const Result<void> updated =
      m_impl->constants->Update(device, &constants, static_cast<uint32_t>(sizeof(constants)));
  if (!updated) {
    return updated.GetError();
  }

  ID3D11ShaderResourceView* colorResource = static_cast<ID3D11ShaderResourceView*>(colorView);
  ID3D11ShaderResourceView* depthResource = static_cast<ID3D11ShaderResourceView*>(depthView);
  ID3D11ShaderResourceView* historyResource =
      m_impl->hasHistory ? static_cast<ID3D11ShaderResourceView*>(previous->ShaderResourceView())
                         : colorResource;
  ID3D11ShaderResourceView* resources[3] = {colorResource, depthResource, historyResource};
  context->PSSetShaderResources(0, 3, resources);
  context->Draw(static_cast<UINT>(std::size(detail::kFullscreenVertices)), 0);

  context->PSSetShaderResources(0, 3, nullViews);
  context->OMSetRenderTargets(0, nullptr, nullptr);

  m_impl->historyIndex = targetIndex;
  m_impl->previousViewProjection = viewProjection;
  m_impl->hasHistory = true;
  return {};
}

void* TaaRenderer::ResultView() const noexcept {
  if (m_impl->historyFirst == nullptr) {
    return nullptr;
  }
  return m_impl->historyIndex == 0 ? m_impl->historyFirst->ShaderResourceView()
                                   : m_impl->historySecond->ShaderResourceView();
}

bool TaaRenderer::HasHistory() const noexcept {
  return m_impl->hasHistory;
}

void TaaRenderer::Reset() noexcept {
  m_impl->hasHistory = false;
  m_impl->previousViewProjection = math::Identity();
}

}
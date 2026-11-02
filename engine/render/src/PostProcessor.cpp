#include <windows.h>

#include "RenderPassUtil.h"
#include <algorithm>
#include <caligo/render/PostProcessor.h>
#include <caligo/render/RhiBuffer.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiSampler.h>
#include <caligo/render/RhiTexture.h>
#include <d3d11.h>
#include <iterator>
#include <utility>

namespace caligo::render {

namespace {

struct PostVertex {
  float position[2];
  float uv[2];
};

constexpr PostVertex kQuadVertices[] = {
    {-1.0f, -1.0f, 0.0f, 1.0f},
    {1.0f, -1.0f, 1.0f, 1.0f},
    {-1.0f, 1.0f, 0.0f, 0.0f},
    {1.0f, 1.0f, 1.0f, 0.0f},
};

constexpr RhiVertexElement kQuadElements[] = {
    {"POSITION", 0, RhiVertexFormat::Float32x2, 0},
    {"TEXCOORD", 0, RhiVertexFormat::Float32x2, 8},
};

constexpr char kBrightShader[] = R"(
cbuffer PostConstants : register(b0) {
  float2 texelSize;
  float threshold;
  float intensity;
  float exposure;
  float3 padding;
};

Texture2D sourceTexture : register(t0);
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

float4 PsMain(PostOutput input) : SV_TARGET {
  float3 color = sourceTexture.Sample(linearSampler, input.uv).rgb;
  return float4(max(color - threshold, 0.0), 1.0);
}
)";

constexpr char kBlurShader[] = R"(
cbuffer PostConstants : register(b0) {
  float2 texelSize;
  float threshold;
  float intensity;
  float exposure;
  float3 padding;
};

Texture2D sourceTexture : register(t0);
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

float4 PsMain(PostOutput input) : SV_TARGET {
  float3 total = sourceTexture.Sample(linearSampler, input.uv).rgb * 0.227027;
  total += sourceTexture.Sample(linearSampler, input.uv + texelSize).rgb * 0.1945946;
  total += sourceTexture.Sample(linearSampler, input.uv - texelSize).rgb * 0.1945946;
  total += sourceTexture.Sample(linearSampler, input.uv + texelSize * 2.0).rgb * 0.1216216;
  total += sourceTexture.Sample(linearSampler, input.uv - texelSize * 2.0).rgb * 0.1216216;
  total += sourceTexture.Sample(linearSampler, input.uv + texelSize * 3.0).rgb * 0.054054;
  total += sourceTexture.Sample(linearSampler, input.uv - texelSize * 3.0).rgb * 0.054054;
  total += sourceTexture.Sample(linearSampler, input.uv + texelSize * 4.0).rgb * 0.016216;
  total += sourceTexture.Sample(linearSampler, input.uv - texelSize * 4.0).rgb * 0.016216;
  return float4(total, 1.0);
}
)";

constexpr char kCompositeShader[] = R"(
cbuffer PostConstants : register(b0) {
  float2 texelSize;
  float threshold;
  float intensity;
  float exposure;
  float3 padding;
};

Texture2D sceneTexture : register(t0);
Texture2D bloomTexture : register(t1);
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

float3 AcesTonemap(float3 value) {
  return saturate((value * (2.51 * value + 0.03)) / (value * (2.43 * value + 0.59) + 0.14));
}

float4 PsMain(PostOutput input) : SV_TARGET {
  float3 scene = sceneTexture.Sample(linearSampler, input.uv).rgb;
  float3 bloom = bloomTexture.Sample(linearSampler, input.uv).rgb;
  float3 color = (scene + bloom * intensity) * exposure;
  color = AcesTonemap(color);
  color = pow(color, 1.0 / 2.2);
  return float4(color, 1.0);
}
)";

struct PostConstants {
  float texelSize[2]{0.0f, 0.0f};
  float threshold = 1.0f;
  float intensity = 0.6f;
  float exposure = 1.0f;
  float padding[3]{0.0f, 0.0f, 0.0f};
};

constexpr float kBloomThreshold = 1.0f;
constexpr float kBloomIntensity = 0.6f;
constexpr float kExposure = 1.0f;

Result<void> CreateResources(RhiDevice& device, PostProcessor::Impl& impl);

}

struct PostProcessor::Impl {
  std::unique_ptr<RhiShader> brightShader;
  std::unique_ptr<RhiShader> blurShader;
  std::unique_ptr<RhiShader> compositeShader;
  std::unique_ptr<RhiInputLayout> brightLayout;
  std::unique_ptr<RhiInputLayout> blurLayout;
  std::unique_ptr<RhiInputLayout> compositeLayout;
  std::unique_ptr<RhiPipeline> pipeline;
  std::unique_ptr<RhiBuffer> quadVertices;
  std::unique_ptr<RhiBuffer> constants;
  std::unique_ptr<RhiSampler> sampler;
  std::unique_ptr<RhiTexture> bloomFirst;
  std::unique_ptr<RhiTexture> bloomSecond;
  uint32_t bloomWidth = 0;
  uint32_t bloomHeight = 0;
  bool initialized = false;
};

namespace {

Result<void> CreateResources(RhiDevice& device, PostProcessor::Impl& impl) {
  Result<void> created = detail::CreateShader(device, kBrightShader, impl.brightShader);
  if (!created) {
    return created.GetError();
  }
  created = detail::CreateShader(device, kBlurShader, impl.blurShader);
  if (!created) {
    return created.GetError();
  }
  created = detail::CreateShader(device, kCompositeShader, impl.compositeShader);
  if (!created) {
    return created.GetError();
  }

  auto brightLayout = RhiInputLayout::Create(device, *impl.brightShader, kQuadElements);
  if (!brightLayout) {
    return brightLayout.GetError();
  }
  auto blurLayout = RhiInputLayout::Create(device, *impl.blurShader, kQuadElements);
  if (!blurLayout) {
    return blurLayout.GetError();
  }
  auto compositeLayout = RhiInputLayout::Create(device, *impl.compositeShader, kQuadElements);
  if (!compositeLayout) {
    return compositeLayout.GetError();
  }
  impl.brightLayout = std::move(brightLayout).Value();
  impl.blurLayout = std::move(blurLayout).Value();
  impl.compositeLayout = std::move(compositeLayout).Value();

  RhiPipelineDesc pipelineDescription;
  pipelineDescription.cullBack = false;
  pipelineDescription.depthTest = false;
  pipelineDescription.depthWrite = false;
  created = detail::CreatePipeline(device, pipelineDescription, impl.pipeline);
  if (!created) {
    return created.GetError();
  }

  auto quadVertices = RhiBuffer::Create(device, RhiBufferKind::Vertex, kQuadVertices,
                                        static_cast<uint32_t>(sizeof(kQuadVertices)));
  if (!quadVertices) {
    return quadVertices.GetError();
  }
  impl.quadVertices = std::move(quadVertices).Value();

  auto constants = RhiBuffer::Create(device, RhiBufferKind::Constant, nullptr,
                                     static_cast<uint32_t>(sizeof(PostConstants)));
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

Result<void> EnsureBloomTargets(RhiDevice& device, PostProcessor::Impl& impl, uint32_t width,
                                uint32_t height) {
  if (impl.bloomFirst != nullptr && impl.bloomWidth == width && impl.bloomHeight == height) {
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
  impl.bloomFirst = std::move(first).Value();
  impl.bloomSecond = std::move(second).Value();
  impl.bloomWidth = width;
  impl.bloomHeight = height;
  return {};
}

Result<void> UpdateConstants(RhiDevice& device, PostProcessor::Impl& impl,
                             const PostConstants& constants) {
  return impl.constants->Update(device, &constants, static_cast<uint32_t>(sizeof(constants)));
}

}

PostProcessor::PostProcessor() : m_impl(std::make_unique<Impl>()) {}

PostProcessor::~PostProcessor() = default;

Result<void> PostProcessor::Apply(RhiDevice& device, void* sourceView, void* outputView,
                                  uint32_t width, uint32_t height) {
  if (sourceView == nullptr || outputView == nullptr || width == 0 || height == 0) {
    return Error{"post processor needs live views and a positive size", 1};
  }
  if (!m_impl->initialized) {
    const Result<void> created = CreateResources(device, *m_impl);
    if (!created) {
      return created.GetError();
    }
  }

  const uint32_t bloomWidth = std::max(1u, width / 2);
  const uint32_t bloomHeight = std::max(1u, height / 2);
  const Result<void> targets = EnsureBloomTargets(device, *m_impl, bloomWidth, bloomHeight);
  if (!targets) {
    return targets.GetError();
  }

  auto* context = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  auto* firstTarget = static_cast<ID3D11RenderTargetView*>(m_impl->bloomFirst->RenderTargetView());
  auto* secondTarget =
      static_cast<ID3D11RenderTargetView*>(m_impl->bloomSecond->RenderTargetView());
  auto* outputTarget = static_cast<ID3D11RenderTargetView*>(outputView);
  if (context == nullptr || firstTarget == nullptr || secondTarget == nullptr ||
      outputTarget == nullptr) {
    return Error{"post processor has no live targets", 2};
  }

  ID3D11Buffer* constantBuffer = static_cast<ID3D11Buffer*>(m_impl->constants->Handle());
  ID3D11SamplerState* sampler = static_cast<ID3D11SamplerState*>(m_impl->sampler->Handle());
  ID3D11Buffer* vertexBuffer = static_cast<ID3D11Buffer*>(m_impl->quadVertices->Handle());
  UINT stride = static_cast<UINT>(sizeof(PostVertex));
  UINT offset = 0;
  context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  context->PSSetConstantBuffers(0, 1, &constantBuffer);
  context->PSSetSamplers(0, 1, &sampler);

  const auto bindTarget = [context](ID3D11RenderTargetView* target, uint32_t targetWidth,
                                    uint32_t targetHeight) {
    ID3D11ShaderResourceView* nullViews[2] = {nullptr, nullptr};
    context->PSSetShaderResources(0, 2, nullViews);
    context->OMSetRenderTargets(1, &target, nullptr);
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(targetWidth);
    viewport.Height = static_cast<float>(targetHeight);
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
  };

  bindTarget(firstTarget, bloomWidth, bloomHeight);
  detail::BindPipeline(context, *m_impl->brightShader, *m_impl->brightLayout, *m_impl->pipeline);
  PostConstants brightConstants;
  brightConstants.threshold = kBloomThreshold;
  Result<void> updated = UpdateConstants(device, *m_impl, brightConstants);
  if (!updated) {
    return updated.GetError();
  }
  ID3D11ShaderResourceView* sceneResource = static_cast<ID3D11ShaderResourceView*>(sourceView);
  context->PSSetShaderResources(0, 1, &sceneResource);
  context->Draw(static_cast<UINT>(std::size(kQuadVertices)), 0);

  bindTarget(secondTarget, bloomWidth, bloomHeight);
  detail::BindPipeline(context, *m_impl->blurShader, *m_impl->blurLayout, *m_impl->pipeline);
  PostConstants horizontalConstants;
  horizontalConstants.texelSize[0] = 1.0f / static_cast<float>(bloomWidth);
  updated = UpdateConstants(device, *m_impl, horizontalConstants);
  if (!updated) {
    return updated.GetError();
  }
  ID3D11ShaderResourceView* firstResource =
      static_cast<ID3D11ShaderResourceView*>(m_impl->bloomFirst->ShaderResourceView());
  context->PSSetShaderResources(0, 1, &firstResource);
  context->Draw(static_cast<UINT>(std::size(kQuadVertices)), 0);

  bindTarget(firstTarget, bloomWidth, bloomHeight);
  PostConstants verticalConstants;
  verticalConstants.texelSize[1] = 1.0f / static_cast<float>(bloomHeight);
  updated = UpdateConstants(device, *m_impl, verticalConstants);
  if (!updated) {
    return updated.GetError();
  }
  ID3D11ShaderResourceView* secondResource =
      static_cast<ID3D11ShaderResourceView*>(m_impl->bloomSecond->ShaderResourceView());
  context->PSSetShaderResources(0, 1, &secondResource);
  context->Draw(static_cast<UINT>(std::size(kQuadVertices)), 0);

  bindTarget(outputTarget, width, height);
  detail::BindPipeline(context, *m_impl->compositeShader, *m_impl->compositeLayout,
                       *m_impl->pipeline);
  PostConstants compositeConstants;
  compositeConstants.intensity = kBloomIntensity;
  compositeConstants.exposure = kExposure;
  updated = UpdateConstants(device, *m_impl, compositeConstants);
  if (!updated) {
    return updated.GetError();
  }
  ID3D11ShaderResourceView* compositeViews[2] = {sceneResource, firstResource};
  context->PSSetShaderResources(0, 2, compositeViews);
  context->Draw(static_cast<UINT>(std::size(kQuadVertices)), 0);

  ID3D11ShaderResourceView* nullViews[2] = {nullptr, nullptr};
  context->PSSetShaderResources(0, 2, nullViews);
  context->OMSetRenderTargets(0, nullptr, nullptr);
  return {};
}

}
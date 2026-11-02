#include <windows.h>

#include "FullscreenQuad.h"
#include "RenderPassUtil.h"
#include <caligo/render/LightingPass.h>
#include <caligo/render/RhiBuffer.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiSampler.h>
#include <caligo/render/RhiTexture.h>
#include <d3d11.h>
#include <iterator>
#include <utility>

namespace caligo::render {

namespace {

constexpr char kLightingShader[] = R"(
cbuffer LightingConstants : register(b0) {
  row_major float4x4 inverseViewProjection;
  row_major float4x4 lightViewProjection;
  float4 lightDirection;
  float4 lightColor;
  float4 background;
  float4 shadowParams;
};

Texture2D albedoTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D<float> depthTex : register(t2);
Texture2D shadowMap : register(t3);
SamplerState linearSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

struct LightingInput {
  float2 position : POSITION;
  float2 uv : TEXCOORD0;
};

struct LightingOutput {
  float4 position : SV_POSITION;
  float2 uv : TEXCOORD0;
};

LightingOutput VsMain(LightingInput input) {
  LightingOutput output;
  output.position = float4(input.position, 0.0, 1.0);
  output.uv = input.uv;
  return output;
}

float4 PsMain(LightingOutput input) : SV_TARGET {
  float4 albedo = albedoTex.Sample(linearSampler, input.uv);
  if (albedo.a < 0.5) {
    return float4(background.rgb, 1.0);
  }

  uint2 pixel = uint2(input.position.xy);
  float depth = depthTex.Load(int3(pixel, 0));
  float4 clip = float4(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0, depth, 1.0);
  float4 worldPosition = mul(clip, inverseViewProjection);
  worldPosition /= worldPosition.w;

  float3 normal = normalize(normalTex.Sample(linearSampler, input.uv).xyz);
  float diffuse = saturate(dot(normal, normalize(lightDirection.xyz)));

  float shadow = 1.0;
  float4 shadowPosition = mul(worldPosition, lightViewProjection);
  float3 projected = shadowPosition.xyz / shadowPosition.w;
  float2 shadowUv = float2(projected.x * 0.5 + 0.5, 0.5 - projected.y * 0.5);
  if (projected.z >= 0.0 && projected.z <= 1.0 && shadowUv.x >= 0.0 && shadowUv.x <= 1.0 &&
      shadowUv.y >= 0.0 && shadowUv.y <= 1.0) {
    float reference = projected.z - shadowParams.y;
    float2 texel = shadowParams.xx;
    float total = 0.0;
    [unroll]
    for (int row = -1; row <= 1; ++row) {
      [unroll]
      for (int column = -1; column <= 1; ++column) {
        total += shadowMap.SampleCmpLevelZero(shadowSampler, shadowUv + float2(column, row) * texel,
                                              reference);
      }
    }
    shadow = total / 9.0;
  }

  float3 ambient = albedo.rgb * 0.22;
  float3 direct = albedo.rgb * lightColor.rgb * diffuse * shadow;
  return float4(ambient + direct, 1.0);
}
)";

struct LightingConstants {
  math::Mat4 inverseViewProjection{};
  math::Mat4 lightViewProjection{};
  float lightDirection[4]{0.0f, 0.0f, 0.0f, 0.0f};
  float lightColor[4]{1.0f, 1.0f, 1.0f, 1.0f};
  float background[4]{0.0f, 0.0f, 0.0f, 1.0f};
  float shadowParams[4]{0.0f, 0.0f, 0.0f, 0.0f};
};

Result<void> CreateResources(RhiDevice& device, LightingPass::Impl& impl);

}

struct LightingPass::Impl {
  std::unique_ptr<RhiShader> shader;
  std::unique_ptr<RhiInputLayout> layout;
  std::unique_ptr<RhiPipeline> pipeline;
  std::unique_ptr<RhiBuffer> quadVertices;
  std::unique_ptr<RhiBuffer> constants;
  std::unique_ptr<RhiSampler> sampler;
  bool initialized = false;
};

namespace {

Result<void> CreateResources(RhiDevice& device, LightingPass::Impl& impl) {
  Result<void> created = detail::CreateShader(device, kLightingShader, impl.shader);
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
                                     static_cast<uint32_t>(sizeof(LightingConstants)));
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

}

LightingPass::LightingPass() : m_impl(std::make_unique<Impl>()) {}

LightingPass::~LightingPass() = default;

Result<void> LightingPass::Render(RhiDevice& device, const LightingInputs& inputs,
                                  const LightingSettings& settings, void* outputView,
                                  uint32_t width, uint32_t height) {
  if (inputs.albedo == nullptr || inputs.normal == nullptr || inputs.depth == nullptr ||
      inputs.shadowMap == nullptr || inputs.shadowSampler == nullptr || outputView == nullptr ||
      width == 0 || height == 0) {
    return Error{"lighting pass needs live inputs and a target", 1};
  }
  if (!m_impl->initialized) {
    const Result<void> created = CreateResources(device, *m_impl);
    if (!created) {
      return created.GetError();
    }
  }

  auto* context = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  auto* targetView = static_cast<ID3D11RenderTargetView*>(outputView);
  if (context == nullptr || targetView == nullptr) {
    return Error{"lighting pass has no live target", 2};
  }

  ID3D11ShaderResourceView* nullViews[4] = {nullptr, nullptr, nullptr, nullptr};
  context->PSSetShaderResources(0, 4, nullViews);
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

  ID3D11SamplerState* samplers[2] = {static_cast<ID3D11SamplerState*>(m_impl->sampler->Handle()),
                                     static_cast<ID3D11SamplerState*>(inputs.shadowSampler)};
  context->PSSetSamplers(0, 2, samplers);

  ID3D11ShaderResourceView* resources[4] = {
      static_cast<ID3D11ShaderResourceView*>(inputs.albedo),
      static_cast<ID3D11ShaderResourceView*>(inputs.normal),
      static_cast<ID3D11ShaderResourceView*>(inputs.depth),
      static_cast<ID3D11ShaderResourceView*>(inputs.shadowMap)};
  context->PSSetShaderResources(0, 4, resources);

  LightingConstants constants;
  constants.inverseViewProjection = settings.inverseViewProjection;
  constants.lightViewProjection = settings.lightViewProjection;
  constants.lightDirection[0] = settings.lightDirection.x;
  constants.lightDirection[1] = settings.lightDirection.y;
  constants.lightDirection[2] = settings.lightDirection.z;
  constants.lightColor[0] = settings.lightColor.x;
  constants.lightColor[1] = settings.lightColor.y;
  constants.lightColor[2] = settings.lightColor.z;
  constants.background[0] = settings.background.x;
  constants.background[1] = settings.background.y;
  constants.background[2] = settings.background.z;
  constants.shadowParams[0] = settings.shadowTexel;
  constants.shadowParams[1] = settings.shadowBias;
  const Result<void> updated =
      m_impl->constants->Update(device, &constants, static_cast<uint32_t>(sizeof(constants)));
  if (!updated) {
    return updated.GetError();
  }

  context->Draw(static_cast<UINT>(std::size(detail::kFullscreenVertices)), 0);

  context->PSSetShaderResources(0, 4, nullViews);
  context->OMSetRenderTargets(0, nullptr, nullptr);
  return {};
}

}

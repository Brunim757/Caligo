#include <windows.h>

#include <algorithm>
#include <caligo/render/RhiBuffer.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiInputLayout.h>
#include <caligo/render/RhiPipeline.h>
#include <caligo/render/RhiSampler.h>
#include <caligo/render/RhiShader.h>
#include <caligo/render/RhiTexture.h>
#include <caligo/render/SceneRenderer.h>
#include <caligo/render/ShadowMap.h>
#include <cmath>
#include <d3d11.h>
#include <iterator>
#include <utility>

namespace caligo::render {

namespace {

constexpr float kHalf = 0.5f;
constexpr uint32_t kShadowMapSize = 1024;

constexpr math::Vec3 kUnitCorners[] = {{-kHalf, -kHalf, -kHalf}, {kHalf, -kHalf, -kHalf},
                                       {-kHalf, kHalf, -kHalf},  {kHalf, kHalf, -kHalf},
                                       {-kHalf, -kHalf, kHalf},  {kHalf, -kHalf, kHalf},
                                       {-kHalf, kHalf, kHalf},   {kHalf, kHalf, kHalf}};

constexpr RhiVertexElement kShadowElements[] = {
    {"POSITION", 0, RhiVertexFormat::Float32x3, 0},
};

constexpr char kShadowShader[] = R"(
cbuffer ShadowConstants : register(b0) {
  row_major float4x4 world;
  row_major float4x4 lightViewProjection;
};

struct ShadowInput {
  float3 position : POSITION;
};

struct ShadowOutput {
  float4 position : SV_POSITION;
};

ShadowOutput VsMain(ShadowInput input) {
  ShadowOutput output;
  float4 worldPosition = mul(float4(input.position, 1.0), world);
  output.position = mul(worldPosition, lightViewProjection);
  return output;
}

float4 PsMain(ShadowOutput input) : SV_TARGET {
  return float4(0.0, 0.0, 0.0, 1.0);
}
)";

struct ShadowConstants {
  math::Mat4 world{};
  math::Mat4 lightViewProjection{};
};

struct SceneBounds {
  math::Vec3 minimum{};
  math::Vec3 maximum{};
  bool valid = false;
};

math::Vec3 SafeLightDirection(const math::Vec3& direction) noexcept {
  const float lengthSquared = math::Dot(direction, direction);
  if (lengthSquared < math::kEpsilon) {
    return math::Vec3{0.0f, 1.0f, 0.0f};
  }
  return math::Normalize(direction);
}

void ExpandBounds(SceneBounds& bounds, const math::Vec3& point) noexcept {
  if (!bounds.valid) {
    bounds.minimum = point;
    bounds.maximum = point;
    bounds.valid = true;
    return;
  }
  bounds.minimum.x = std::min(bounds.minimum.x, point.x);
  bounds.minimum.y = std::min(bounds.minimum.y, point.y);
  bounds.minimum.z = std::min(bounds.minimum.z, point.z);
  bounds.maximum.x = std::max(bounds.maximum.x, point.x);
  bounds.maximum.y = std::max(bounds.maximum.y, point.y);
  bounds.maximum.z = std::max(bounds.maximum.z, point.z);
}

SceneBounds ComputeBounds(std::span<const RenderItem> items,
                          std::span<const TerrainGeometry> terrains) {
  SceneBounds bounds;
  for (const RenderItem& item : items) {
    for (const math::Vec3& corner : kUnitCorners) {
      ExpandBounds(bounds, math::TransformPoint(item.world, corner));
    }
  }
  for (const TerrainGeometry& terrain : terrains) {
    for (const math::Vec3& corner : kUnitCorners) {
      math::Vec3 local;
      local.x = corner.x < 0.0f ? terrain.boundsMinimum.x : terrain.boundsMaximum.x;
      local.y = corner.y < 0.0f ? terrain.boundsMinimum.y : terrain.boundsMaximum.y;
      local.z = corner.z < 0.0f ? terrain.boundsMinimum.z : terrain.boundsMaximum.z;
      ExpandBounds(bounds, math::TransformPoint(terrain.world, local));
    }
  }
  return bounds;
}

math::Mat4 ComputeLightViewProjection(const SceneBounds& bounds, const math::Vec3& direction) {
  if (!bounds.valid) {
    return math::Identity();
  }

  const math::Vec3 extent = math::Subtract(bounds.maximum, bounds.minimum);
  const float radius = std::max(math::Length(extent) * 0.5f, 0.5f);
  const math::Vec3 center = math::Scale(math::Add(bounds.minimum, bounds.maximum), 0.5f);
  const math::Vec3 lightDirection = SafeLightDirection(direction);

  math::Vec3 up{0.0f, 1.0f, 0.0f};
  if (std::abs(lightDirection.y) > 0.99f) {
    up = math::Vec3{0.0f, 0.0f, 1.0f};
  }
  const math::Vec3 eye = math::Add(center, math::Scale(lightDirection, radius * 2.0f + 1.0f));
  const math::Mat4 view = math::LookAt(eye, center, up);

  DirectX::XMFLOAT4X4 projection{};
  DirectX::XMStoreFloat4x4(
      &projection,
      DirectX::XMMatrixOrthographicLH(radius * 2.0f, radius * 2.0f, 0.1f, radius * 4.0f + 2.0f));
  return math::Multiply(view, projection);
}

Result<void> CreateResources(RhiDevice& device, ShadowMap::Impl& impl);

}

struct ShadowMap::Impl {
  std::unique_ptr<RhiShader> shader;
  std::unique_ptr<RhiInputLayout> layout;
  std::unique_ptr<RhiPipeline> pipeline;
  std::unique_ptr<RhiBuffer> constants;
  std::unique_ptr<RhiSampler> sampler;
  std::unique_ptr<RhiTexture> texture;
  math::Mat4 lightViewProjection{};
  bool hasContent = false;
  bool initialized = false;
};

namespace {

Result<void> CreateResources(RhiDevice& device, ShadowMap::Impl& impl) {
  const RhiShaderDesc shaderDescription{kShadowShader};
  auto shader = RhiShader::Create(device, shaderDescription);
  if (!shader) {
    return shader.GetError();
  }
  impl.shader = std::move(shader).Value();

  auto layout = RhiInputLayout::Create(device, *impl.shader, kShadowElements);
  if (!layout) {
    return layout.GetError();
  }
  impl.layout = std::move(layout).Value();

  RhiPipelineDesc pipelineDescription;
  pipelineDescription.cullBack = false;
  auto pipeline = RhiPipeline::Create(device, pipelineDescription);
  if (!pipeline) {
    return pipeline.GetError();
  }
  impl.pipeline = std::move(pipeline).Value();

  auto constants = RhiBuffer::Create(device, RhiBufferKind::Constant, nullptr,
                                     static_cast<uint32_t>(sizeof(ShadowConstants)));
  if (!constants) {
    return constants.GetError();
  }
  impl.constants = std::move(constants).Value();

  RhiTextureDesc textureDescription;
  textureDescription.width = kShadowMapSize;
  textureDescription.height = kShadowMapSize;
  textureDescription.format = RhiFormat::Depth32Float;
  textureDescription.depthStencil = true;
  auto texture = RhiTexture::Create(device, textureDescription);
  if (!texture) {
    return texture.GetError();
  }
  impl.texture = std::move(texture).Value();

  RhiSamplerDesc samplerDescription;
  samplerDescription.comparison = true;
  samplerDescription.address = RhiAddressMode::Border;
  samplerDescription.borderWhite = true;
  auto sampler = RhiSampler::Create(device, samplerDescription);
  if (!sampler) {
    return sampler.GetError();
  }
  impl.sampler = std::move(sampler).Value();

  impl.initialized = true;
  return {};
}

}

ShadowMap::ShadowMap() : m_impl(std::make_unique<Impl>()) {}

ShadowMap::~ShadowMap() = default;

Result<void> ShadowMap::Render(RhiDevice& device, const MeshGeometry& mesh,
                               std::span<const RenderItem> items,
                               std::span<const TerrainGeometry> terrains,
                               const math::Vec3& lightDirection) {
  m_impl->hasContent = false;
  if (!m_impl->initialized) {
    const Result<void> created = CreateResources(device, *m_impl);
    if (!created) {
      return created.GetError();
    }
  }
  const bool hasMesh = mesh.vertices != nullptr && mesh.indices != nullptr;
  if (!hasMesh && terrains.empty()) {
    return {};
  }

  auto* context = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  auto* depthView = static_cast<ID3D11DepthStencilView*>(m_impl->texture->DepthStencilView());
  if (context == nullptr || depthView == nullptr) {
    return Error{"shadow map has no depth target", 1};
  }

  m_impl->lightViewProjection = ComputeLightViewProjection(
      ComputeBounds(hasMesh ? items : std::span<const RenderItem>{}, terrains), lightDirection);

  context->OMSetRenderTargets(0, nullptr, depthView);
  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(kShadowMapSize);
  viewport.Height = static_cast<float>(kShadowMapSize);
  viewport.MaxDepth = 1.0f;
  context->RSSetViewports(1, &viewport);
  context->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

  context->IASetInputLayout(static_cast<ID3D11InputLayout*>(m_impl->layout->Handle()));
  context->VSSetShader(static_cast<ID3D11VertexShader*>(m_impl->shader->VertexShaderHandle()),
                       nullptr, 0);
  context->PSSetShader(static_cast<ID3D11PixelShader*>(m_impl->shader->PixelShaderHandle()),
                       nullptr, 0);
  context->RSSetState(static_cast<ID3D11RasterizerState*>(m_impl->pipeline->RasterizerState()));
  context->OMSetDepthStencilState(
      static_cast<ID3D11DepthStencilState*>(m_impl->pipeline->DepthStencilState()), 0);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  ID3D11Buffer* constantBuffer = static_cast<ID3D11Buffer*>(m_impl->constants->Handle());
  context->VSSetConstantBuffers(0, 1, &constantBuffer);

  if (hasMesh) {
    ID3D11Buffer* vertexBuffer = static_cast<ID3D11Buffer*>(mesh.vertices->Handle());
    UINT vertexStride = static_cast<UINT>(mesh.vertexStride);
    UINT vertexOffset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);
    context->IASetIndexBuffer(static_cast<ID3D11Buffer*>(mesh.indices->Handle()),
                              DXGI_FORMAT_R16_UINT, 0);

    for (const RenderItem& item : items) {
      ShadowConstants constants;
      constants.world = item.world;
      constants.lightViewProjection = m_impl->lightViewProjection;
      const Result<void> updated =
          m_impl->constants->Update(device, &constants, static_cast<uint32_t>(sizeof(constants)));
      if (!updated) {
        return updated.GetError();
      }
      context->DrawIndexed(static_cast<UINT>(mesh.indexCount), 0, 0);
    }
  }

  for (const TerrainGeometry& terrain : terrains) {
    if (terrain.vertices == nullptr || terrain.indices == nullptr || terrain.indexCount == 0) {
      continue;
    }
    ID3D11Buffer* vertexBuffer = static_cast<ID3D11Buffer*>(terrain.vertices->Handle());
    UINT vertexStride = static_cast<UINT>(terrain.vertexStride);
    UINT vertexOffset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);
    context->IASetIndexBuffer(static_cast<ID3D11Buffer*>(terrain.indices->Handle()),
                              DXGI_FORMAT_R16_UINT, 0);

    ShadowConstants constants;
    constants.world = terrain.world;
    constants.lightViewProjection = m_impl->lightViewProjection;
    const Result<void> updated =
        m_impl->constants->Update(device, &constants, static_cast<uint32_t>(sizeof(constants)));
    if (!updated) {
      return updated.GetError();
    }
    context->DrawIndexed(static_cast<UINT>(terrain.indexCount), 0, 0);
  }

  context->OMSetRenderTargets(0, nullptr, nullptr);
  m_impl->hasContent = true;
  return {};
}

void* ShadowMap::ResourceView() const noexcept {
  return m_impl->texture != nullptr ? m_impl->texture->ShaderResourceView() : nullptr;
}

void* ShadowMap::Sampler() const noexcept {
  return m_impl->sampler != nullptr ? m_impl->sampler->Handle() : nullptr;
}

const math::Mat4& ShadowMap::LightViewProjection() const noexcept {
  return m_impl->lightViewProjection;
}

bool ShadowMap::HasContent() const noexcept {
  return m_impl->hasContent;
}

float ShadowMap::TexelSize() const noexcept {
  return 1.0f / static_cast<float>(kShadowMapSize);
}

}

#include <windows.h>

#include "RenderPassUtil.h"
#include <caligo/render/GBufferPass.h>
#include <caligo/render/RhiBuffer.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiInputLayout.h>
#include <caligo/render/RhiPipeline.h>
#include <caligo/render/RhiShader.h>
#include <caligo/render/RhiTexture.h>
#include <caligo/render/SceneRenderer.h>
#include <d3d11.h>
#include <iterator>
#include <utility>

namespace caligo::render {

namespace {

constexpr RhiVertexElement kMeshElements[] = {
    {"POSITION", 0, RhiVertexFormat::Float32x3, 0},
    {"NORMAL", 0, RhiVertexFormat::Float32x3, 12},
};

constexpr RhiVertexElement kTerrainElements[] = {
    {"POSITION", 0, RhiVertexFormat::Float32x3, 0},
    {"NORMAL", 0, RhiVertexFormat::Float32x3, 12},
    {"COLOR", 0, RhiVertexFormat::Float32x3, 24},
};

constexpr char kGBufferShader[] = R"(
cbuffer GBufferConstants : register(b0) {
  row_major float4x4 world;
  row_major float4x4 viewProjection;
  float4 baseColor;
};

struct GBufferInput {
  float3 position : POSITION;
  float3 normal : NORMAL;
};

struct GBufferOutput {
  float4 position : SV_POSITION;
  float3 normal : NORMAL;
};

struct GBufferTargets {
  float4 albedo : SV_TARGET0;
  float4 normal : SV_TARGET1;
};

GBufferOutput VsMain(GBufferInput input) {
  GBufferOutput output;
  float4 worldPosition = mul(float4(input.position, 1.0), world);
  output.position = mul(worldPosition, viewProjection);
  output.normal = mul(input.normal, (float3x3)world);
  return output;
}

GBufferTargets PsMain(GBufferOutput input) {
  GBufferTargets output;
  output.albedo = float4(baseColor.rgb, 1.0);
  output.normal = float4(normalize(input.normal), 1.0);
  return output;
}
)";

constexpr char kTerrainShader[] = R"(
cbuffer GBufferConstants : register(b0) {
  row_major float4x4 world;
  row_major float4x4 viewProjection;
  float4 baseColor;
};

struct TerrainInput {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float3 color : COLOR;
};

struct TerrainOutput {
  float4 position : SV_POSITION;
  float3 normal : NORMAL;
  float3 color : COLOR;
};

struct TerrainTargets {
  float4 albedo : SV_TARGET0;
  float4 normal : SV_TARGET1;
};

TerrainOutput VsMain(TerrainInput input) {
  TerrainOutput output;
  float4 worldPosition = mul(float4(input.position, 1.0), world);
  output.position = mul(worldPosition, viewProjection);
  output.normal = mul(input.normal, (float3x3)world);
  output.color = input.color;
  return output;
}

TerrainTargets PsMain(TerrainOutput input) {
  TerrainTargets output;
  output.albedo = float4(input.color, 1.0);
  output.normal = float4(normalize(input.normal), 1.0);
  return output;
}
)";

struct GBufferConstants {
  math::Mat4 world{};
  math::Mat4 viewProjection{};
  float baseColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
};

Result<void> CreateResources(RhiDevice& device, GBufferPass::Impl& impl);

}

struct GBufferPass::Impl {
  std::unique_ptr<RhiShader> shader;
  std::unique_ptr<RhiInputLayout> layout;
  std::unique_ptr<RhiPipeline> pipeline;
  std::unique_ptr<RhiShader> terrainShader;
  std::unique_ptr<RhiInputLayout> terrainLayout;
  std::unique_ptr<RhiPipeline> terrainPipeline;
  std::unique_ptr<RhiBuffer> constants;
  bool initialized = false;
};

namespace {

Result<void> CreateResources(RhiDevice& device, GBufferPass::Impl& impl) {
  Result<void> created = detail::CreateShader(device, kGBufferShader, impl.shader);
  if (!created) {
    return created.GetError();
  }

  auto layout = RhiInputLayout::Create(device, *impl.shader, kMeshElements);
  if (!layout) {
    return layout.GetError();
  }
  impl.layout = std::move(layout).Value();

  RhiPipelineDesc pipelineDescription;
  pipelineDescription.cullBack = false;
  created = detail::CreatePipeline(device, pipelineDescription, impl.pipeline);
  if (!created) {
    return created.GetError();
  }

  created = detail::CreateShader(device, kTerrainShader, impl.terrainShader);
  if (!created) {
    return created.GetError();
  }

  auto terrainLayout = RhiInputLayout::Create(device, *impl.terrainShader, kTerrainElements);
  if (!terrainLayout) {
    return terrainLayout.GetError();
  }
  impl.terrainLayout = std::move(terrainLayout).Value();

  created = detail::CreatePipeline(device, pipelineDescription, impl.terrainPipeline);
  if (!created) {
    return created.GetError();
  }

  auto constants = RhiBuffer::Create(device, RhiBufferKind::Constant, nullptr,
                                     static_cast<uint32_t>(sizeof(GBufferConstants)));
  if (!constants) {
    return constants.GetError();
  }
  impl.constants = std::move(constants).Value();

  impl.initialized = true;
  return {};
}

}

GBufferPass::GBufferPass() : m_impl(std::make_unique<Impl>()) {}

GBufferPass::~GBufferPass() = default;

Result<void> GBufferPass::Render(RhiDevice& device, const MeshGeometry& mesh,
                                 std::span<const RenderItem> items,
                                 std::span<const TerrainGeometry> terrains,
                                 const math::Mat4& viewProjection, const RhiTexture& albedo,
                                 const RhiTexture& normal, const RhiTexture& depth) {
  const bool hasMesh = mesh.vertices != nullptr && mesh.indices != nullptr;
  if (!hasMesh && terrains.empty()) {
    return Error{"g-buffer needs geometry", 1};
  }
  if (!m_impl->initialized) {
    const Result<void> created = CreateResources(device, *m_impl);
    if (!created) {
      return created.GetError();
    }
  }

  auto* context = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  auto* albedoView = static_cast<ID3D11RenderTargetView*>(albedo.RenderTargetView());
  auto* normalView = static_cast<ID3D11RenderTargetView*>(normal.RenderTargetView());
  auto* depthView = static_cast<ID3D11DepthStencilView*>(depth.DepthStencilView());
  if (context == nullptr || albedoView == nullptr || normalView == nullptr ||
      depthView == nullptr) {
    return Error{"g-buffer has no live targets", 2};
  }

  ID3D11ShaderResourceView* nullViews[3] = {nullptr, nullptr, nullptr};
  context->PSSetShaderResources(0, 3, nullViews);
  ID3D11RenderTargetView* targets[2] = {albedoView, normalView};
  context->OMSetRenderTargets(2, targets, depthView);

  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(albedo.Width());
  viewport.Height = static_cast<float>(albedo.Height());
  viewport.MaxDepth = 1.0f;
  context->RSSetViewports(1, &viewport);

  const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  context->ClearRenderTargetView(albedoView, clearColor);
  context->ClearRenderTargetView(normalView, clearColor);
  context->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ID3D11Buffer* constantBuffer = static_cast<ID3D11Buffer*>(m_impl->constants->Handle());
  context->VSSetConstantBuffers(0, 1, &constantBuffer);
  context->PSSetConstantBuffers(0, 1, &constantBuffer);

  if (hasMesh) {
    detail::BindPipeline(context, *m_impl->shader, *m_impl->layout, *m_impl->pipeline);
    ID3D11Buffer* vertexBuffer = static_cast<ID3D11Buffer*>(mesh.vertices->Handle());
    UINT stride = static_cast<UINT>(mesh.vertexStride);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(static_cast<ID3D11Buffer*>(mesh.indices->Handle()),
                              DXGI_FORMAT_R16_UINT, 0);

    for (const RenderItem& item : items) {
      GBufferConstants constants;
      constants.world = item.world;
      constants.viewProjection = viewProjection;
      constants.baseColor[0] = item.color.x;
      constants.baseColor[1] = item.color.y;
      constants.baseColor[2] = item.color.z;
      constants.baseColor[3] = 1.0f;
      const Result<void> updated =
          m_impl->constants->Update(device, &constants, static_cast<uint32_t>(sizeof(constants)));
      if (!updated) {
        return updated.GetError();
      }
      context->DrawIndexed(static_cast<UINT>(mesh.indexCount), 0, 0);
    }
  }

  if (!terrains.empty()) {
    detail::BindPipeline(context, *m_impl->terrainShader, *m_impl->terrainLayout,
                         *m_impl->terrainPipeline);
    for (const TerrainGeometry& terrain : terrains) {
      if (terrain.vertices == nullptr || terrain.indices == nullptr || terrain.indexCount == 0) {
        continue;
      }
      ID3D11Buffer* vertexBuffer = static_cast<ID3D11Buffer*>(terrain.vertices->Handle());
      UINT stride = static_cast<UINT>(terrain.vertexStride);
      UINT offset = 0;
      context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
      context->IASetIndexBuffer(static_cast<ID3D11Buffer*>(terrain.indices->Handle()),
                                DXGI_FORMAT_R16_UINT, 0);

      GBufferConstants constants;
      constants.world = terrain.world;
      constants.viewProjection = viewProjection;
      const Result<void> updated =
          m_impl->constants->Update(device, &constants, static_cast<uint32_t>(sizeof(constants)));
      if (!updated) {
        return updated.GetError();
      }
      context->DrawIndexed(static_cast<UINT>(terrain.indexCount), 0, 0);
    }
  }

  context->OMSetRenderTargets(0, nullptr, nullptr);
  return {};
}

}

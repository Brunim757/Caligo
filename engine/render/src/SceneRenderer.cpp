#include <windows.h>

#include <algorithm>
#include <caligo/render/RhiBuffer.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiInputLayout.h>
#include <caligo/render/RhiPipeline.h>
#include <caligo/render/RhiShader.h>
#include <caligo/render/RhiTexture.h>
#include <caligo/render/SceneRenderer.h>
#include <caligo/render/ShadowMap.h>
#include <cmath>
#include <d3d11.h>
#include <iterator>
#include <utility>
#include <vector>

namespace caligo::render {

namespace {

struct MeshVertex {
  float position[3];
  float normal[3];
};

constexpr float kHalf = 0.5f;
constexpr int kGridHalfLines = 10;
constexpr float kShadowBias = 0.0025f;

constexpr MeshVertex kCubeVertices[] = {
    {{-kHalf, -kHalf, kHalf}, {0.0f, 0.0f, 1.0f}},
    {{kHalf, -kHalf, kHalf}, {0.0f, 0.0f, 1.0f}},
    {{kHalf, kHalf, kHalf}, {0.0f, 0.0f, 1.0f}},
    {{-kHalf, kHalf, kHalf}, {0.0f, 0.0f, 1.0f}},
    {{kHalf, -kHalf, -kHalf}, {0.0f, 0.0f, -1.0f}},
    {{-kHalf, -kHalf, -kHalf}, {0.0f, 0.0f, -1.0f}},
    {{-kHalf, kHalf, -kHalf}, {0.0f, 0.0f, -1.0f}},
    {{kHalf, kHalf, -kHalf}, {0.0f, 0.0f, -1.0f}},
    {{kHalf, -kHalf, kHalf}, {1.0f, 0.0f, 0.0f}},
    {{kHalf, -kHalf, -kHalf}, {1.0f, 0.0f, 0.0f}},
    {{kHalf, kHalf, -kHalf}, {1.0f, 0.0f, 0.0f}},
    {{kHalf, kHalf, kHalf}, {1.0f, 0.0f, 0.0f}},
    {{-kHalf, -kHalf, -kHalf}, {-1.0f, 0.0f, 0.0f}},
    {{-kHalf, -kHalf, kHalf}, {-1.0f, 0.0f, 0.0f}},
    {{-kHalf, kHalf, kHalf}, {-1.0f, 0.0f, 0.0f}},
    {{-kHalf, kHalf, -kHalf}, {-1.0f, 0.0f, 0.0f}},
    {{-kHalf, kHalf, kHalf}, {0.0f, 1.0f, 0.0f}},
    {{kHalf, kHalf, kHalf}, {0.0f, 1.0f, 0.0f}},
    {{kHalf, kHalf, -kHalf}, {0.0f, 1.0f, 0.0f}},
    {{-kHalf, kHalf, -kHalf}, {0.0f, 1.0f, 0.0f}},
    {{-kHalf, -kHalf, -kHalf}, {0.0f, -1.0f, 0.0f}},
    {{kHalf, -kHalf, -kHalf}, {0.0f, -1.0f, 0.0f}},
    {{kHalf, -kHalf, kHalf}, {0.0f, -1.0f, 0.0f}},
    {{-kHalf, -kHalf, kHalf}, {0.0f, -1.0f, 0.0f}},
};

constexpr uint16_t kCubeIndices[] = {0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,
                                     8,  9,  10, 8,  10, 11, 12, 13, 14, 12, 14, 15,
                                     16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23};

constexpr RhiVertexElement kBoxElements[] = {
    {"POSITION", 0, RhiVertexFormat::Float32x3, 0},
    {"NORMAL", 0, RhiVertexFormat::Float32x3, 12},
};

constexpr RhiVertexElement kGridElements[] = {
    {"POSITION", 0, RhiVertexFormat::Float32x3, 0},
};

constexpr char kBoxShader[] = R"(
cbuffer BoxConstants : register(b0) {
  row_major float4x4 world;
  row_major float4x4 viewProjection;
  row_major float4x4 lightViewProjection;
  float4 baseColor;
  float4 lightDirection;
  float4 lightColor;
  float4 shadowParams;
};

Texture2D shadowMap : register(t0);
SamplerComparisonState shadowSampler : register(s0);

struct BoxInput {
  float3 position : POSITION;
  float3 normal : NORMAL;
};

struct BoxOutput {
  float4 position : SV_POSITION;
  float3 normal : NORMAL;
  float4 shadowPosition : TEXCOORD0;
};

BoxOutput VsMain(BoxInput input) {
  BoxOutput output;
  float4 worldPosition = mul(float4(input.position, 1.0), world);
  output.position = mul(worldPosition, viewProjection);
  output.normal = mul(input.normal, (float3x3)world);
  output.shadowPosition = mul(worldPosition, lightViewProjection);
  return output;
}

float4 PsMain(BoxOutput input) : SV_TARGET {
  float3 normal = normalize(input.normal);
  float diffuse = saturate(dot(normal, normalize(lightDirection.xyz)));

  float shadow = 1.0;
  float3 projected = input.shadowPosition.xyz / input.shadowPosition.w;
  float2 uv = float2(projected.x * 0.5 + 0.5, 0.5 - projected.y * 0.5);
  if (projected.z >= 0.0 && projected.z <= 1.0 && uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 &&
      uv.y <= 1.0) {
    float reference = projected.z - shadowParams.y;
    float2 texel = shadowParams.xx;
    float total = 0.0;
    [unroll]
    for (int row = -1; row <= 1; ++row) {
      [unroll]
      for (int column = -1; column <= 1; ++column) {
        total +=
            shadowMap.SampleCmpLevelZero(shadowSampler, uv + float2(column, row) * texel, reference);
      }
    }
    shadow = total / 9.0;
  }

  float3 ambient = baseColor.rgb * 0.22;
  float3 direct = baseColor.rgb * lightColor.rgb * diffuse * shadow;
  return float4(ambient + direct, 1.0);
}
)";

constexpr char kGridShader[] = R"(
cbuffer GridConstants : register(b0) {
  row_major float4x4 viewProjection;
  float4 gridColor;
};

struct GridInput {
  float3 position : POSITION;
};

float4 VsMain(GridInput input) : SV_POSITION {
  return mul(float4(input.position, 1.0), viewProjection);
}

float4 PsMain() : SV_TARGET {
  return gridColor;
}
)";

struct BoxConstants {
  math::Mat4 world{};
  math::Mat4 viewProjection{};
  math::Mat4 lightViewProjection{};
  float baseColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
  float lightDirection[4]{0.0f, 0.0f, 0.0f, 0.0f};
  float lightColor[4]{1.0f, 1.0f, 1.0f, 1.0f};
  float shadowParams[4]{0.0f, 0.0f, 0.0f, 0.0f};
};

struct GridConstants {
  math::Mat4 viewProjection{};
  float gridColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
};

std::vector<float> BuildGridVertices() {
  std::vector<float> vertices;
  vertices.reserve(static_cast<size_t>(kGridHalfLines * 2 + 1) * 4 * 3);
  const float extent = static_cast<float>(kGridHalfLines);
  for (int index = -kGridHalfLines; index <= kGridHalfLines; ++index) {
    const float offset = static_cast<float>(index);
    vertices.insert(vertices.end(), {offset, 0.0f, -extent, offset, 0.0f, extent});
    vertices.insert(vertices.end(), {-extent, 0.0f, offset, extent, 0.0f, offset});
  }
  return vertices;
}

math::Vec3 SafeLightDirection(const math::Vec3& direction) noexcept {
  const float lengthSquared = math::Dot(direction, direction);
  if (lengthSquared < math::kEpsilon) {
    return math::Vec3{0.0f, 1.0f, 0.0f};
  }
  return math::Normalize(direction);
}

Result<void> CreateTargets(RhiDevice& device, uint32_t width, uint32_t height,
                           SceneRenderer::Impl& impl);

}

struct SceneRenderer::Impl {
  std::unique_ptr<RhiShader> boxShader;
  std::unique_ptr<RhiShader> gridShader;
  std::unique_ptr<RhiInputLayout> boxLayout;
  std::unique_ptr<RhiInputLayout> gridLayout;
  std::unique_ptr<RhiPipeline> boxPipeline;
  std::unique_ptr<RhiPipeline> gridPipeline;
  std::unique_ptr<RhiBuffer> boxVertices;
  std::unique_ptr<RhiBuffer> boxIndices;
  std::unique_ptr<RhiBuffer> gridVertices;
  std::unique_ptr<RhiBuffer> boxConstants;
  std::unique_ptr<RhiBuffer> gridConstants;
  ShadowMap shadowMap;
  std::unique_ptr<RhiTexture> color;
  std::unique_ptr<RhiTexture> depth;
  uint32_t gridVertexCount = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  bool initialized = false;
};

namespace {

Result<void> CreateTargets(RhiDevice& device, uint32_t width, uint32_t height,
                           SceneRenderer::Impl& impl) {
  RhiTextureDesc colorDesc;
  colorDesc.width = width;
  colorDesc.height = height;
  colorDesc.format = RhiFormat::Rgba8Unorm;
  colorDesc.renderTarget = true;
  auto color = RhiTexture::Create(device, colorDesc);
  if (!color) {
    return color.GetError();
  }

  RhiTextureDesc depthDesc;
  depthDesc.width = width;
  depthDesc.height = height;
  depthDesc.format = RhiFormat::Depth32Float;
  depthDesc.depthStencil = true;
  auto depth = RhiTexture::Create(device, depthDesc);
  if (!depth) {
    return depth.GetError();
  }

  impl.color = std::move(color).Value();
  impl.depth = std::move(depth).Value();
  impl.width = width;
  impl.height = height;
  return {};
}

template <typename T>
Result<void> CreateShader(RhiDevice& device, const char* source, std::unique_ptr<T>& target) {
  const RhiShaderDesc description{source};
  auto shader = RhiShader::Create(device, description);
  if (!shader) {
    return shader.GetError();
  }
  target = std::move(shader).Value();
  return {};
}

template <typename T>
Result<void> CreatePipeline(RhiDevice& device, const RhiPipelineDesc& description,
                            std::unique_ptr<T>& target) {
  auto pipeline = RhiPipeline::Create(device, description);
  if (!pipeline) {
    return pipeline.GetError();
  }
  target = std::move(pipeline).Value();
  return {};
}

Result<void> Initialize(RhiDevice& device, SceneRenderer::Impl& impl) {
  Result<void> shaders = CreateShader(device, kBoxShader, impl.boxShader);
  if (!shaders) {
    return shaders.GetError();
  }
  shaders = CreateShader(device, kGridShader, impl.gridShader);
  if (!shaders) {
    return shaders.GetError();
  }

  auto boxLayout = RhiInputLayout::Create(device, *impl.boxShader, kBoxElements);
  if (!boxLayout) {
    return boxLayout.GetError();
  }
  auto gridLayout = RhiInputLayout::Create(device, *impl.gridShader, kGridElements);
  if (!gridLayout) {
    return gridLayout.GetError();
  }
  impl.boxLayout = std::move(boxLayout).Value();
  impl.gridLayout = std::move(gridLayout).Value();

  RhiPipelineDesc boxDescription;
  boxDescription.cullBack = false;
  Result<void> pipelines = CreatePipeline(device, boxDescription, impl.boxPipeline);
  if (!pipelines) {
    return pipelines.GetError();
  }
  RhiPipelineDesc gridDescription;
  gridDescription.cullBack = false;
  gridDescription.depthWrite = false;
  pipelines = CreatePipeline(device, gridDescription, impl.gridPipeline);
  if (!pipelines) {
    return pipelines.GetError();
  }

  auto boxVertices = RhiBuffer::Create(device, RhiBufferKind::Vertex, kCubeVertices,
                                       static_cast<uint32_t>(sizeof(kCubeVertices)));
  if (!boxVertices) {
    return boxVertices.GetError();
  }
  auto boxIndices = RhiBuffer::Create(device, RhiBufferKind::Index, kCubeIndices,
                                      static_cast<uint32_t>(sizeof(kCubeIndices)));
  if (!boxIndices) {
    return boxIndices.GetError();
  }
  const std::vector<float> grid = BuildGridVertices();
  auto gridVertices = RhiBuffer::Create(device, RhiBufferKind::Vertex, grid.data(),
                                        static_cast<uint32_t>(grid.size() * sizeof(float)));
  if (!gridVertices) {
    return gridVertices.GetError();
  }
  impl.boxVertices = std::move(boxVertices).Value();
  impl.boxIndices = std::move(boxIndices).Value();
  impl.gridVertices = std::move(gridVertices).Value();
  impl.gridVertexCount = static_cast<uint32_t>(grid.size() / 3);

  auto boxConstants = RhiBuffer::Create(device, RhiBufferKind::Constant, nullptr,
                                        static_cast<uint32_t>(sizeof(BoxConstants)));
  if (!boxConstants) {
    return boxConstants.GetError();
  }
  auto gridConstants = RhiBuffer::Create(device, RhiBufferKind::Constant, nullptr,
                                         static_cast<uint32_t>(sizeof(GridConstants)));
  if (!gridConstants) {
    return gridConstants.GetError();
  }
  impl.boxConstants = std::move(boxConstants).Value();
  impl.gridConstants = std::move(gridConstants).Value();

  impl.initialized = true;
  return {};
}

void BindPipeline(ID3D11DeviceContext* context, const RhiShader& shader,
                  const RhiInputLayout& layout, const RhiPipeline& pipeline) {
  context->IASetInputLayout(static_cast<ID3D11InputLayout*>(layout.Handle()));
  context->VSSetShader(static_cast<ID3D11VertexShader*>(shader.VertexShaderHandle()), nullptr, 0);
  context->PSSetShader(static_cast<ID3D11PixelShader*>(shader.PixelShaderHandle()), nullptr, 0);
  context->RSSetState(static_cast<ID3D11RasterizerState*>(pipeline.RasterizerState()));
  context->OMSetDepthStencilState(
      static_cast<ID3D11DepthStencilState*>(pipeline.DepthStencilState()), 0);
}

template <typename T>
void BindVertexBuffer(ID3D11DeviceContext* context, const std::unique_ptr<T>& buffer,
                      uint32_t stride) {
  ID3D11Buffer* handle = static_cast<ID3D11Buffer*>(buffer->Handle());
  UINT bufferStride = static_cast<UINT>(stride);
  UINT offset = 0;
  context->IASetVertexBuffers(0, 1, &handle, &bufferStride, &offset);
}

}

SceneRenderer::SceneRenderer() : m_impl(std::make_unique<Impl>()) {}

SceneRenderer::~SceneRenderer() = default;

Result<void> SceneRenderer::Render(RhiDevice& device, std::span<const RenderItem> items,
                                   const RenderCamera& camera, const RenderLight& light,
                                   uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return Error{"scene renderer needs a positive size", 1};
  }
  if (!m_impl->initialized) {
    const Result<void> initialized = Initialize(device, *m_impl);
    if (!initialized) {
      return initialized.GetError();
    }
  }
  if (m_impl->color == nullptr || m_impl->width != width || m_impl->height != height) {
    const Result<void> targets = CreateTargets(device, width, height, *m_impl);
    if (!targets) {
      return targets.GetError();
    }
  }

  const math::Vec3 lightDirection = SafeLightDirection(light.direction);

  ShadowMesh shadowMesh;
  shadowMesh.vertices = m_impl->boxVertices.get();
  shadowMesh.vertexStride = static_cast<uint32_t>(sizeof(MeshVertex));
  shadowMesh.indices = m_impl->boxIndices.get();
  shadowMesh.indexCount = static_cast<uint32_t>(std::size(kCubeIndices));
  const Result<void> shadows = m_impl->shadowMap.Render(device, shadowMesh, items, light.direction);
  if (!shadows) {
    return shadows.GetError();
  }
  const math::Mat4 lightViewProjection = m_impl->shadowMap.LightViewProjection();

  auto* context = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  auto* targetView = static_cast<ID3D11RenderTargetView*>(m_impl->color->RenderTargetView());
  auto* depthView = static_cast<ID3D11DepthStencilView*>(m_impl->depth->DepthStencilView());
  if (context == nullptr || targetView == nullptr || depthView == nullptr) {
    return Error{"scene renderer has no live targets", 2};
  }

  context->OMSetRenderTargets(1, &targetView, depthView);
  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(width);
  viewport.Height = static_cast<float>(height);
  viewport.MaxDepth = 1.0f;
  context->RSSetViewports(1, &viewport);

  const float clear[4] = {0.086f, 0.098f, 0.118f, 1.0f};
  context->ClearRenderTargetView(targetView, clear);
  context->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

  BindPipeline(context, *m_impl->boxShader, *m_impl->boxLayout, *m_impl->boxPipeline);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  BindVertexBuffer(context, m_impl->boxVertices, static_cast<uint32_t>(sizeof(MeshVertex)));
  context->IASetIndexBuffer(static_cast<ID3D11Buffer*>(m_impl->boxIndices->Handle()),
                            DXGI_FORMAT_R16_UINT, 0);
  ID3D11Buffer* boxConstantBuffer = static_cast<ID3D11Buffer*>(m_impl->boxConstants->Handle());
  context->VSSetConstantBuffers(0, 1, &boxConstantBuffer);
  context->PSSetConstantBuffers(0, 1, &boxConstantBuffer);

  ID3D11ShaderResourceView* shadowResource =
      static_cast<ID3D11ShaderResourceView*>(m_impl->shadowMap.ResourceView());
  ID3D11SamplerState* shadowSampler = static_cast<ID3D11SamplerState*>(m_impl->shadowMap.Sampler());
  context->PSSetShaderResources(0, 1, &shadowResource);
  context->PSSetSamplers(0, 1, &shadowSampler);

  for (const RenderItem& item : items) {
    BoxConstants constants;
    constants.world = item.world;
    constants.viewProjection = math::Multiply(camera.view, camera.projection);
    constants.lightViewProjection = lightViewProjection;
    constants.baseColor[0] = item.color.x;
    constants.baseColor[1] = item.color.y;
    constants.baseColor[2] = item.color.z;
    constants.baseColor[3] = 1.0f;
    constants.lightDirection[0] = lightDirection.x;
    constants.lightDirection[1] = lightDirection.y;
    constants.lightDirection[2] = lightDirection.z;
    constants.lightColor[0] = light.color.x;
    constants.lightColor[1] = light.color.y;
    constants.lightColor[2] = light.color.z;
    constants.shadowParams[0] = m_impl->shadowMap.TexelSize();
    constants.shadowParams[1] = kShadowBias;
    const Result<void> updated =
        m_impl->boxConstants->Update(device, &constants, static_cast<uint32_t>(sizeof(constants)));
    if (!updated) {
      return updated.GetError();
    }
    context->DrawIndexed(static_cast<UINT>(std::size(kCubeIndices)), 0, 0);
  }

  BindPipeline(context, *m_impl->gridShader, *m_impl->gridLayout, *m_impl->gridPipeline);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  BindVertexBuffer(context, m_impl->gridVertices, static_cast<uint32_t>(sizeof(float) * 3));
  context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
  ID3D11Buffer* gridConstantBuffer = static_cast<ID3D11Buffer*>(m_impl->gridConstants->Handle());
  context->VSSetConstantBuffers(0, 1, &gridConstantBuffer);
  context->PSSetConstantBuffers(0, 1, &gridConstantBuffer);

  GridConstants gridConstants;
  gridConstants.viewProjection = math::Multiply(camera.view, camera.projection);
  gridConstants.gridColor[0] = 0.20f;
  gridConstants.gridColor[1] = 0.24f;
  gridConstants.gridColor[2] = 0.30f;
  gridConstants.gridColor[3] = 1.0f;
  const Result<void> gridUpdated = m_impl->gridConstants->Update(
      device, &gridConstants, static_cast<uint32_t>(sizeof(gridConstants)));
  if (!gridUpdated) {
    return gridUpdated.GetError();
  }
  context->Draw(m_impl->gridVertexCount, 0);

  context->OMSetRenderTargets(0, nullptr, nullptr);
  return {};
}

void* SceneRenderer::ColorView() const noexcept {
  return m_impl->color != nullptr ? m_impl->color->ShaderResourceView() : nullptr;
}

Result<std::vector<uint8_t>> SceneRenderer::ReadColor() const {
  if (m_impl->color == nullptr) {
    return Error{"scene renderer has no color target", 1};
  }
  return m_impl->color->ReadPixels();
}

uint32_t SceneRenderer::Width() const noexcept {
  return m_impl->width;
}

uint32_t SceneRenderer::Height() const noexcept {
  return m_impl->height;
}

}

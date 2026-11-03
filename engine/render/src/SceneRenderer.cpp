#include <windows.h>

#include "RenderPassUtil.h"
#include <caligo/render/GBufferPass.h>
#include <caligo/render/LightingPass.h>
#include <caligo/render/MeshGeometry.h>
#include <caligo/render/PostProcessor.h>
#include <caligo/render/RenderGraph.h>
#include <caligo/render/RhiBuffer.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiInputLayout.h>
#include <caligo/render/RhiPipeline.h>
#include <caligo/render/RhiShader.h>
#include <caligo/render/RhiTexture.h>
#include <caligo/render/SceneRenderer.h>
#include <caligo/render/ShadowMap.h>
#include <caligo/render/TaaRenderer.h>
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
constexpr math::Vec3 kBackground{0.020f, 0.025f, 0.035f};

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

constexpr RhiVertexElement kGridElements[] = {
    {"POSITION", 0, RhiVertexFormat::Float32x3, 0},
};

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

float Halton(uint32_t index, uint32_t base) noexcept {
  float result = 0.0f;
  float fraction = 1.0f;
  while (index > 0) {
    fraction /= static_cast<float>(base);
    result += fraction * static_cast<float>(index % base);
    index /= base;
  }
  return result;
}

Result<void> CreateTargets(RhiDevice& device, uint32_t width, uint32_t height,
                           SceneRenderer::Impl& impl);

Result<void> DrawGrid(RhiDevice& device, SceneRenderer::Impl& impl,
                      const math::Mat4& viewProjection, const RhiTexture& target,
                      const RhiTexture& depth);

}

struct SceneRenderer::Impl {
  std::unique_ptr<RhiShader> gridShader;
  std::unique_ptr<RhiInputLayout> gridLayout;
  std::unique_ptr<RhiPipeline> gridPipeline;
  std::unique_ptr<RhiBuffer> boxVertices;
  std::unique_ptr<RhiBuffer> boxIndices;
  std::unique_ptr<RhiBuffer> gridVertices;
  std::unique_ptr<RhiBuffer> gridConstants;
  ShadowMap shadowMap;
  GBufferPass gbuffer;
  LightingPass lighting;
  PostProcessor post;
  TaaRenderer taa;
  RenderGraph graph;
  std::unique_ptr<RhiTexture> color;
  uint32_t gridVertexCount = 0;
  uint32_t frameIndex = 0;
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
  impl.color = std::move(color).Value();
  impl.width = width;
  impl.height = height;
  return {};
}

Result<void> Initialize(RhiDevice& device, SceneRenderer::Impl& impl) {
  Result<void> shaders = detail::CreateShader(device, kGridShader, impl.gridShader);
  if (!shaders) {
    return shaders.GetError();
  }

  auto gridLayout = RhiInputLayout::Create(device, *impl.gridShader, kGridElements);
  if (!gridLayout) {
    return gridLayout.GetError();
  }
  impl.gridLayout = std::move(gridLayout).Value();

  RhiPipelineDesc gridDescription;
  gridDescription.cullBack = false;
  gridDescription.depthWrite = false;
  Result<void> pipelines = detail::CreatePipeline(device, gridDescription, impl.gridPipeline);
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

  auto gridConstants = RhiBuffer::Create(device, RhiBufferKind::Constant, nullptr,
                                         static_cast<uint32_t>(sizeof(GridConstants)));
  if (!gridConstants) {
    return gridConstants.GetError();
  }
  impl.gridConstants = std::move(gridConstants).Value();

  impl.initialized = true;
  return {};
}

Result<void> DrawGrid(RhiDevice& device, SceneRenderer::Impl& impl,
                      const math::Mat4& viewProjection, const RhiTexture& target,
                      const RhiTexture& depth) {
  auto* context = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  auto* targetView = static_cast<ID3D11RenderTargetView*>(target.RenderTargetView());
  auto* depthView = static_cast<ID3D11DepthStencilView*>(depth.DepthStencilView());
  if (context == nullptr || targetView == nullptr || depthView == nullptr) {
    return Error{"grid pass has no live targets", 1};
  }

  context->OMSetRenderTargets(1, &targetView, depthView);
  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(target.Width());
  viewport.Height = static_cast<float>(target.Height());
  viewport.MaxDepth = 1.0f;
  context->RSSetViewports(1, &viewport);

  detail::BindPipeline(context, *impl.gridShader, *impl.gridLayout, *impl.gridPipeline);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  ID3D11Buffer* vertexBuffer = static_cast<ID3D11Buffer*>(impl.gridVertices->Handle());
  UINT stride = static_cast<UINT>(sizeof(float) * 3);
  UINT offset = 0;
  context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
  context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
  ID3D11Buffer* constantBuffer = static_cast<ID3D11Buffer*>(impl.gridConstants->Handle());
  context->VSSetConstantBuffers(0, 1, &constantBuffer);
  context->PSSetConstantBuffers(0, 1, &constantBuffer);

  GridConstants constants;
  constants.viewProjection = viewProjection;
  constants.gridColor[0] = 0.20f;
  constants.gridColor[1] = 0.24f;
  constants.gridColor[2] = 0.30f;
  constants.gridColor[3] = 1.0f;
  const Result<void> updated =
      impl.gridConstants->Update(device, &constants, static_cast<uint32_t>(sizeof(constants)));
  if (!updated) {
    return updated.GetError();
  }
  context->Draw(impl.gridVertexCount, 0);

  context->OMSetRenderTargets(0, nullptr, nullptr);
  return {};
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

  MeshGeometry mesh;
  mesh.vertices = m_impl->boxVertices.get();
  mesh.vertexStride = static_cast<uint32_t>(sizeof(MeshVertex));
  mesh.indices = m_impl->boxIndices.get();
  mesh.indexCount = static_cast<uint32_t>(std::size(kCubeIndices));

  ++m_impl->frameIndex;
  const float jitterX = (Halton(m_impl->frameIndex, 2) - 0.5f) * 2.0f / static_cast<float>(width);
  const float jitterY = (Halton(m_impl->frameIndex, 3) - 0.5f) * 2.0f / static_cast<float>(height);
  math::Mat4 projection = camera.projection;
  projection._31 += jitterX;
  projection._32 -= jitterY;
  const math::Mat4 viewProjection = math::Multiply(camera.view, projection);
  const math::Mat4 inverseViewProjection = math::Inverse(viewProjection);
  const math::Mat4 stableViewProjection = math::Multiply(camera.view, camera.projection);
  const math::Mat4 stableInverseViewProjection = math::Inverse(stableViewProjection);

  RenderGraph& graph = m_impl->graph;
  graph.Reset();

  RenderGraphTextureDesc colorDescription;
  colorDescription.width = width;
  colorDescription.height = height;
  colorDescription.format = RhiFormat::Rgba16Float;
  const RenderGraph::ResourceId albedo = graph.CreateTexture(colorDescription);
  const RenderGraph::ResourceId normal = graph.CreateTexture(colorDescription);
  const RenderGraph::ResourceId hdr = graph.CreateTexture(colorDescription);

  RenderGraphTextureDesc depthDescription;
  depthDescription.width = width;
  depthDescription.height = height;
  depthDescription.format = RhiFormat::Depth32Float;
  depthDescription.depthStencil = true;
  const RenderGraph::ResourceId depth = graph.CreateTexture(depthDescription);
  const RenderGraph::ResourceId finalColor = graph.ImportTexture(*m_impl->color);

  graph.AddPass([this, &device, mesh, items, light](RenderGraphContext&) {
    return m_impl->shadowMap.Render(device, mesh, items, light.direction);
  });

  const RenderGraph::PassId gbufferPass =
      graph.AddPass([this, &device, mesh, items, viewProjection, albedo, normal,
                     depth](RenderGraphContext& context) {
        return m_impl->gbuffer.Render(device, mesh, items, viewProjection, context.Texture(albedo),
                                      context.Texture(normal), context.Texture(depth));
      });
  graph.Write(gbufferPass, albedo);
  graph.Write(gbufferPass, normal);
  graph.Write(gbufferPass, depth);

  const RenderGraph::PassId lightingPass =
      graph.AddPass([this, &device, light, albedo, normal, depth, hdr, inverseViewProjection, width,
                     height](RenderGraphContext& context) {
        LightingInputs inputs;
        inputs.albedo = context.Texture(albedo).ShaderResourceView();
        inputs.normal = context.Texture(normal).ShaderResourceView();
        inputs.depth = context.Texture(depth).ShaderResourceView();
        inputs.shadowMap = m_impl->shadowMap.ResourceView();
        inputs.shadowSampler = m_impl->shadowMap.Sampler();

        LightingSettings settings;
        settings.inverseViewProjection = inverseViewProjection;
        settings.lightViewProjection = m_impl->shadowMap.LightViewProjection();
        settings.lightDirection = SafeLightDirection(light.direction);
        settings.lightColor = light.color;
        settings.background = kBackground;
        settings.shadowTexel = m_impl->shadowMap.TexelSize();
        settings.shadowBias = kShadowBias;

        return m_impl->lighting.Render(device, inputs, settings,
                                       context.Texture(hdr).RenderTargetView(), width, height);
      });
  graph.Read(lightingPass, albedo);
  graph.Read(lightingPass, normal);
  graph.Read(lightingPass, depth);
  graph.Write(lightingPass, hdr);

  const RenderGraph::PassId gridPass = graph.AddPass([this, &device, viewProjection, hdr,
                                                      depth](RenderGraphContext& context) {
    return DrawGrid(device, *m_impl, viewProjection, context.Texture(hdr), context.Texture(depth));
  });
  graph.Read(gridPass, depth);
  graph.Write(gridPass, hdr);

  const RenderGraph::PassId taaPass =
      graph.AddPass([this, &device, hdr, depth, stableInverseViewProjection, stableViewProjection,
                     width, height](RenderGraphContext& context) {
        return m_impl->taa.Apply(device, context.Texture(hdr).ShaderResourceView(),
                                 context.Texture(depth).ShaderResourceView(),
                                 stableInverseViewProjection, stableViewProjection, width, height);
      });
  graph.Read(taaPass, hdr);
  graph.Read(taaPass, depth);

  const RenderGraph::PassId postPass =
      graph.AddPass([this, &device, finalColor, width, height](RenderGraphContext& context) {
        return m_impl->post.Apply(device, m_impl->taa.ResultView(),
                                  context.Texture(finalColor).RenderTargetView(), width, height);
      });
  graph.Write(postPass, finalColor);

  const Result<void> executed = graph.Execute(device);
  if (!executed) {
    return executed.GetError();
  }
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

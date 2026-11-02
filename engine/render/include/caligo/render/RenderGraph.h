#pragma once

#include <caligo/core/Result.h>
#include <caligo/render/RhiTypes.h>
#include <cstdint>
#include <functional>
#include <memory>

namespace caligo::render {

class RhiDevice;
class RhiTexture;

struct RenderGraphTextureDesc {
  uint32_t width = 0;
  uint32_t height = 0;
  RhiFormat format = RhiFormat::Rgba8Unorm;
  bool depthStencil = false;
};

class RenderGraphContext final {
public:
  [[nodiscard]] RhiDevice& Device() const noexcept;
  [[nodiscard]] const RhiTexture& Texture(uint32_t resource) const;

private:
  friend class RenderGraph;

  RenderGraphContext(RhiDevice& device, const RhiTexture* const* textures) noexcept;

  RhiDevice* m_device = nullptr;
  const RhiTexture* const* m_textures = nullptr;
};

class RenderGraph final {
public:
  using ResourceId = uint32_t;
  using PassId = uint32_t;
  using PassCallback = std::function<Result<void>(RenderGraphContext&)>;

  struct Impl;

  RenderGraph();
  ~RenderGraph();

  RenderGraph(const RenderGraph&) = delete;
  RenderGraph& operator=(const RenderGraph&) = delete;
  RenderGraph(RenderGraph&&) = delete;
  RenderGraph& operator=(RenderGraph&&) = delete;

  void Reset();

  [[nodiscard]] ResourceId ImportTexture(const RhiTexture& texture);
  [[nodiscard]] ResourceId CreateTexture(const RenderGraphTextureDesc& desc);

  PassId AddPass(PassCallback callback);
  void Read(PassId pass, ResourceId resource);
  void Write(PassId pass, ResourceId resource);

  [[nodiscard]] Result<void> Execute(RhiDevice& device);

private:
  std::unique_ptr<Impl> m_impl;
};

}

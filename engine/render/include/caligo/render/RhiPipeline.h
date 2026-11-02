#pragma once

#include <caligo/core/Result.h>
#include <memory>

namespace caligo::render {

class RhiDevice;

struct RhiPipelineDesc {
  bool wireframe = false;
  bool depthTest = true;
  bool depthWrite = true;
  bool cullBack = true;
};

class RhiPipeline final {
public:
  struct Impl;

  [[nodiscard]] static Result<std::unique_ptr<RhiPipeline>> Create(RhiDevice& device,
                                                                   const RhiPipelineDesc& desc);

  RhiPipeline();
  ~RhiPipeline();

  RhiPipeline(const RhiPipeline&) = delete;
  RhiPipeline& operator=(const RhiPipeline&) = delete;
  RhiPipeline(RhiPipeline&&) = delete;
  RhiPipeline& operator=(RhiPipeline&&) = delete;

  [[nodiscard]] void* RasterizerState() const noexcept;
  [[nodiscard]] void* DepthStencilState() const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}
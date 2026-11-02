#pragma once

#include <caligo/core/Result.h>
#include <caligo/render/RhiTypes.h>
#include <memory>

namespace caligo::render {

class RhiDevice;

class RhiShader final {
public:
  struct Impl;

  [[nodiscard]] static Result<std::unique_ptr<RhiShader>> Create(RhiDevice& device,
                                                                 const RhiShaderDesc& desc);

  RhiShader();
  ~RhiShader();

  RhiShader(const RhiShader&) = delete;
  RhiShader& operator=(const RhiShader&) = delete;
  RhiShader(RhiShader&&) = delete;
  RhiShader& operator=(RhiShader&&) = delete;

  [[nodiscard]] void* VertexShaderHandle() const noexcept;
  [[nodiscard]] void* PixelShaderHandle() const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}

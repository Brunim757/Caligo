#pragma once

#include <caligo/core/Result.h>
#include <caligo/render/RhiTypes.h>
#include <memory>

namespace caligo::render {

class RhiDevice;

class RhiSampler final {
public:
  struct Impl;

  [[nodiscard]] static Result<std::unique_ptr<RhiSampler>> Create(RhiDevice& device,
                                                                  const RhiSamplerDesc& desc);

  RhiSampler();
  ~RhiSampler();

  RhiSampler(const RhiSampler&) = delete;
  RhiSampler& operator=(const RhiSampler&) = delete;
  RhiSampler(RhiSampler&&) = delete;
  RhiSampler& operator=(RhiSampler&&) = delete;

  [[nodiscard]] void* Handle() const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}
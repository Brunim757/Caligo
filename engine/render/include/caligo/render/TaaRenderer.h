#pragma once

#include <caligo/core/Math.h>
#include <caligo/core/Result.h>
#include <cstdint>
#include <memory>

namespace caligo::render {

class RhiDevice;

class TaaRenderer final {
public:
  struct Impl;

  TaaRenderer();
  ~TaaRenderer();

  TaaRenderer(const TaaRenderer&) = delete;
  TaaRenderer& operator=(const TaaRenderer&) = delete;
  TaaRenderer(TaaRenderer&&) = delete;
  TaaRenderer& operator=(TaaRenderer&&) = delete;

  [[nodiscard]] Result<void> Apply(RhiDevice& device, void* colorView, void* depthView,
                                   const math::Mat4& inverseViewProjection,
                                   const math::Mat4& viewProjection, uint32_t width,
                                   uint32_t height);
  [[nodiscard]] void* ResultView() const noexcept;
  [[nodiscard]] bool HasHistory() const noexcept;
  void Reset() noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}
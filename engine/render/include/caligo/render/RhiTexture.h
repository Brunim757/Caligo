#pragma once

#include <caligo/core/Result.h>
#include <caligo/render/RhiTypes.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace caligo::render {

class RhiDevice;

class RhiTexture final {
public:
  struct Impl;

  [[nodiscard]] static Result<std::unique_ptr<RhiTexture>> Create(RhiDevice& device,
                                                                  const RhiTextureDesc& desc);

  RhiTexture();
  ~RhiTexture();

  RhiTexture(const RhiTexture&) = delete;
  RhiTexture& operator=(const RhiTexture&) = delete;
  RhiTexture(RhiTexture&&) = delete;
  RhiTexture& operator=(RhiTexture&&) = delete;

  [[nodiscard]] uint32_t Width() const noexcept;
  [[nodiscard]] uint32_t Height() const noexcept;
  [[nodiscard]] RhiFormat Format() const noexcept;
  [[nodiscard]] void* RenderTargetView() const noexcept;
  [[nodiscard]] void* DepthStencilView() const noexcept;
  [[nodiscard]] void* ShaderResourceView() const noexcept;
  [[nodiscard]] Result<std::vector<uint8_t>> ReadPixels() const;

private:
  std::unique_ptr<Impl> m_impl;
};

}

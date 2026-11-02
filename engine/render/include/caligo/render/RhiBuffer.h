#pragma once

#include <caligo/core/Result.h>
#include <caligo/render/RhiTypes.h>
#include <cstdint>
#include <memory>

namespace caligo::render {

class RhiDevice;

class RhiBuffer final {
public:
  struct Impl;

  [[nodiscard]] static Result<std::unique_ptr<RhiBuffer>>
  Create(RhiDevice& device, RhiBufferKind kind, const void* data, uint32_t sizeBytes);

  RhiBuffer();
  ~RhiBuffer();

  RhiBuffer(const RhiBuffer&) = delete;
  RhiBuffer& operator=(const RhiBuffer&) = delete;
  RhiBuffer(RhiBuffer&&) = delete;
  RhiBuffer& operator=(RhiBuffer&&) = delete;

  [[nodiscard]] Result<void> Update(RhiDevice& device, const void* data, uint32_t sizeBytes);
  [[nodiscard]] void* Handle() const noexcept;
  [[nodiscard]] uint32_t SizeBytes() const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}
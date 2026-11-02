#pragma once

#include <caligo/core/Result.h>
#include <cstdint>
#include <memory>

namespace caligo::render {

class RhiDevice final {
public:
  struct Impl;

  [[nodiscard]] static Result<std::unique_ptr<RhiDevice>> Create(bool forceWarp);

  RhiDevice();
  ~RhiDevice();

  RhiDevice(const RhiDevice&) = delete;
  RhiDevice& operator=(const RhiDevice&) = delete;
  RhiDevice(RhiDevice&&) = delete;
  RhiDevice& operator=(RhiDevice&&) = delete;

  [[nodiscard]] void* DeviceHandle() const noexcept;
  [[nodiscard]] void* ContextHandle() const noexcept;
  [[nodiscard]] bool IsWarp() const noexcept;
  [[nodiscard]] const char* DriverName() const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}

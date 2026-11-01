#pragma once

#include <caligo/core/Result.h>
#include <cstdint>
#include <memory>

namespace caligo::render {

class GraphicsDevice final {
public:
  struct Impl;

  GraphicsDevice();
  ~GraphicsDevice();

  GraphicsDevice(const GraphicsDevice&) = delete;
  GraphicsDevice& operator=(const GraphicsDevice&) = delete;
  GraphicsDevice(GraphicsDevice&&) = delete;
  GraphicsDevice& operator=(GraphicsDevice&&) = delete;

  [[nodiscard]] Result<void> Create(void* nativeWindow, uint32_t width, uint32_t height);
  void Destroy() noexcept;
  [[nodiscard]] Result<void> Resize(uint32_t width, uint32_t height);
  void BeginFrame(float red, float green, float blue, float alpha) noexcept;
  [[nodiscard]] Result<void> EndFrame();

  [[nodiscard]] bool IsValid() const noexcept;
  [[nodiscard]] void* DeviceHandle() const noexcept;
  [[nodiscard]] void* ContextHandle() const noexcept;
  [[nodiscard]] uint32_t Width() const noexcept;
  [[nodiscard]] uint32_t Height() const noexcept;

private:
  [[nodiscard]] Result<void> CreateRenderTarget();

  std::unique_ptr<Impl> m_impl;
};

}

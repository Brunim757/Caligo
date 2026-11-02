#pragma once

#include <caligo/core/Result.h>
#include <caligo/render/RhiTypes.h>
#include <memory>
#include <span>

namespace caligo::render {

class RhiDevice;
class RhiShader;

class RhiInputLayout final {
public:
  struct Impl;

  [[nodiscard]] static Result<std::unique_ptr<RhiInputLayout>>
  Create(RhiDevice& device, RhiShader& shader, std::span<const RhiVertexElement> elements);

  RhiInputLayout();
  ~RhiInputLayout();

  RhiInputLayout(const RhiInputLayout&) = delete;
  RhiInputLayout& operator=(const RhiInputLayout&) = delete;
  RhiInputLayout(RhiInputLayout&&) = delete;
  RhiInputLayout& operator=(RhiInputLayout&&) = delete;

  [[nodiscard]] void* Handle() const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}
#pragma once

#include <caligo/core/Result.h>
#include <cstdint>
#include <memory>

namespace caligo::render {

class RhiDevice;

class PostProcessor final {
public:
  struct Impl;

  PostProcessor();
  ~PostProcessor();

  PostProcessor(const PostProcessor&) = delete;
  PostProcessor& operator=(const PostProcessor&) = delete;
  PostProcessor(PostProcessor&&) = delete;
  PostProcessor& operator=(PostProcessor&&) = delete;

  [[nodiscard]] Result<void> Apply(RhiDevice& device, void* sourceView, void* outputView,
                                   uint32_t width, uint32_t height);

private:
  std::unique_ptr<Impl> m_impl;
};

}
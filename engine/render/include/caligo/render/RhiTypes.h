#pragma once

#include <cstdint>

namespace caligo::render {

enum class RhiFormat : uint8_t { Rgba8Unorm, Depth32Float };

struct RhiTextureDesc {
  uint32_t width = 0;
  uint32_t height = 0;
  RhiFormat format = RhiFormat::Rgba8Unorm;
  bool renderTarget = false;
  bool depthStencil = false;
};

struct RhiShaderDesc {
  const char* source = nullptr;
  const char* vertexEntry = "VsMain";
  const char* pixelEntry = "PsMain";
};

}

#pragma once

#include <cstdint>

namespace caligo::render {

enum class RhiFormat : uint8_t { Rgba8Unorm, Depth32Float };

enum class RhiVertexFormat : uint8_t { Float32x2, Float32x3, Float32x4 };

enum class RhiBufferKind : uint8_t { Vertex, Index, Constant };

enum class RhiFilter : uint8_t { Point, Linear };

enum class RhiAddressMode : uint8_t { Clamp, Border };

struct RhiSamplerDesc {
  RhiFilter filter = RhiFilter::Linear;
  RhiAddressMode address = RhiAddressMode::Clamp;
  bool comparison = false;
  bool borderWhite = false;
};

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

struct RhiVertexElement {
  const char* semantic = "POSITION";
  uint32_t semanticIndex = 0;
  RhiVertexFormat format = RhiVertexFormat::Float32x3;
  uint32_t offset = 0;
};

}
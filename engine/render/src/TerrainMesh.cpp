#include <windows.h>

#include <algorithm>
#include <caligo/render/RhiBuffer.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/TerrainMesh.h>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace caligo::render {

namespace {

constexpr uint32_t kMinResolution = 2;
constexpr uint32_t kMaxResolution = 255;
constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

struct TerrainVertex {
  float position[3];
  float normal[3];
  float color[3];
};

void HashBytes(uint64_t& hash, const void* data, size_t bytes) noexcept {
  const auto* bytesPointer = static_cast<const unsigned char*>(data);
  for (size_t index = 0; index < bytes; ++index) {
    hash ^= static_cast<uint64_t>(bytesPointer[index]);
    hash *= kFnvPrime;
  }
}

void HashFloats(uint64_t& hash, std::span<const float> values) noexcept {
  for (const float value : values) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    HashBytes(hash, &bits, sizeof(bits));
  }
}

uint64_t Fingerprint(const TerrainSurface& surface) noexcept {
  uint64_t hash = kFnvOffset;
  HashBytes(hash, &surface.resolution, sizeof(surface.resolution));
  HashBytes(hash, &surface.extent, sizeof(surface.extent));
  HashFloats(hash, surface.heights);
  HashFloats(hash, surface.layerWeights);
  for (const math::Vec3& color : surface.layerColors) {
    const float components[3] = {color.x, color.y, color.z};
    HashFloats(hash, components);
  }
  return hash;
}

bool SurfaceIsValid(const TerrainSurface& surface) noexcept {
  if (surface.resolution < kMinResolution || surface.resolution > kMaxResolution) {
    return false;
  }
  if (!std::isfinite(surface.extent) || !(surface.extent > 0.0f)) {
    return false;
  }
  if (surface.layerColors.empty()) {
    return false;
  }
  const size_t sampleCount = static_cast<size_t>(surface.resolution) * surface.resolution;
  return surface.heights.size() == sampleCount &&
         surface.layerWeights.size() == sampleCount * surface.layerColors.size();
}

math::Vec3 BlendColor(const TerrainSurface& surface, size_t sample, size_t layerCount) noexcept {
  float total = 0.0f;
  math::Vec3 blended{0.0f, 0.0f, 0.0f};
  for (size_t layer = 0; layer < layerCount; ++layer) {
    const float weight = surface.layerWeights[sample * layerCount + layer];
    const math::Vec3& color = surface.layerColors[layer];
    total += weight;
    blended.x += color.x * weight;
    blended.y += color.y * weight;
    blended.z += color.z * weight;
  }
  if (total <= 1.0e-5f) {
    return surface.layerColors.front();
  }
  return math::Vec3{blended.x / total, blended.y / total, blended.z / total};
}

}

struct TerrainMesh::Impl {
  std::unique_ptr<RhiBuffer> vertices;
  std::unique_ptr<RhiBuffer> indices;
  std::vector<TerrainVertex> cpuVertices;
  std::vector<uint16_t> cpuIndices;
  TerrainGeometry geometry;
  uint64_t fingerprint = 0;
  bool hasSurface = false;
};

TerrainMesh::TerrainMesh() : m_impl(std::make_unique<Impl>()) {}

TerrainMesh::~TerrainMesh() = default;

Result<void> TerrainMesh::Update(RhiDevice& device, const TerrainSurface& surface) {
  if (!SurfaceIsValid(surface)) {
    m_impl->hasSurface = false;
    m_impl->geometry = TerrainGeometry{};
    return {};
  }

  m_impl->geometry.world = surface.world;

  const uint64_t fingerprint = Fingerprint(surface);
  if (m_impl->hasSurface && fingerprint == m_impl->fingerprint) {
    return {};
  }

  const uint32_t resolution = surface.resolution;
  const size_t layerCount = surface.layerColors.size();
  const size_t sampleCount = static_cast<size_t>(resolution) * resolution;
  const float spacing = surface.extent / static_cast<float>(resolution - 1);
  const float half = surface.extent * 0.5f;

  std::vector<TerrainVertex>& vertices = m_impl->cpuVertices;
  vertices.resize(sampleCount);
  math::Vec3 minimum{0.0f, 0.0f, 0.0f};
  math::Vec3 maximum{0.0f, 0.0f, 0.0f};
  bool hasBounds = false;

  for (uint32_t z = 0; z < resolution; ++z) {
    for (uint32_t x = 0; x < resolution; ++x) {
      const size_t sample = static_cast<size_t>(z) * resolution + x;
      const float height = surface.heights[sample];
      const float left = surface.heights[static_cast<size_t>(z) * resolution + (x > 0 ? x - 1 : x)];
      const float right =
          surface.heights[static_cast<size_t>(z) * resolution + (x + 1 < resolution ? x + 1 : x)];
      const float down = surface.heights[(z > 0 ? z - 1 : z) * resolution + x];
      const float up = surface.heights[(z + 1 < resolution ? z + 1 : z) * resolution + x];

      const math::Vec3 normal =
          math::Normalize(math::Vec3{left - right, 2.0f * spacing, down - up});
      const math::Vec3 color = BlendColor(surface, sample, layerCount);

      TerrainVertex& vertex = vertices[sample];
      vertex.position[0] = -half + static_cast<float>(x) * spacing;
      vertex.position[1] = height;
      vertex.position[2] = -half + static_cast<float>(z) * spacing;
      vertex.normal[0] = normal.x;
      vertex.normal[1] = normal.y;
      vertex.normal[2] = normal.z;
      vertex.color[0] = color.x;
      vertex.color[1] = color.y;
      vertex.color[2] = color.z;

      const math::Vec3 position{vertex.position[0], vertex.position[1], vertex.position[2]};
      if (!hasBounds) {
        minimum = position;
        maximum = position;
        hasBounds = true;
      } else {
        minimum.x = std::min(minimum.x, position.x);
        minimum.y = std::min(minimum.y, position.y);
        minimum.z = std::min(minimum.z, position.z);
        maximum.x = std::max(maximum.x, position.x);
        maximum.y = std::max(maximum.y, position.y);
        maximum.z = std::max(maximum.z, position.z);
      }
    }
  }

  std::vector<uint16_t>& indices = m_impl->cpuIndices;
  indices.resize(static_cast<size_t>(resolution - 1) * (resolution - 1) * 6);
  size_t cursor = 0;
  for (uint32_t z = 0; z + 1 < resolution; ++z) {
    for (uint32_t x = 0; x + 1 < resolution; ++x) {
      const uint16_t topLeft = static_cast<uint16_t>(z * resolution + x);
      const uint16_t topRight = static_cast<uint16_t>(z * resolution + x + 1);
      const uint16_t bottomLeft = static_cast<uint16_t>((z + 1) * resolution + x);
      const uint16_t bottomRight = static_cast<uint16_t>((z + 1) * resolution + x + 1);
      indices[cursor++] = topLeft;
      indices[cursor++] = topRight;
      indices[cursor++] = bottomRight;
      indices[cursor++] = topLeft;
      indices[cursor++] = bottomRight;
      indices[cursor++] = bottomLeft;
    }
  }

  auto vertexBuffer =
      RhiBuffer::Create(device, RhiBufferKind::Vertex, vertices.data(),
                        static_cast<uint32_t>(vertices.size() * sizeof(TerrainVertex)));
  if (!vertexBuffer) {
    return vertexBuffer.GetError();
  }
  auto indexBuffer = RhiBuffer::Create(device, RhiBufferKind::Index, indices.data(),
                                       static_cast<uint32_t>(indices.size() * sizeof(uint16_t)));
  if (!indexBuffer) {
    return indexBuffer.GetError();
  }
  m_impl->vertices = std::move(vertexBuffer).Value();
  m_impl->indices = std::move(indexBuffer).Value();

  m_impl->geometry.vertices = m_impl->vertices.get();
  m_impl->geometry.vertexStride = static_cast<uint32_t>(sizeof(TerrainVertex));
  m_impl->geometry.indices = m_impl->indices.get();
  m_impl->geometry.indexCount = static_cast<uint32_t>(indices.size());
  m_impl->geometry.boundsMinimum = minimum;
  m_impl->geometry.boundsMaximum = maximum;

  m_impl->fingerprint = fingerprint;
  m_impl->hasSurface = true;
  return {};
}

const TerrainGeometry& TerrainMesh::Geometry() const noexcept {
  return m_impl->geometry;
}

bool TerrainMesh::HasSurface() const noexcept {
  return m_impl->hasSurface;
}

}

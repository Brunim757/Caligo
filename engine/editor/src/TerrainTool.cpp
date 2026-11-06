#include "TerrainTool.h"

#include <algorithm>
#include <caligo/core/Log.h>
#include <cmath>
#include <cstdint>

namespace caligo::editor {

namespace {

constexpr float kCursorRadius = 10.0f;

constexpr const char* kLayerNames[scene::kTerrainLayers] = {"Grama", "Terra", "Rocha", "Neve"};

struct Ray {
  math::Vec3 origin{};
  math::Vec3 direction{0.0f, 0.0f, 1.0f};
};

float ClampHeight(float height) noexcept {
  return math::Clamp(height, -scene::kTerrainMaxHeight, scene::kTerrainMaxHeight);
}

bool ProjectPoint(const math::Mat4& view, const math::Mat4& projection, const math::Vec3& point,
                  const ImVec2& origin, const ImVec2& size, ImVec2& result) {
  const DirectX::XMVECTOR position = DirectX::XMVectorSet(point.x, point.y, point.z, 1.0f);
  const DirectX::XMVECTOR viewPosition =
      DirectX::XMVector4Transform(position, DirectX::XMLoadFloat4x4(&view));
  const DirectX::XMVECTOR clip =
      DirectX::XMVector4Transform(viewPosition, DirectX::XMLoadFloat4x4(&projection));
  const float w = DirectX::XMVectorGetW(clip);
  if (!std::isfinite(w) || w <= 0.0001f) {
    return false;
  }
  const float ndcX = DirectX::XMVectorGetX(clip) / w;
  const float ndcY = DirectX::XMVectorGetY(clip) / w;
  const float ndcZ = DirectX::XMVectorGetZ(clip) / w;
  if (ndcZ < 0.0f || ndcZ > 1.0f) {
    return false;
  }
  result.x = origin.x + (ndcX * 0.5f + 0.5f) * size.x;
  result.y = origin.y + (1.0f - (ndcY * 0.5f + 0.5f)) * size.y;
  return true;
}

Ray BuildRay(const math::Mat4& view, const math::Mat4& projection, const ImVec2& origin,
             const ImVec2& size) {
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const float ndcX = (mouse.x - origin.x) / size.x * 2.0f - 1.0f;
  const float ndcY = 1.0f - (mouse.y - origin.y) / size.y * 2.0f;
  const math::Mat4 inverse = math::Inverse(math::Multiply(view, projection));
  const math::Vec3 nearPoint = math::TransformPoint(inverse, math::Vec3{ndcX, ndcY, 0.0f});
  const math::Vec3 farPoint = math::TransformPoint(inverse, math::Vec3{ndcX, ndcY, 1.0f});
  return Ray{nearPoint, math::Normalize(math::Subtract(farPoint, nearPoint))};
}

bool ClipSlab(float origin, float direction, float minimum, float maximum, float& enter,
              float& exit) {
  if (std::abs(direction) < 1.0e-6f) {
    return origin >= minimum && origin <= maximum;
  }
  float near = (minimum - origin) / direction;
  float far = (maximum - origin) / direction;
  if (near > far) {
    std::swap(near, far);
  }
  enter = std::max(enter, near);
  exit = std::min(exit, far);
  return enter <= exit;
}

bool RaycastTerrain(const scene::Terrain& terrain, const math::Mat4& world, const Ray& ray,
                    math::Vec3& localHit) {
  const math::Mat4 inverseWorld = math::Inverse(world);
  const math::Vec3 origin = math::TransformPoint(inverseWorld, ray.origin);
  const math::Vec3 ahead = math::TransformPoint(inverseWorld, math::Add(ray.origin, ray.direction));
  const math::Vec3 delta = math::Subtract(ahead, origin);
  const float deltaLength = math::Length(delta);
  if (!(deltaLength > 1.0e-6f) || !std::isfinite(deltaLength)) {
    return false;
  }
  const math::Vec3 direction = math::Normalize(delta);

  const float half = terrain.extent * 0.5f;
  float enter = 0.0f;
  float exit = 1.0e9f;
  if (!ClipSlab(origin.x, direction.x, -half, half, enter, exit) ||
      !ClipSlab(origin.z, direction.z, -half, half, enter, exit)) {
    return false;
  }
  if (exit < 0.0f) {
    return false;
  }
  enter = std::max(enter, 0.0f);

  const float spacing = scene::TerrainSpacing(terrain);
  const float step = std::max(spacing * 0.5f, 0.01f);
  for (float t = enter; t <= exit; t += step) {
    const math::Vec3 point = math::Add(origin, math::Scale(direction, t));
    if (point.y <= scene::TerrainHeightAt(terrain, point.x, point.z)) {
      float low = std::max(enter, t - step);
      float high = t;
      for (int iteration = 0; iteration < 20; ++iteration) {
        const float middle = (low + high) * 0.5f;
        const math::Vec3 sample = math::Add(origin, math::Scale(direction, middle));
        if (sample.y <= scene::TerrainHeightAt(terrain, sample.x, sample.z)) {
          high = middle;
        } else {
          low = middle;
        }
      }
      localHit = math::Add(origin, math::Scale(direction, high));
      return true;
    }
  }
  return false;
}

void PaintSample(scene::Terrain& terrain, size_t sample, uint32_t layer, float amount) noexcept {
  const size_t base = sample * scene::kTerrainLayers;
  terrain.layerWeights[base + layer] += amount;
  float total = 0.0f;
  for (uint32_t index = 0; index < scene::kTerrainLayers; ++index) {
    total += terrain.layerWeights[base + index];
  }
  if (total > 1.0e-5f) {
    for (uint32_t index = 0; index < scene::kTerrainLayers; ++index) {
      terrain.layerWeights[base + index] /= total;
    }
  } else {
    terrain.layerWeights[base] = 1.0f;
  }
}

void ApplyBrush(scene::Terrain& terrain, const TerrainBrush& brush, const math::Vec3& localHit,
                float deltaSeconds) {
  const float spacing = scene::TerrainSpacing(terrain);
  if (!(spacing > 0.0f) || brush.layer >= scene::kTerrainLayers) {
    return;
  }
  const float half = terrain.extent * 0.5f;
  const float radius = std::max(brush.radius, spacing);
  const float radiusSquared = radius * radius;
  const int resolution = static_cast<int>(terrain.resolution);
  const int minX = std::max(0, static_cast<int>((localHit.x - radius + half) / spacing));
  const int maxX =
      std::min(resolution - 1, static_cast<int>((localHit.x + radius + half) / spacing) + 1);
  const int minZ = std::max(0, static_cast<int>((localHit.z - radius + half) / spacing));
  const int maxZ =
      std::min(resolution - 1, static_cast<int>((localHit.z + radius + half) / spacing) + 1);

  std::vector<float> source;
  if (brush.mode == TerrainBrushMode::Smooth) {
    source = terrain.heights;
  }
  const float rate = math::Clamp(brush.strength * deltaSeconds, 0.0f, 1.0f);

  for (int z = minZ; z <= maxZ; ++z) {
    for (int x = minX; x <= maxX; ++x) {
      const float localX = -half + static_cast<float>(x) * spacing;
      const float localZ = -half + static_cast<float>(z) * spacing;
      const float deltaX = localX - localHit.x;
      const float deltaZ = localZ - localHit.z;
      const float distanceSquared = deltaX * deltaX + deltaZ * deltaZ;
      if (distanceSquared > radiusSquared) {
        continue;
      }
      const float distance = std::sqrt(distanceSquared);
      float falloff = 1.0f - distance / radius;
      falloff = falloff * falloff * (3.0f - 2.0f * falloff);
      const size_t sample = static_cast<size_t>(z) * terrain.resolution + static_cast<size_t>(x);
      switch (brush.mode) {
      case TerrainBrushMode::Raise:
        terrain.heights[sample] =
            ClampHeight(terrain.heights[sample] + brush.strength * falloff * deltaSeconds);
        break;
      case TerrainBrushMode::Lower:
        terrain.heights[sample] =
            ClampHeight(terrain.heights[sample] - brush.strength * falloff * deltaSeconds);
        break;
      case TerrainBrushMode::Flatten:
        terrain.heights[sample] = math::Lerp(terrain.heights[sample], localHit.y, rate * falloff);
        break;
      case TerrainBrushMode::Smooth: {
        const size_t row = static_cast<size_t>(z) * terrain.resolution;
        const size_t previousX = static_cast<size_t>(x > 0 ? x - 1 : x);
        const size_t nextX = static_cast<size_t>(x + 1 < resolution ? x + 1 : x);
        const size_t previousZ = static_cast<size_t>(z > 0 ? z - 1 : z) * terrain.resolution;
        const size_t nextZ =
            static_cast<size_t>(z + 1 < resolution ? z + 1 : z) * terrain.resolution;
        const float average = (source[row + previousX] + source[row + nextX] +
                               source[previousZ + x] + source[nextZ + x]) *
                              0.25f;
        terrain.heights[sample] = math::Lerp(source[sample], average, rate * falloff);
        break;
      }
      case TerrainBrushMode::Paint:
        PaintSample(terrain, sample, brush.layer, rate * falloff);
        break;
      }
    }
  }
}

void DrawBrushControls(EditorState& state) {
  const float width = ImGui::GetContentRegionAvail().x * 0.64f;
  ImGui::Checkbox("Editar terreno", &state.terrainBrush.active);
  ImGui::SetItemTooltip("Desenha no relevo com o botão esquerdo do mouse.");

  int mode = static_cast<int>(state.terrainBrush.mode);
  ImGui::SetNextItemWidth(width);
  if (ImGui::Combo("Modo", &mode, "Subir\0Baixar\0Suavizar\0Aplanar\0Pintar\0")) {
    state.terrainBrush.mode = static_cast<TerrainBrushMode>(mode);
  }
  ImGui::SetNextItemWidth(width);
  ImGui::DragFloat("Raio", &state.terrainBrush.radius, 0.2f, 0.5f, 512.0f, "%.1f m");
  ImGui::SetNextItemWidth(width);
  ImGui::DragFloat("Força", &state.terrainBrush.strength, 0.2f, 0.1f, 40.0f, "%.1f");
  if (state.terrainBrush.mode == TerrainBrushMode::Paint) {
    int layer = static_cast<int>(state.terrainBrush.layer);
    ImGui::SetNextItemWidth(width);
    if (ImGui::Combo("Camada", &layer, kLayerNames, static_cast<int>(scene::kTerrainLayers))) {
      state.terrainBrush.layer = static_cast<uint32_t>(layer);
    }
  }
}

}

void DrawTerrainToolbar(EditorState& state) {
  const bool enabled = state.scene.FindTerrain(state.selected) != nullptr;
  ImGui::BeginDisabled(state.play.active || !enabled);
  const bool active = state.terrainBrush.active;
  if (active) {
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 118, 201, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 139, 232, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 104, 178, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
  }
  if (ImGui::Button(active ? "Pincel (B)" : "Terreno (B)", ImVec2(0.0f, Pixels(state, 30.0f)))) {
    state.terrainBrush.active = !state.terrainBrush.active;
    state.terrainBrush.stroking = false;
  }
  if (active) {
    ImGui::PopStyleColor(4);
  }
  ImGui::SetItemTooltip("Ativa o pincel de terreno.");
  ImGui::EndDisabled();
}

void DrawTerrainInspector(EditorState& state) {
  scene::Entity* entity = state.scene.Find(state.selected);
  if (entity == nullptr) {
    return;
  }
  ImGui::SeparatorText("Terreno");
  ImGui::BeginDisabled(state.play.active);
  const scene::Terrain* terrain = state.scene.FindTerrain(entity->id);
  if (terrain == nullptr) {
    if (ImGui::Button("Adicionar terreno")) {
      if (state.scene.AddTerrain(entity->id, 65, 64.0f)) {
        state.terrainBrush.active = true;
      } else {
        CALIGO_LOG_WARN("não foi possível adicionar o terreno à entidade {}", entity->id);
      }
    }
    ImGui::EndDisabled();
    return;
  }

  ImGui::TextDisabled("Resolução %u x %u", terrain->resolution, terrain->resolution);
  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.64f);
  float extent = terrain->extent;
  if (ImGui::DragFloat("Extensão", &extent, 1.0f, scene::kTerrainMinExtent,
                       scene::kTerrainMaxExtent, "%.1f m")) {
    if (scene::Terrain* editable = state.scene.EditTerrain(entity->id)) {
      editable->extent = extent;
    }
  }
  ImGui::PopItemWidth();

  for (uint32_t layer = 0; layer < scene::kTerrainLayers; ++layer) {
    ImGui::PushID(static_cast<int>(layer));
    float color[3] = {terrain->layerColors[layer].x, terrain->layerColors[layer].y,
                      terrain->layerColors[layer].z};
    if (ImGui::ColorEdit3(kLayerNames[layer], color)) {
      if (scene::Terrain* editable = state.scene.EditTerrain(entity->id)) {
        editable->layerColors[layer] = math::Vec3{color[0], color[1], color[2]};
      }
    }
    ImGui::PopID();
  }

  DrawBrushControls(state);
  if (ImGui::Button("Remover terreno")) {
    if (state.scene.RemoveTerrain(entity->id)) {
      state.terrainBrush.active = false;
      state.terrainBrush.stroking = false;
    } else {
      CALIGO_LOG_WARN("não foi possível remover o terreno da entidade {}", entity->id);
    }
  }
  ImGui::EndDisabled();
}

std::vector<render::TerrainSurface> BuildTerrainSurfaces(const EditorState& state) {
  std::vector<render::TerrainSurface> surfaces;
  for (const scene::Entity& entity : state.scene.Entities()) {
    if (!entity.terrain.has_value()) {
      continue;
    }
    const scene::Terrain& terrain = entity.terrain.value();
    render::TerrainSurface surface;
    surface.resolution = terrain.resolution;
    surface.extent = terrain.extent;
    surface.heights = terrain.heights;
    surface.layerWeights = terrain.layerWeights;
    surface.layerColors = terrain.layerColors;
    surface.world = state.scene.WorldMatrix(entity.id);
    surfaces.push_back(surface);
  }
  return surfaces;
}

void EditTerrainWithBrush(EditorState& state, const math::Mat4& view, const math::Mat4& projection,
                          const ImVec2& origin, const ImVec2& size, bool canvasHovered) {
  if (!state.terrainBrush.active || state.play.active) {
    state.terrainBrush.stroking = false;
    return;
  }
  scene::Entity* entity = state.scene.Find(state.selected);
  const scene::Terrain* terrain = entity != nullptr ? state.scene.FindTerrain(entity->id) : nullptr;
  if (entity == nullptr || terrain == nullptr ||
      terrain->resolution < scene::kTerrainMinResolution) {
    state.terrainBrush.stroking = false;
    return;
  }

  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    state.terrainBrush.stroking = false;
  } else if (canvasHovered) {
    state.terrainBrush.stroking = true;
  }

  const math::Mat4 world = state.scene.WorldMatrix(entity->id);
  const Ray ray = BuildRay(view, projection, origin, size);
  math::Vec3 localHit{};
  if (!RaycastTerrain(*terrain, world, ray, localHit)) {
    return;
  }

  const math::Vec3 worldHit = math::TransformPoint(world, localHit);
  ImVec2 screen{};
  if (ProjectPoint(view, projection, worldHit, origin, size, screen)) {
    ImGui::GetWindowDrawList()->AddCircle(screen, Pixels(state, kCursorRadius),
                                          IM_COL32(255, 176, 64, 230), 0, Pixels(state, 2.0f));
  }

  if (!state.terrainBrush.stroking) {
    return;
  }
  scene::Terrain* editable = state.scene.EditTerrain(entity->id);
  if (editable == nullptr) {
    return;
  }
  const float delta = math::Clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
  ApplyBrush(*editable, state.terrainBrush, localHit, delta);
}

}

#include "EditorPanels.h"
#include <ImGuizmo.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <imgui.h>
#include <string>
#include <vector>

namespace caligo::editor {

namespace {

constexpr float kMarkerRadius = 4.0f;
constexpr float kMarkerSelectRadius = 14.0f;
constexpr float kMinCameraDistance = 0.75f;
constexpr float kMaxCameraDistance = 3000.0f;
constexpr float kGridSize = 10.0f;
constexpr float kOverlayHeight = 34.0f;

constexpr std::array<math::Vec3, 8> kBoundsCorners = {
    math::Vec3{-0.5f, -0.5f, -0.5f}, math::Vec3{0.5f, -0.5f, -0.5f}, math::Vec3{0.5f, 0.5f, -0.5f},
    math::Vec3{-0.5f, 0.5f, -0.5f},  math::Vec3{-0.5f, -0.5f, 0.5f}, math::Vec3{0.5f, -0.5f, 0.5f},
    math::Vec3{0.5f, 0.5f, 0.5f},    math::Vec3{-0.5f, 0.5f, 0.5f}};

constexpr std::array<std::array<int, 2>, 12> kBoundsEdges = {
    std::array<int, 2>{0, 1}, std::array<int, 2>{1, 2}, std::array<int, 2>{2, 3},
    std::array<int, 2>{3, 0}, std::array<int, 2>{4, 5}, std::array<int, 2>{5, 6},
    std::array<int, 2>{6, 7}, std::array<int, 2>{7, 4}, std::array<int, 2>{0, 4},
    std::array<int, 2>{1, 5}, std::array<int, 2>{2, 6}, std::array<int, 2>{3, 7}};

struct ScreenMarker {
  scene::EntityId id = scene::kNoEntity;
  ImVec2 position{0.0f, 0.0f};
  bool selected = false;
};

ImGuizmo::OPERATION ToImGuizmoOperation(GizmoOperation operation) noexcept {
  switch (operation) {
  case GizmoOperation::Rotate:
    return ImGuizmo::ROTATE;
  case GizmoOperation::Scale:
    return ImGuizmo::SCALE;
  case GizmoOperation::Translate:
  default:
    return ImGuizmo::TRANSLATE;
  }
}

ImGuizmo::MODE ToImGuizmoSpace(GizmoSpace space) noexcept {
  return space == GizmoSpace::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
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

std::vector<ScreenMarker> BuildMarkers(const EditorState& state, const math::Mat4& view,
                                       const math::Mat4& projection, const ImVec2& origin,
                                       const ImVec2& size) {
  std::vector<ScreenMarker> markers;
  markers.reserve(state.scene.Entities().size());
  for (const scene::Entity& entity : state.scene.Entities()) {
    const math::Vec3 worldPosition =
        math::TransformPoint(state.scene.WorldMatrix(entity.id), math::Vec3{0.0f, 0.0f, 0.0f});
    ImVec2 screen{};
    if (!ProjectPoint(view, projection, worldPosition, origin, size, screen)) {
      continue;
    }
    ScreenMarker marker;
    marker.id = entity.id;
    marker.position = screen;
    marker.selected = entity.id == state.selected;
    markers.push_back(marker);
  }
  return markers;
}

const ScreenMarker* FindMarkerUnderMouse(const std::vector<ScreenMarker>& markers,
                                         float selectRadius) {
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const ScreenMarker* closest = nullptr;
  float closestDistance = selectRadius;
  for (const ScreenMarker& marker : markers) {
    const float deltaX = marker.position.x - mouse.x;
    const float deltaY = marker.position.y - mouse.y;
    const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    if (distance < closestDistance) {
      closestDistance = distance;
      closest = &marker;
    }
  }
  return closest;
}

void PanCamera(EditorState& state, const ImVec2& delta) {
  const math::Vec3 eye = state.camera.Position();
  const math::Vec3 forward = math::Normalize(math::Subtract(state.camera.target, eye));
  math::Vec3 up{0.0f, 1.0f, 0.0f};
  if (std::abs(math::Dot(forward, up)) > 0.999f) {
    up = math::Vec3{0.0f, 0.0f, 1.0f};
  }
  const math::Vec3 right = math::Normalize(math::Cross(forward, up));
  const math::Vec3 cameraUp = math::Normalize(math::Cross(right, forward));
  const float speed = std::clamp(state.camera.distance, 1.0f, 30.0f) * 0.0018f;
  state.camera.target =
      math::Add(state.camera.target, math::Add(math::Scale(right, -delta.x * speed),
                                               math::Scale(cameraUp, delta.y * speed)));
}

void HandleNavigation(EditorState& state, bool hovered) {
  ImGuiIO& io = ImGui::GetIO();
  if (hovered) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      state.orbiting = true;
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
      state.panning = true;
    }
    if (io.MouseWheel != 0.0f) {
      state.camera.distance = std::clamp(state.camera.distance * std::pow(0.86f, io.MouseWheel),
                                         kMinCameraDistance, kMaxCameraDistance);
    }
  }

  if (state.orbiting) {
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
      state.orbiting = false;
    } else {
      state.camera.yaw -= io.MouseDelta.x * 0.32f;
      state.camera.pitch = std::clamp(state.camera.pitch + io.MouseDelta.y * 0.32f, -89.0f, 89.0f);
    }
  }
  if (state.panning) {
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
      state.panning = false;
    } else {
      PanCamera(state, io.MouseDelta);
    }
  }
}

void FocusSelection(EditorState& state) {
  const scene::Entity* entity = state.scene.Find(state.selected);
  if (entity == nullptr) {
    state.camera.Focus(math::Vec3{0.0f, 0.0f, 0.0f});
    return;
  }
  state.camera.Focus(
      math::TransformPoint(state.scene.WorldMatrix(entity->id), math::Vec3{0.0f, 0.0f, 0.0f}));
}

void HandleShortcuts(EditorState& state, bool hovered) {
  if (!hovered || ImGui::GetIO().WantTextInput) {
    return;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
    state.operation = GizmoOperation::Translate;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
    state.operation = GizmoOperation::Rotate;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
    state.operation = GizmoOperation::Scale;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
    FocusSelection(state);
  }
}

bool ToolButton(EditorState& state, const char* label, bool active) {
  if (active) {
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 118, 201, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 139, 232, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 104, 178, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
  }
  const bool pressed = ImGui::Button(label, ImVec2(0.0f, Pixels(state, 30.0f)));
  if (active) {
    ImGui::PopStyleColor(4);
  }
  return pressed;
}

void DrawToolbar(EditorState& state) {
  if (ToolButton(state, "Mover (W)", state.operation == GizmoOperation::Translate)) {
    state.operation = GizmoOperation::Translate;
  }
  ImGui::SameLine();
  if (ToolButton(state, "Girar (E)", state.operation == GizmoOperation::Rotate)) {
    state.operation = GizmoOperation::Rotate;
  }
  ImGui::SameLine();
  if (ToolButton(state, "Escalar (R)", state.operation == GizmoOperation::Scale)) {
    state.operation = GizmoOperation::Scale;
  }
  ImGui::SameLine();
  if (ToolButton(state, state.space == GizmoSpace::Local ? "Espaço local" : "Espaço global",
                 state.space == GizmoSpace::World)) {
    state.space = state.space == GizmoSpace::Local ? GizmoSpace::World : GizmoSpace::Local;
  }
  ImGui::SetItemTooltip("Alterna entre eixos locais e globais.");
  ImGui::SameLine();
  ImGui::BeginDisabled(state.selected == scene::kNoEntity);
  if (ToolButton(state, "Focar (F)", false)) {
    FocusSelection(state);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::TextDisabled("Ctrl: encaixe");
}

void ManipulateSelection(EditorState& state, const math::Mat4& view, const math::Mat4& projection) {
  scene::Entity* entity = state.scene.Find(state.selected);
  if (entity == nullptr) {
    return;
  }

  const math::Mat4 world = state.scene.WorldMatrix(entity->id);
  math::Mat4 manipulated = math::Transpose(world);
  float snap[3]{0.0f, 0.0f, 0.0f};
  const float* snapPointer = nullptr;
  if (ImGui::GetIO().KeyCtrl) {
    if (state.operation == GizmoOperation::Translate) {
      snap[0] = snap[1] = snap[2] = 0.5f;
    } else if (state.operation == GizmoOperation::Rotate) {
      snap[0] = snap[1] = snap[2] = 15.0f;
    } else {
      snap[0] = snap[1] = snap[2] = 0.1f;
    }
    snapPointer = snap;
  }

  if (!ImGuizmo::Manipulate(&view._11, &projection._11, ToImGuizmoOperation(state.operation),
                            ToImGuizmoSpace(state.space), &manipulated._11, nullptr, snapPointer)) {
    return;
  }

  math::Mat4 local = math::Transpose(manipulated);
  if (entity->parent != scene::kNoEntity) {
    const math::Mat4 parentInverse = math::Inverse(state.scene.WorldMatrix(entity->parent));
    local = math::Multiply(local, parentInverse);
  }

  scene::Transform transform;
  if (!math::DecomposeTransform(local, transform.position, transform.rotation, transform.scale)) {
    return;
  }
  if (std::abs(transform.scale.x) < 1.0e-5f || std::abs(transform.scale.y) < 1.0e-5f ||
      std::abs(transform.scale.z) < 1.0e-5f) {
    return;
  }
  if (!state.scene.SetTransform(entity->id, transform)) {
    return;
  }
  state.eulerOwner = scene::kNoEntity;
}

void DrawEntityBounds(EditorState& state, const math::Mat4& view, const math::Mat4& projection,
                      const ImVec2& origin, const ImVec2& size, scene::EntityId id, ImU32 color,
                      float thickness) {
  if (state.scene.Find(id) == nullptr) {
    return;
  }
  const math::Mat4 world = state.scene.WorldMatrix(id);
  std::array<ImVec2, kBoundsCorners.size()> projected{};
  for (size_t index = 0; index < kBoundsCorners.size(); ++index) {
    const math::Vec3 point = math::TransformPoint(world, kBoundsCorners[index]);
    if (!ProjectPoint(view, projection, point, origin, size, projected[index])) {
      return;
    }
  }

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  for (const std::array<int, 2>& edge : kBoundsEdges) {
    const size_t first = static_cast<size_t>(edge[0]);
    const size_t second = static_cast<size_t>(edge[1]);
    drawList->AddLine(projected[first], projected[second], color, thickness);
  }
}

void DrawMarkers(EditorState& state, const math::Mat4& view, const math::Mat4& projection,
                 const std::vector<ScreenMarker>& markers, const ScreenMarker* hovered,
                 const ImVec2& origin, const ImVec2& size) {
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const float markerRadius = Pixels(state, kMarkerRadius);
  const float selectedRadius = Pixels(state, 6.5f);
  const float hoveredRadius = Pixels(state, 5.5f);
  const float ringOffset = Pixels(state, 1.5f);
  const float ringThickness = Pixels(state, 1.5f);
  const float lineThickness = Pixels(state, 1.0f);

  for (const ScreenMarker& marker : markers) {
    const scene::Entity* entity = state.scene.Find(marker.id);
    if (entity == nullptr) {
      continue;
    }
    const math::Vec3 worldPosition =
        math::TransformPoint(state.scene.WorldMatrix(marker.id), math::Vec3{0.0f, 0.0f, 0.0f});
    if (worldPosition.y > 0.01f) {
      ImVec2 ground{};
      if (ProjectPoint(view, projection, math::Vec3{worldPosition.x, 0.0f, worldPosition.z}, origin,
                       size, ground)) {
        drawList->AddLine(marker.position, ground, IM_COL32(70, 84, 104, 110), lineThickness);
      }
    }

    const bool highlighted = marker.selected || hovered == &marker;
    const ImU32 color = marker.selected ? IM_COL32(255, 176, 64, 255)
                        : highlighted   ? IM_COL32(125, 210, 255, 255)
                                        : IM_COL32(86, 168, 228, 210);
    const float radius =
        marker.selected ? selectedRadius : (highlighted ? hoveredRadius : markerRadius);
    drawList->AddCircleFilled(marker.position, radius, color);
    drawList->AddCircle(marker.position, radius + ringOffset, IM_COL32(12, 14, 18, 220), 0,
                        ringThickness);

    if (highlighted) {
      const ImVec2 labelPosition{marker.position.x + Pixels(state, 11.0f),
                                 marker.position.y - Pixels(state, 10.0f)};
      const ImVec2 labelSize = ImGui::CalcTextSize(entity->name.c_str());
      const float paddingX = Pixels(state, 5.0f);
      const float paddingY = Pixels(state, 3.0f);
      drawList->AddRectFilled(ImVec2(labelPosition.x - paddingX, labelPosition.y - paddingY),
                              ImVec2(labelPosition.x + labelSize.x + paddingX,
                                     labelPosition.y + labelSize.y + paddingY),
                              IM_COL32(15, 18, 23, 210), Pixels(state, 3.0f));
      drawList->AddText(labelPosition, IM_COL32(232, 237, 245, 255), entity->name.c_str());
    }
  }
}

void HandleSelection(EditorState& state, const std::vector<ScreenMarker>& markers, bool hovered,
                     bool gizmoBusy, float selectRadius) {
  if (!hovered || gizmoBusy || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    return;
  }
  const ScreenMarker* marker = FindMarkerUnderMouse(markers, selectRadius);
  SelectEntity(state, marker != nullptr ? marker->id : scene::kNoEntity);
}

void DrawOverlay(EditorState& state, const ImVec2& origin, const ImVec2& size) {
  const float overlayHeight = Pixels(state, kOverlayHeight);
  if (size.x < Pixels(state, 360.0f) || size.y < overlayHeight + Pixels(state, 20.0f)) {
    return;
  }
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const scene::Entity* entity = state.scene.Find(state.selected);
  std::string label = "Nenhuma entidade selecionada";
  if (entity != nullptr) {
    const math::Vec3 position =
        math::TransformPoint(state.scene.WorldMatrix(entity->id), math::Vec3{0.0f, 0.0f, 0.0f});
    label = std::format("{}   X {:.1f}   Y {:.1f}   Z {:.1f}", entity->name, position.x, position.y,
                        position.z);
  }
  const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
  const ImVec2 labelMin{origin.x + Pixels(state, 12.0f),
                        origin.y + size.y - overlayHeight + Pixels(state, 6.0f)};
  drawList->AddRectFilled(
      ImVec2(labelMin.x - Pixels(state, 7.0f), labelMin.y - Pixels(state, 4.0f)),
      ImVec2(labelMin.x + labelSize.x + Pixels(state, 7.0f),
             labelMin.y + labelSize.y + Pixels(state, 4.0f)),
      IM_COL32(13, 16, 20, 205), Pixels(state, 4.0f));
  drawList->AddText(labelMin, IM_COL32(218, 224, 233, 255), label.c_str());

  const char* hint = "Botão direito: orbitar | Botão do meio: deslocar | Roda: zoom";
  const ImVec2 hintSize = ImGui::CalcTextSize(hint);
  if (size.x > hintSize.x + labelSize.x + Pixels(state, 70.0f)) {
    const ImVec2 hintPosition{origin.x + size.x - hintSize.x - Pixels(state, 14.0f),
                              origin.y + size.y - overlayHeight + Pixels(state, 6.0f)};
    drawList->AddRectFilled(
        ImVec2(hintPosition.x - Pixels(state, 7.0f), hintPosition.y - Pixels(state, 4.0f)),
        ImVec2(hintPosition.x + hintSize.x + Pixels(state, 7.0f),
               hintPosition.y + hintSize.y + Pixels(state, 4.0f)),
        IM_COL32(13, 16, 20, 205), Pixels(state, 4.0f));
    drawList->AddText(hintPosition, IM_COL32(156, 166, 180, 255), hint);
  }
}

}

void DrawViewport(EditorState& state) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  const bool open = ImGui::Begin("Cena");
  ImGui::PopStyleVar();
  if (!open) {
    ImGui::End();
    return;
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 9.0f));
  ImGui::BeginChild("##viewport-toolbar", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  DrawToolbar(state);
  ImGui::EndChild();
  ImGui::PopStyleVar();

  const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
  const ImVec2 canvasSize{std::max(ImGui::GetContentRegionAvail().x, 1.0f),
                          std::max(ImGui::GetContentRegionAvail().y, 1.0f)};
  ImGui::InvisibleButton("##viewport-canvas", canvasSize,
                         ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                             ImGuiButtonFlags_MouseButtonMiddle);
  const bool hovered = ImGui::IsItemHovered();

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->PushClipRect(
      canvasOrigin, ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y), true);
  drawList->AddRectFilled(canvasOrigin,
                          ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y),
                          IM_COL32(22, 25, 31, 255));

  HandleNavigation(state, hovered);
  HandleShortcuts(state, hovered);

  const float aspect = canvasSize.y > 0.0f ? canvasSize.x / canvasSize.y : 1.0f;
  const math::Mat4 view = state.camera.ViewMatrix();
  const math::Mat4 projection = state.camera.ProjectionMatrix(aspect);
  const math::Mat4 viewTransposed = math::Transpose(view);
  const math::Mat4 projectionTransposed = math::Transpose(projection);

  ImGuizmo::SetRect(canvasOrigin.x, canvasOrigin.y, canvasSize.x, canvasSize.y);
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::SetDrawlist(drawList);

  const math::Mat4 identity = math::Identity();
  ImGuizmo::DrawGrid(&viewTransposed._11, &projectionTransposed._11, &identity._11, kGridSize);

  const std::vector<ScreenMarker> markers =
      BuildMarkers(state, view, projection, canvasOrigin, canvasSize);
  const float selectRadius = Pixels(state, kMarkerSelectRadius);
  const ScreenMarker* hoveredMarker =
      hovered ? FindMarkerUnderMouse(markers, selectRadius) : nullptr;

  if (state.selected != scene::kNoEntity) {
    DrawEntityBounds(state, view, projection, canvasOrigin, canvasSize, state.selected,
                     IM_COL32(255, 176, 64, 150), Pixels(state, 1.5f));
  }
  if (hoveredMarker != nullptr && hoveredMarker->id != state.selected) {
    DrawEntityBounds(state, view, projection, canvasOrigin, canvasSize, hoveredMarker->id,
                     IM_COL32(125, 210, 255, 120), Pixels(state, 1.2f));
  }
  DrawMarkers(state, view, projection, markers, hoveredMarker, canvasOrigin, canvasSize);

  ManipulateSelection(state, viewTransposed, projectionTransposed);

  const bool gizmoBusy =
      state.scene.Find(state.selected) != nullptr && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());
  HandleSelection(state, markers, hovered, gizmoBusy, selectRadius);
  DrawOverlay(state, canvasOrigin, canvasSize);

  drawList->PopClipRect();
  ImGui::End();
}

}

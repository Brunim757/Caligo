#include <algorithm>
#include <caligo/editor/ViewportCanvas.h>

namespace caligo::editor {

ViewportCanvas ResolveViewportCanvas(const ImVec2& cursorScreenPosition,
                                     const ImVec2& availableSize) noexcept {
  ViewportCanvas canvas;
  canvas.origin = cursorScreenPosition;
  canvas.size = ImVec2{std::max(availableSize.x, 1.0f), std::max(availableSize.y, 1.0f)};

  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const bool inside = mouse.x >= canvas.origin.x && mouse.x < canvas.origin.x + canvas.size.x &&
                      mouse.y >= canvas.origin.y && mouse.y < canvas.origin.y + canvas.size.y;
  canvas.hovered = inside && ImGui::IsWindowHovered();
  return canvas;
}

}
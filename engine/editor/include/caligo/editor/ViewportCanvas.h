#pragma once

#include <imgui.h>

namespace caligo::editor {

struct ViewportCanvas {
  ImVec2 origin{};
  ImVec2 size{};
  bool hovered = false;
};

[[nodiscard]] ViewportCanvas ResolveViewportCanvas(const ImVec2& cursorScreenPosition,
                                                   const ImVec2& availableSize) noexcept;

}
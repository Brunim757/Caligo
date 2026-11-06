#pragma once

#include "EditorState.h"
#include <caligo/render/TerrainMesh.h>
#include <imgui.h>
#include <vector>

namespace caligo::editor {

void DrawTerrainToolbar(EditorState& state);
void DrawTerrainInspector(EditorState& state);
[[nodiscard]] std::vector<render::TerrainSurface> BuildTerrainSurfaces(const EditorState& state);
void EditTerrainWithBrush(EditorState& state, const math::Mat4& view, const math::Mat4& projection,
                          const ImVec2& origin, const ImVec2& size, bool canvasHovered);

}

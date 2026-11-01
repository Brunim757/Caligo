#pragma once

#include "EditorState.h"

#include <filesystem>
#include <string>

namespace caligo::editor {

void DrawMainMenu(EditorState& state);
void DrawDockSpace(EditorState& state);
void ResetLayout(EditorState& state);
void DrawHierarchy(EditorState& state);
void DrawInspector(EditorState& state);
void DrawViewport(EditorState& state);
void DrawConsole(EditorState& state);
void DrawModals(EditorState& state);

void SelectEntity(EditorState& state, scene::EntityId id);
void CreateEntity(EditorState& state);
void DeleteSelectedEntity(EditorState& state);
void NewScene(EditorState& state);
void OpenScene(EditorState& state);
[[nodiscard]] bool SaveScene(EditorState& state, bool saveAs);
[[nodiscard]] bool LoadSceneFile(EditorState& state, const std::filesystem::path& path);
void RequestAction(EditorState& state, PendingAction action);
void ExecuteAction(EditorState& state, PendingAction action);
void ShowError(EditorState& state, std::string title, std::string message);
void UpdateWindowTitle(EditorState& state);

void ApplyTheme(float dpiScale);
[[nodiscard]] bool InitializeImGui(EditorState& state);
void ShutdownImGui(EditorState& state) noexcept;

}

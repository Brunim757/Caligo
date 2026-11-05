#include "EditorPanels.h"
#include <caligo/core/Log.h>
#include <caligo/platform/File.h>
#include <caligo/platform/FileDialog.h>
#include <caligo/platform/System.h>
#include <caligo/scene/SceneSerialization.h>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace caligo::editor {

namespace {

constexpr char kSceneFilter[] = "Cenas Caligo (*.cscene)\0*.cscene\0Todos os arquivos (*.*)\0*.*\0";

std::string_view SceneFilter() {
  return std::string_view(kSceneFilter, sizeof(kSceneFilter) - 1);
}

std::string PathFileName(const std::filesystem::path& path) {
  const std::u8string text = path.filename().u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

std::string PathText(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

std::filesystem::path InitialSceneDirectory(const EditorState& state) {
  if (!state.scenePath.empty()) {
    return state.scenePath.parent_path();
  }
  const Result<std::filesystem::path> executable = platform::ExecutableDirectory();
  if (!executable) {
    return {};
  }
  const std::filesystem::path scenes = executable.Value() / "content" / "scenes";
  std::error_code error;
  if (std::filesystem::is_directory(scenes, error)) {
    return scenes;
  }
  return executable.Value();
}

std::string SuggestedSceneName(const EditorState& state) {
  if (!state.scenePath.empty()) {
    return PathFileName(state.scenePath);
  }
  return "exemplo.cscene";
}

}

void ShowError(EditorState& state, std::string title, std::string message) {
  state.errorTitle = std::move(title);
  state.errorMessage = std::move(message);
  state.errorPopupRequested = true;
  CALIGO_LOG_ERROR("{}: {}", state.errorTitle, state.errorMessage);
}

void SelectEntity(EditorState& state, scene::EntityId id) {
  if (state.selected == id) {
    return;
  }
  state.selected = id;
  state.nameOwner = scene::kNoEntity;
  state.eulerOwner = scene::kNoEntity;
}

void NewScene(EditorState& state) {
  StopPlay(state);
  state.scene = scene::Scene{};
  state.selected = state.scene.CreateEntity("Mundo");
  state.scenePath.clear();
  state.savedRevision = state.scene.Revision();
  state.nameOwner = scene::kNoEntity;
  state.eulerOwner = scene::kNoEntity;
  state.camera = Camera{};
  CALIGO_LOG_INFO("nova cena criada");
}

bool LoadSceneFile(EditorState& state, const std::filesystem::path& path) {
  StopPlay(state);
  Result<std::string> text = platform::ReadTextFile(path);
  if (!text) {
    ShowError(state, "Não foi possível abrir a cena", text.GetError().message);
    return false;
  }

  Result<scene::Scene> loaded = scene::LoadSceneFromString(text.Value());
  if (!loaded) {
    ShowError(state, "A cena não pôde ser lida", loaded.GetError().message);
    return false;
  }

  state.scene = std::move(loaded).Value();
  state.scenePath = path;
  state.selected =
      state.scene.Entities().empty() ? scene::kNoEntity : state.scene.Entities().front().id;
  state.savedRevision = state.scene.Revision();
  state.nameOwner = scene::kNoEntity;
  state.eulerOwner = scene::kNoEntity;
  state.camera = Camera{};
  CALIGO_LOG_INFO("cena carregada: {}", PathText(path));
  return true;
}

void OpenScene(EditorState& state) {
  const Result<std::optional<std::filesystem::path>> chosen = platform::ShowOpenFileDialog(
      "Abrir cena", SceneFilter(), InitialSceneDirectory(state), state.window->NativeHandle());
  if (!chosen) {
    ShowError(state, "Não foi possível abrir o seletor de arquivos", chosen.GetError().message);
    return;
  }
  if (!chosen.Value().has_value()) {
    return;
  }
  static_cast<void>(LoadSceneFile(state, *chosen.Value()));
}

bool SaveScene(EditorState& state, bool saveAs) {
  std::filesystem::path path = state.scenePath;
  if (saveAs || path.empty()) {
    const Result<std::optional<std::filesystem::path>> chosen = platform::ShowSaveFileDialog(
        "Salvar cena", SceneFilter(), "cscene", SuggestedSceneName(state),
        InitialSceneDirectory(state), state.window->NativeHandle());
    if (!chosen) {
      ShowError(state, "Não foi possível abrir o seletor de arquivos", chosen.GetError().message);
      return false;
    }
    if (!chosen.Value().has_value()) {
      return false;
    }
    path = *chosen.Value();
  }

  Result<std::string> text = scene::SaveSceneToString(state.scene);
  if (!text) {
    ShowError(state, "A cena não pôde ser salva", text.GetError().message);
    return false;
  }

  const Result<void> written = platform::WriteTextFile(path, text.Value());
  if (!written) {
    ShowError(state, "Não foi possível gravar a cena", written.GetError().message);
    return false;
  }

  state.scenePath = path;
  state.savedRevision = state.scene.Revision();
  CALIGO_LOG_INFO("cena salva: {}", PathText(path));
  return true;
}

void RequestAction(EditorState& state, PendingAction action) {
  if (action == PendingAction::None) {
    return;
  }
  if (state.play.active) {
    StopPlay(state);
  }
  if (IsDirty(state)) {
    state.pendingAction = action;
    state.discardPopupRequested = true;
    return;
  }
  state.pendingAction = action;
  ExecuteAction(state, action);
}

void ExecuteAction(EditorState& state, PendingAction action) {
  state.pendingAction = PendingAction::None;
  switch (action) {
  case PendingAction::NewScene:
    NewScene(state);
    break;
  case PendingAction::OpenScene:
    OpenScene(state);
    break;
  case PendingAction::Quit:
    state.allowClose = true;
    state.window->RequestClose();
    break;
  case PendingAction::None:
    break;
  }
}

void CreateEntity(EditorState& state) {
  if (state.play.active) {
    return;
  }
  const scene::EntityId parent =
      state.scene.Find(state.selected) != nullptr ? state.selected : scene::kNoEntity;
  const scene::EntityId id = state.scene.CreateEntity("Entidade", parent);
  if (id == scene::kNoEntity) {
    ShowError(state, "Não foi possível criar a entidade",
              "A entidade pai não existe mais ou a hierarquia atingiu o limite de níveis.");
    return;
  }
  SelectEntity(state, id);
  CALIGO_LOG_INFO("entidade criada: {}", id);
}

void DeleteSelectedEntity(EditorState& state) {
  if (state.play.active) {
    return;
  }
  const scene::Entity* entity = state.scene.Find(state.selected);
  if (entity == nullptr) {
    state.selected = scene::kNoEntity;
    return;
  }
  const std::string name = entity->name;
  if (!state.scene.DestroyEntity(state.selected)) {
    ShowError(state, "Não foi possível excluir a entidade", "A entidade não existe mais na cena.");
    return;
  }
  state.selected = scene::kNoEntity;
  state.nameOwner = scene::kNoEntity;
  state.eulerOwner = scene::kNoEntity;
  CALIGO_LOG_INFO("entidade excluída: {}", name);
}

void UpdateWindowTitle(EditorState& state) {
  if (state.window == nullptr) {
    return;
  }
  std::string base;
  if (state.project.has_value()) {
    base = state.project->name;
  } else if (!state.scenePath.empty()) {
    base = PathFileName(state.scenePath);
  } else {
    base = std::string("Sem título");
  }
  const std::string title = std::format("{} — Caligo Editor{}", IsDirty(state) ? base + " *" : base,
                                        state.play.active ? " (executando)" : "");
  if (title != state.windowTitle) {
    state.windowTitle = title;
    state.window->SetTitle(state.windowTitle);
  }
}

}

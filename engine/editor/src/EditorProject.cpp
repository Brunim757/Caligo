#include "EditorPanels.h"
#include <algorithm>
#include <caligo/build/Project.h>
#include <caligo/core/Log.h>
#include <caligo/platform/File.h>
#include <caligo/platform/FileDialog.h>
#include <caligo/platform/System.h>
#include <caligo/scene/SceneSerialization.h>
#include <format>
#include <imgui.h>

namespace caligo::editor {

namespace {

std::string PathText(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

}

void EnterProject(EditorState& state) {
  if (!state.project.has_value()) {
    state.mode = EditorMode::Hub;
    return;
  }
  state.mode = EditorMode::Editor;
  if (state.play.active) {
    StopPlay(state);
  }
  state.selected = scene::kNoEntity;
  state.nameOwner = scene::kNoEntity;
  state.eulerOwner = scene::kNoEntity;
  state.camera = Camera{};
  if (!state.projectPath.empty()) {
    const std::filesystem::path scenePath =
        build::ProjectInitialScene(state.projectPath, *state.project);
    if (platform::FileExists(scenePath)) {
      LoadSceneFile(state, scenePath);
    } else {
      NewScene(state);
    }
  } else {
    NewScene(state);
  }
  UpdateWindowTitle(state);
}

void OpenProject(EditorState& state, const std::filesystem::path& path) {
  const Result<build::Project> loaded = build::LoadProjectFile(path);
  if (!loaded) {
    ShowError(state, "Não foi possível abrir o projeto", loaded.GetError().message);
    return;
  }
  state.projectPath = path;
  state.project = loaded.Value();
  RecentProject recent;
  recent.path = path;
  recent.name = loaded.Value().name;
  state.recentProjects.erase(std::remove_if(state.recentProjects.begin(),
                                            state.recentProjects.end(),
                                            [&path](const RecentProject& item) {
                                              return std::filesystem::equivalent(item.path, path);
                                            }),
                             state.recentProjects.end());
  state.recentProjects.insert(state.recentProjects.begin(), recent);
  if (state.recentProjects.size() > 10) {
    state.recentProjects.resize(10);
  }
  EnterProject(state);
  CALIGO_LOG_INFO("projeto aberto: {}", PathText(path));
}

void CreateProject(EditorState& state, const std::filesystem::path& directory,
                   const std::string& name) {
  std::error_code error;
  if (name.empty()) {
    ShowError(state, "Não foi possível criar o projeto", "O nome do projeto não pode estar vazio.");
    return;
  }
  std::filesystem::path projectDir = directory / name;
  static_cast<void>(std::filesystem::create_directories(projectDir / "content" / "scenes", error));
  static_cast<void>(std::filesystem::create_directories(projectDir / "content" / "assets", error));
  static_cast<void>(
      std::filesystem::create_directories(projectDir / "scripts" / (name + ".Gameplay"), error));
  static_cast<void>(std::filesystem::create_directories(projectDir / "out", error));

  build::Project project;
  project.name = name;
  project.projectVersion = "0.1.0";
  project.contentRoot = "content";
  project.initialScene = "scenes/inicio.cscene";
  project.gameplayProject = "scripts/" + name + ".Gameplay/" + name + ".Gameplay.csproj";
  project.gameplayAssembly = name + ".Gameplay";
  project.target = "windows-x64";
  project.output = "out";
  project.render.quality = "high";
  project.render.resolutionScale = 1.0f;

  const std::filesystem::path projectPath = projectDir / (name + ".cproject");
  const Result<void> saved = build::SaveProjectFile(projectPath, project);
  if (!saved) {
    ShowError(state, "Não foi possível criar o projeto", saved.GetError().message);
    return;
  }

  scene::Scene scene;
  scene.CreateEntity("Mundo");
  const Result<std::string> sceneText = scene::SaveSceneToString(scene);
  if (sceneText) {
    const std::filesystem::path scenePath = projectDir / "content" / "scenes" / "inicio.cscene";
    static_cast<void>(platform::WriteTextFile(scenePath, sceneText.Value()));
  }

  OpenProject(state, projectPath);
}

void DrawProjectHub(EditorState& state) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(Pixels(state, 32.0f), Pixels(state, 32.0f)));
  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus;
  ImGui::Begin("##caligo-hub", nullptr, kFlags);

  ImGui::TextUnformatted("Caligo");
  ImGui::TextDisabled("Hub de projetos");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Columns(2, nullptr, true);
  ImGui::SetColumnWidth(0, Pixels(state, 320.0f));

  ImGui::TextUnformatted("Criar projeto");
  ImGui::Spacing();
  ImGui::TextDisabled("Nome");
  ImGui::PushItemWidth(Pixels(state, 280.0f));
  static char hubName[128] = "NovoJogo";
  if (ImGui::InputText("##hubname", hubName, IM_ARRAYSIZE(hubName))) {
    state.newProjectName = hubName;
  } else if (state.newProjectName != hubName && state.newProjectName.empty()) {
    state.newProjectName = hubName;
  }
  ImGui::PopItemWidth();
  ImGui::Spacing();
  if (ImGui::Button("Criar novo projeto", ImVec2(Pixels(state, 280.0f), 0.0f))) {
    const Result<std::optional<std::filesystem::path>> chosen = platform::ShowOpenFolderDialog(
        "Selecionar pasta para o projeto", std::filesystem::path{},
        state.window != nullptr ? state.window->NativeHandle() : nullptr);
    if (chosen && chosen.Value().has_value()) {
      CreateProject(state, *chosen.Value(), state.newProjectName);
    } else if (!chosen) {
      ShowError(state, "Não foi possível abrir o seletor de pastas", chosen.GetError().message);
    }
  }
  ImGui::Spacing();
  if (ImGui::Button("Abrir projeto...", ImVec2(Pixels(state, 280.0f), 0.0f))) {
    const Result<std::optional<std::filesystem::path>> chosen = platform::ShowOpenFileDialog(
        "Abrir projeto", "Projetos Caligo (*.cproject)\0*.cproject\0Todos os arquivos (*.*)\0*.*\0",
        std::filesystem::path{}, state.window != nullptr ? state.window->NativeHandle() : nullptr);
    if (chosen && chosen.Value().has_value()) {
      OpenProject(state, *chosen.Value());
    } else if (!chosen) {
      ShowError(state, "Não foi possível abrir o seletor de arquivos", chosen.GetError().message);
    }
  }

  ImGui::NextColumn();

  ImGui::TextUnformatted("Projetos recentes");
  ImGui::Spacing();
  if (state.recentProjects.empty()) {
    ImGui::TextDisabled("Nenhum projeto recente.");
  } else {
    for (const RecentProject& recent : state.recentProjects) {
      if (ImGui::Selectable(recent.name.c_str(), false)) {
        OpenProject(state, recent.path);
      }
      ImGui::TextDisabled("  %s", PathText(recent.path).c_str());
      ImGui::Spacing();
    }
  }

  ImGui::Columns(1);
  ImGui::End();
  ImGui::PopStyleVar(3);
}

}

#include "EditorPanels.h"
#include <algorithm>
#include <caligo/build/Project.h>
#include <caligo/core/Log.h>
#include <caligo/platform/File.h>
#include <caligo/platform/System.h>
#include <caligo/scene/SceneSerialization.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace caligo::editor {

namespace {

constexpr size_t kMaxRecentProjects = 10;

std::string PathText(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

std::filesystem::path PathFromText(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  const std::u8string utf8(reinterpret_cast<const char8_t*>(text.data()), text.size());
  return std::filesystem::path(utf8);
}

std::string Trim(std::string_view text) {
  constexpr std::string_view kWhitespace = " \t\r\n";
  const size_t first = text.find_first_not_of(kWhitespace);
  if (first == std::string_view::npos) {
    return {};
  }
  const size_t last = text.find_last_not_of(kWhitespace);
  return std::string(text.substr(first, last - first + 1));
}

bool IsValidProjectName(std::string_view name) {
  if (name.empty() || name == "." || name == "..") {
    return false;
  }
  if (name.front() == ' ' || name.back() == ' ') {
    return false;
  }
  for (const char character : name) {
    const unsigned char code = static_cast<unsigned char>(character);
    if (code < 32) {
      return false;
    }
    switch (character) {
    case '<':
    case '>':
    case ':':
    case '"':
    case '/':
    case '\\':
    case '|':
    case '?':
    case '*':
      return false;
    default:
      break;
    }
  }
  return true;
}

bool SamePath(const std::filesystem::path& left, const std::filesystem::path& right) {
  std::error_code error;
  const bool equivalent = std::filesystem::equivalent(left, right, error);
  if (!error) {
    return equivalent;
  }
  return left.lexically_normal() == right.lexically_normal();
}

std::filesystem::path RecentProjectsFile() {
  const Result<std::filesystem::path> data = platform::UserDataDirectory();
  if (!data) {
    return {};
  }
  return data.Value() / "recent-projects.txt";
}

void SaveRecentProjects(const EditorState& state) {
  const std::filesystem::path file = RecentProjectsFile();
  if (file.empty()) {
    return;
  }
  std::error_code error;
  static_cast<void>(std::filesystem::create_directories(file.parent_path(), error));
  std::string text;
  for (const RecentProject& recent : state.recentProjects) {
    text += recent.name;
    text += '\t';
    text += PathText(recent.path);
    text += '\n';
  }
  const Result<void> written = platform::WriteTextFile(file, text);
  if (!written) {
    CALIGO_LOG_WARN("não foi possível salvar os projetos recentes: {}", written.GetError().message);
  }
}

void AddRecentProject(EditorState& state, const std::filesystem::path& path,
                      const std::string& name) {
  std::erase_if(state.recentProjects,
                [&path](const RecentProject& item) { return SamePath(item.path, path); });
  state.recentProjects.insert(state.recentProjects.begin(), RecentProject{path, name});
  if (state.recentProjects.size() > kMaxRecentProjects) {
    state.recentProjects.resize(kMaxRecentProjects);
  }
  SaveRecentProjects(state);
}

void LoadRecentProjects(EditorState& state) {
  state.recentProjects.clear();
  const std::filesystem::path file = RecentProjectsFile();
  if (file.empty() || !platform::FileExists(file)) {
    return;
  }
  const Result<std::string> text = platform::ReadTextFile(file);
  if (!text) {
    CALIGO_LOG_WARN("não foi possível ler os projetos recentes: {}", text.GetError().message);
    return;
  }
  std::string_view remaining = text.Value();
  while (!remaining.empty() && state.recentProjects.size() < kMaxRecentProjects) {
    const size_t end = remaining.find('\n');
    std::string_view line = remaining.substr(0, end);
    if (end == std::string_view::npos) {
      remaining = {};
    } else {
      remaining.remove_prefix(end + 1);
    }
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (line.empty()) {
      continue;
    }
    std::string name;
    std::string_view pathText = line;
    const size_t tab = line.find('\t');
    if (tab != std::string_view::npos) {
      name.assign(line.substr(0, tab));
      pathText = line.substr(tab + 1);
    }
    const std::filesystem::path path = PathFromText(pathText);
    if (path.empty() || !platform::FileExists(path)) {
      continue;
    }
    if (name.empty()) {
      name = PathText(path.stem());
    }
    state.recentProjects.push_back(RecentProject{path, name});
  }
}

}

void InitializeProjects(EditorState& state) {
  constexpr char kDefaultName[] = "NovoJogo";
  std::snprintf(state.newProjectName.data(), state.newProjectName.size(), "%s", kDefaultName);
  const Result<std::filesystem::path> documents = platform::DocumentsDirectory();
  if (documents) {
    state.newProjectLocation = documents.Value() / "Caligo";
  }
  LoadRecentProjects(state);
  state.mode = EditorMode::Hub;
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
      static_cast<void>(LoadSceneFile(state, scenePath));
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
  AddRecentProject(state, path, loaded.Value().name);
  EnterProject(state);
  CALIGO_LOG_INFO("projeto aberto: {}", PathText(path));
}

void CreateProject(EditorState& state, const std::filesystem::path& directory,
                   const std::string& name) {
  const std::string projectName = Trim(name);
  if (!IsValidProjectName(projectName)) {
    ShowError(state, "Não foi possível criar o projeto",
              "Use um nome sem caracteres especiais ou espaços nas pontas.");
    return;
  }
  if (directory.empty()) {
    ShowError(state, "Não foi possível criar o projeto", "Escolha uma pasta de destino.");
    return;
  }

  const std::filesystem::path projectDir = directory / PathFromText(projectName);
  const std::filesystem::path projectPath = projectDir / PathFromText(projectName + ".cproject");
  if (platform::FileExists(projectPath)) {
    ShowError(state, "Já existe um projeto nessa pasta", PathText(projectPath));
    return;
  }

  std::error_code error;
  static_cast<void>(std::filesystem::create_directories(projectDir / "content" / "scenes", error));
  if (!error) {
    static_cast<void>(
        std::filesystem::create_directories(projectDir / "content" / "assets", error));
  }
  if (!error) {
    static_cast<void>(std::filesystem::create_directories(
        projectDir / "scripts" / PathFromText(projectName + ".Gameplay"), error));
  }
  if (!error) {
    static_cast<void>(std::filesystem::create_directories(projectDir / "out", error));
  }
  if (error) {
    ShowError(state, "Não foi possível criar as pastas do projeto", error.message());
    return;
  }

  build::Project project;
  project.name = projectName;
  project.projectVersion = "0.1.0";
  project.contentRoot = "content";
  project.initialScene = "scenes/inicio.cscene";
  project.gameplayProject =
      "scripts/" + projectName + ".Gameplay/" + projectName + ".Gameplay.csproj";
  project.gameplayAssembly = projectName + ".Gameplay";
  project.target = "windows-x64";
  project.output = "out";
  project.render.quality = "high";
  project.render.resolutionScale = 1.0f;

  const Result<void> saved = build::SaveProjectFile(projectPath, project);
  if (!saved) {
    ShowError(state, "Não foi possível criar o projeto", saved.GetError().message);
    return;
  }

  scene::Scene scene;
  static_cast<void>(scene.CreateEntity("Mundo"));
  const Result<std::string> sceneText = scene::SaveSceneToString(scene);
  if (sceneText) {
    const std::filesystem::path scenePath = projectDir / "content" / "scenes" / "inicio.cscene";
    static_cast<void>(platform::WriteTextFile(scenePath, sceneText.Value()));
  }

  OpenProject(state, projectPath);
}

}

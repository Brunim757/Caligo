#pragma once

#include <caligo/core/Result.h>
#include <filesystem>
#include <string>
#include <string_view>

namespace caligo::build {

inline constexpr int kProjectFormatVersion = 1;

struct ProjectRender {
  std::string quality = "high";
  float resolutionScale = 1.0f;
};

struct Project {
  std::string name;
  std::string projectVersion = "0.1.0";
  std::string contentRoot = "content";
  std::string initialScene = "scenes/inicio.cscene";
  std::string gameplayProject;
  std::string gameplayAssembly;
  std::string target = "windows-x64";
  std::string output = "out";
  ProjectRender render;
};

[[nodiscard]] Result<std::string> SaveProjectToString(const Project& project);
[[nodiscard]] Result<Project> LoadProjectFromString(std::string_view text);
[[nodiscard]] Result<Project> LoadProjectFile(const std::filesystem::path& path);
[[nodiscard]] Result<void> SaveProjectFile(const std::filesystem::path& path,
                                           const Project& project);

[[nodiscard]] std::filesystem::path ProjectContentRoot(const std::filesystem::path& projectPath,
                                                       const Project& project);
[[nodiscard]] std::filesystem::path ProjectInitialScene(const std::filesystem::path& projectPath,
                                                        const Project& project);

}

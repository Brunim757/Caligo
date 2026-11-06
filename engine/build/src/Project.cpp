#include <caligo/build/Project.h>
#include <caligo/platform/File.h>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

namespace caligo::build {

namespace {

using Json = nlohmann::ordered_json;

constexpr char kProjectFormat[] = "caligo.project";
constexpr float kMinResolutionScale = 0.5f;
constexpr float kMaxResolutionScale = 2.0f;

const Json* Member(const Json& object, const char* key) {
  if (!object.is_object()) {
    return nullptr;
  }
  const Json::const_iterator found = object.find(key);
  return found == object.end() ? nullptr : &(*found);
}

bool ReadString(const Json& object, const char* key, std::string& result) {
  const Json* value = Member(object, key);
  if (value == nullptr || !value->is_string()) {
    return false;
  }
  result = value->get<std::string>();
  return true;
}

bool IsKnownQuality(const std::string& quality) {
  return quality == "low" || quality == "medium" || quality == "high" || quality == "ultra";
}

Result<void> Validate(const Project& project) {
  if (project.name.empty()) {
    return Error{"project name is empty", 1};
  }
  if (project.target != "windows-x64") {
    return Error{"project target must be windows-x64", 2};
  }
  if (!IsKnownQuality(project.render.quality)) {
    return Error{"project render quality is unknown", 3};
  }
  if (!(project.render.resolutionScale >= kMinResolutionScale) ||
      project.render.resolutionScale > kMaxResolutionScale) {
    return Error{"project resolution scale is out of range", 4};
  }
  if (project.contentRoot.empty() || project.initialScene.empty()) {
    return Error{"project content paths are empty", 5};
  }

  struct Field {
    const char* name;
    const std::string* value;
  };
  const Field fields[] = {{"contentRoot", &project.contentRoot},
                          {"initialScene", &project.initialScene},
                          {"gameplayProject", &project.gameplayProject},
                          {"output", &project.output}};
  for (const Field& field : fields) {
    if (field.value->empty()) {
      continue;
    }
    if (std::filesystem::path(*field.value).is_absolute()) {
      return Error{std::format("project {} must be a relative path", field.name), 6};
    }
  }
  return {};
}

}

Result<std::string> SaveProjectToString(const Project& project) {
  const Result<void> valid = Validate(project);
  if (!valid) {
    return valid.GetError();
  }

  Json root;
  root["format"] = kProjectFormat;
  root["version"] = kProjectFormatVersion;
  root["name"] = project.name;
  root["projectVersion"] = project.projectVersion;
  root["contentRoot"] = project.contentRoot;
  root["initialScene"] = project.initialScene;
  root["gameplayProject"] = project.gameplayProject;
  root["gameplayAssembly"] = project.gameplayAssembly;
  root["target"] = project.target;
  root["output"] = project.output;
  root["render"] = Json{{"quality", project.render.quality},
                        {"resolutionScale", project.render.resolutionScale}};
  return root.dump(2) + "\n";
}

Result<Project> LoadProjectFromString(std::string_view text) {
  const Json root = Json::parse(text.begin(), text.end(), nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return Error{"project file is not valid JSON", 10};
  }

  std::string format;
  if (!ReadString(root, "format", format) || format != kProjectFormat) {
    return Error{"project file format is not caligo.project", 11};
  }
  const Json* version = Member(root, "version");
  if (version == nullptr || !version->is_number_unsigned()) {
    return Error{"project version must be an unsigned integer", 12};
  }
  if (version->get<uint32_t>() != static_cast<uint32_t>(kProjectFormatVersion)) {
    return Error{"project version is not supported", 13};
  }

  Project project;
  if (!ReadString(root, "name", project.name) || project.name.empty()) {
    return Error{"project name is missing", 14};
  }
  if (!ReadString(root, "contentRoot", project.contentRoot)) {
    return Error{"project content root is missing", 15};
  }
  if (!ReadString(root, "initialScene", project.initialScene)) {
    return Error{"project initial scene is missing", 16};
  }
  ReadString(root, "projectVersion", project.projectVersion);
  ReadString(root, "gameplayProject", project.gameplayProject);
  ReadString(root, "gameplayAssembly", project.gameplayAssembly);
  ReadString(root, "target", project.target);
  ReadString(root, "output", project.output);

  const Json* render = Member(root, "render");
  if (render != nullptr) {
    if (!render->is_object()) {
      return Error{"project render must be an object", 17};
    }
    ReadString(*render, "quality", project.render.quality);
    const Json* scale = Member(*render, "resolutionScale");
    if (scale != nullptr) {
      if (!scale->is_number()) {
        return Error{"project resolution scale must be a number", 18};
      }
      project.render.resolutionScale = scale->get<float>();
    }
  }

  const Result<void> valid = Validate(project);
  if (!valid) {
    return valid.GetError();
  }
  return project;
}

Result<Project> LoadProjectFile(const std::filesystem::path& path) {
  const Result<std::string> text = platform::ReadTextFile(path);
  if (!text) {
    return Error{std::format("could not read project file: {}", text.GetError().message), 20};
  }
  return LoadProjectFromString(text.Value());
}

Result<void> SaveProjectFile(const std::filesystem::path& path, const Project& project) {
  const Result<std::string> text = SaveProjectToString(project);
  if (!text) {
    return text.GetError();
  }
  return platform::WriteTextFile(path, text.Value());
}

std::filesystem::path ProjectContentRoot(const std::filesystem::path& projectPath,
                                         const Project& project) {
  return projectPath.parent_path() / std::filesystem::path(project.contentRoot);
}

std::filesystem::path ProjectInitialScene(const std::filesystem::path& projectPath,
                                          const Project& project) {
  return ProjectContentRoot(projectPath, project) / std::filesystem::path(project.initialScene);
}

}

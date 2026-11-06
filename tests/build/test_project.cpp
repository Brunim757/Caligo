#include <caligo/build/Project.h>
#include <doctest/doctest.h>
#include <filesystem>
#include <string>

namespace {

caligo::build::Project SampleProject() {
  caligo::build::Project project;
  project.name = "MeuJogo";
  project.projectVersion = "0.3.0";
  project.contentRoot = "content";
  project.initialScene = "scenes/inicio.cscene";
  project.gameplayProject = "scripts/MeuJogo.Gameplay/MeuJogo.Gameplay.csproj";
  project.gameplayAssembly = "MeuJogo.Gameplay";
  project.output = "out";
  project.render.quality = "high";
  project.render.resolutionScale = 1.0f;
  return project;
}

}

TEST_CASE("project serialization round trips") {
  const caligo::build::Project original = SampleProject();
  const caligo::Result<std::string> saved = caligo::build::SaveProjectToString(original);
  REQUIRE(saved);
  CHECK(saved.Value().find("\"format\": \"caligo.project\"") != std::string::npos);
  CHECK(saved.Value().find("\"version\": 1") != std::string::npos);
  CHECK(saved.Value().find("\"name\": \"MeuJogo\"") != std::string::npos);

  const caligo::Result<caligo::build::Project> loaded =
      caligo::build::LoadProjectFromString(saved.Value());
  REQUIRE(loaded);
  CHECK(loaded.Value().name == original.name);
  CHECK(loaded.Value().projectVersion == original.projectVersion);
  CHECK(loaded.Value().contentRoot == original.contentRoot);
  CHECK(loaded.Value().initialScene == original.initialScene);
  CHECK(loaded.Value().gameplayProject == original.gameplayProject);
  CHECK(loaded.Value().gameplayAssembly == original.gameplayAssembly);
  CHECK(loaded.Value().target == "windows-x64");
  CHECK(loaded.Value().output == original.output);
  CHECK(loaded.Value().render.quality == "high");
  CHECK(loaded.Value().render.resolutionScale == doctest::Approx(1.0f));
}

TEST_CASE("project loads the documented example") {
  constexpr char kExample[] = R"({
  "format": "caligo.project",
  "version": 1,
  "name": "MeuJogo",
  "projectVersion": "0.1.0",
  "contentRoot": "content",
  "initialScene": "scenes/inicio.cscene",
  "gameplayProject": "scripts/MeuJogo.Gameplay/MeuJogo.Gameplay.csproj",
  "gameplayAssembly": "MeuJogo.Gameplay",
  "target": "windows-x64",
  "output": "out",
  "render": {
    "quality": "high",
    "resolutionScale": 1.0
  }
})";
  const caligo::Result<caligo::build::Project> loaded =
      caligo::build::LoadProjectFromString(kExample);
  REQUIRE(loaded);
  CHECK(loaded.Value().name == "MeuJogo");
  CHECK(loaded.Value().projectVersion == "0.1.0");
  CHECK(loaded.Value().render.quality == "high");
}

TEST_CASE("project rejects the wrong format") {
  const caligo::Result<caligo::build::Project> loaded =
      caligo::build::LoadProjectFromString(R"({"format": "caligo.scene", "version": 1})");
  CHECK_FALSE(loaded.HasValue());
}

TEST_CASE("project rejects an unsupported version") {
  constexpr char kText[] = R"({"format": "caligo.project", "version": 2, "name": "Jogo",
    "contentRoot": "content", "initialScene": "scenes/inicio.cscene"})";
  const caligo::Result<caligo::build::Project> loaded = caligo::build::LoadProjectFromString(kText);
  CHECK_FALSE(loaded.HasValue());
}

TEST_CASE("project rejects invalid json") {
  const caligo::Result<caligo::build::Project> loaded =
      caligo::build::LoadProjectFromString("{ not json");
  CHECK_FALSE(loaded.HasValue());
}

TEST_CASE("project rejects an absolute path") {
  caligo::build::Project project = SampleProject();
  project.contentRoot = (std::filesystem::current_path() / "content").string();
  CHECK_FALSE(caligo::build::SaveProjectToString(project).HasValue());
}

TEST_CASE("project rejects an unknown target") {
  caligo::build::Project project = SampleProject();
  project.target = "linux-x64";
  CHECK_FALSE(caligo::build::SaveProjectToString(project).HasValue());
}

TEST_CASE("project rejects an out of range resolution scale") {
  caligo::build::Project project = SampleProject();
  project.render.resolutionScale = 4.0f;
  CHECK_FALSE(caligo::build::SaveProjectToString(project).HasValue());
}

TEST_CASE("project resolves content paths") {
  const caligo::build::Project project = SampleProject();
  const std::filesystem::path projectPath = "C:/jogo/MeuJogo.cproject";
  const std::filesystem::path root = caligo::build::ProjectContentRoot(projectPath, project);
  CHECK(root == std::filesystem::path("C:/jogo") / "content");
  const std::filesystem::path scene = caligo::build::ProjectInitialScene(projectPath, project);
  CHECK(scene == std::filesystem::path("C:/jogo") / "content" / "scenes/inicio.cscene");
}

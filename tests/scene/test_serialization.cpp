#include <caligo/platform/File.h>
#include <caligo/scene/Scene.h>
#include <caligo/scene/SceneSerialization.h>
#include <doctest/doctest.h>
#include <filesystem>
#include <string>

namespace {

caligo::scene::Scene BuildScene() {
  caligo::scene::Scene scene;
  const caligo::scene::EntityId root = scene.CreateEntity("Mundo");
  const caligo::scene::EntityId child = scene.CreateEntity("Café", root);
  caligo::scene::Transform transform;
  transform.position = caligo::math::Vec3{1.5f, 2.25f, -3.75f};
  transform.rotation =
      caligo::math::RotationFromEulerDegrees(caligo::math::Vec3{0.0f, 45.0f, 0.0f});
  transform.scale = caligo::math::Vec3{2.0f, 0.5f, 3.0f};
  REQUIRE(scene.SetTransform(child, transform));
  REQUIRE(scene.AddScript(child, "Spinner"));
  REQUIRE(scene.AddScript(child, "PlayerMover"));
  caligo::scene::WorldEnvironment environment;
  environment.skyHours = 7.25f;
  REQUIRE(scene.SetEnvironment(environment));
  return scene;
}

}

TEST_CASE("scene serialization round trips") {
  const caligo::scene::Scene original = BuildScene();
  const caligo::Result<std::string> saved = caligo::scene::SaveSceneToString(original);
  REQUIRE(saved);
  CHECK(saved.Value().find("\"format\": \"caligo.scene\"") != std::string::npos);
  CHECK(saved.Value().find("\"version\": 3") != std::string::npos);
  CHECK(saved.Value().find("\"environment\"") != std::string::npos);
  CHECK(saved.Value().find("Café") != std::string::npos);
  CHECK(saved.Value().find("caligo.script") != std::string::npos);

  const caligo::Result<caligo::scene::Scene> loaded =
      caligo::scene::LoadSceneFromString(saved.Value());
  REQUIRE(loaded);
  const caligo::scene::Scene& scene = loaded.Value();
  CHECK(scene.Guid() == original.Guid());
  CHECK(scene.Environment().skyHours == doctest::Approx(7.25f));
  REQUIRE(scene.Entities().size() == 2);
  CHECK(scene.Validate());

  const caligo::scene::Entity* child = nullptr;
  for (const caligo::scene::Entity& entity : scene.Entities()) {
    if (entity.name == "Café") {
      child = &entity;
      break;
    }
  }
  REQUIRE(child != nullptr);
  CHECK(child->transform.position.x == doctest::Approx(1.5f));
  CHECK(child->transform.position.y == doctest::Approx(2.25f));
  CHECK(child->transform.position.z == doctest::Approx(-3.75f));
  CHECK(child->transform.scale.x == doctest::Approx(2.0f));
  CHECK(child->transform.scale.y == doctest::Approx(0.5f));
  CHECK(child->transform.scale.z == doctest::Approx(3.0f));
  REQUIRE(child->scripts.size() == 2);
  CHECK(child->scripts[0].type == "Spinner");
  CHECK(child->scripts[1].type == "PlayerMover");
}

TEST_CASE("scene serialization accepts version one scenes") {
  const std::string text =
      R"({"format":"caligo.scene","version":1,"guid":"8f14e45fceea167a5a36dedd4bea2543","entities":[{"id":1,"name":"Mundo","parent":0,"components":[{"type":"caligo.transform","position":[0.0,0.0,0.0],"rotation":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]}]}]})";
  const caligo::Result<caligo::scene::Scene> loaded = caligo::scene::LoadSceneFromString(text);
  REQUIRE(loaded);
  REQUIRE(loaded.Value().Entities().size() == 1);
  CHECK(loaded.Value().Entities().front().scripts.empty());
  CHECK(loaded.Value().Environment().skyHours == doctest::Approx(12.0f));
  CHECK(loaded.Value().Validate());
}

TEST_CASE("scene serialization rejects malformed input") {
  CHECK_FALSE(caligo::scene::LoadSceneFromString(""));
  CHECK_FALSE(caligo::scene::LoadSceneFromString("[]"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(R"({"format":"other","version":1})"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":4,"guid":"8f14e45fceea167a5a36dedd4bea2543","entities":[]})"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":3,"guid":"8f14e45fceea167a5a36dedd4bea2543","entities":[]})"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":3,"guid":"8f14e45fceea167a5a36dedd4bea2543","environment":{"hours":25.0},"entities":[]})"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":1,"guid":"8f14e45fceea167a5a36dedd4bea2543","entities":[{"id":1,"name":"Sem transform","parent":0,"components":[]}]})"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":2,"guid":"8f14e45fceea167a5a36dedd4bea2543","entities":[{"id":1,"name":"Script sem classe","parent":0,"components":[{"type":"caligo.transform","position":[0.0,0.0,0.0],"rotation":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]},{"type":"caligo.script"}]}]})"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":2,"guid":"8f14e45fceea167a5a36dedd4bea2543","entities":[{"id":1,"name":"Script inválido","parent":0,"components":[{"type":"caligo.transform","position":[0.0,0.0,0.0],"rotation":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]},{"type":"caligo.script","class":"Spinner!"}]}]})"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":2,"guid":"8f14e45fceea167a5a36dedd4bea2543","entities":[{"id":1,"name":"Script repetido","parent":0,"components":[{"type":"caligo.transform","position":[0.0,0.0,0.0],"rotation":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]},{"type":"caligo.script","class":"Spinner"},{"type":"caligo.script","class":"Spinner"}]}]})"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":2,"guid":"8f14e45fceea167a5a36dedd4bea2543","entities":[{"id":1,"name":"Componente desconhecido","parent":0,"components":[{"type":"caligo.transform","position":[0.0,0.0,0.0],"rotation":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]},{"type":"caligo.light"}]}]})"));
}

TEST_CASE("scene serialization round trips terrain") {
  caligo::scene::Scene original;
  const caligo::scene::EntityId id = original.CreateEntity("Relevo");
  REQUIRE(original.AddTerrain(id, 4, 16.0f));
  caligo::scene::Terrain* terrain = original.EditTerrain(id);
  REQUIRE(terrain != nullptr);
  terrain->heights[0] = 3.5f;
  terrain->heights[15] = -2.0f;
  const size_t painting = 5 * caligo::scene::kTerrainLayers;
  terrain->layerWeights[painting] = 0.5f;
  terrain->layerWeights[painting + 1] = 0.5f;
  terrain->layerColors[2] = caligo::math::Vec3{0.1f, 0.2f, 0.3f};

  const caligo::Result<std::string> saved = caligo::scene::SaveSceneToString(original);
  REQUIRE(saved);
  CHECK(saved.Value().find("\"caligo.terrain\"") != std::string::npos);

  const caligo::Result<caligo::scene::Scene> loaded =
      caligo::scene::LoadSceneFromString(saved.Value());
  REQUIRE(loaded);
  const caligo::scene::Terrain* reloaded = loaded.Value().FindTerrain(id);
  REQUIRE(reloaded != nullptr);
  CHECK(reloaded->resolution == 4);
  CHECK(reloaded->extent == doctest::Approx(16.0f));
  REQUIRE(reloaded->heights.size() == 16);
  CHECK(reloaded->heights[0] == doctest::Approx(3.5f));
  CHECK(reloaded->heights[15] == doctest::Approx(-2.0f));
  CHECK(reloaded->layerWeights[painting] == doctest::Approx(0.5f));
  CHECK(reloaded->layerWeights[painting + 1] == doctest::Approx(0.5f));
  CHECK(reloaded->layerColors[2].x == doctest::Approx(0.1f));
  CHECK(reloaded->layerColors[2].y == doctest::Approx(0.2f));
  CHECK(reloaded->layerColors[2].z == doctest::Approx(0.3f));
  CHECK(loaded.Value().Validate());
}

TEST_CASE("scene serialization rejects malformed terrain") {
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":3,"guid":"8f14e45fceea167a5a36dedd4bea2543","environment":{"hours":12.0},"entities":[{"id":1,"name":"Relevo","parent":0,"components":[{"type":"caligo.transform","position":[0.0,0.0,0.0],"rotation":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]},{"type":"caligo.terrain","resolution":3,"extent":4.0,"heights":[0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0],"layers":[1.0,0.0,0.0,0.0],"layerColors":[[0.0,0.0,0.0],[0.0,0.0,0.0],[0.0,0.0,0.0],[0.0,0.0,0.0]]}]}]})"));
}

TEST_CASE("example scene loads") {
  const std::filesystem::path path =
      std::filesystem::path(CALIGO_CONTENT_DIR) / "scenes" / "exemplo.cscene";
  const caligo::Result<std::string> text = caligo::platform::ReadTextFile(path);
  REQUIRE(text);
  const caligo::Result<caligo::scene::Scene> scene =
      caligo::scene::LoadSceneFromString(text.Value());
  REQUIRE(scene);
  CHECK(scene.Value().Validate());
  CHECK(scene.Value().Entities().size() == 9);
  bool foundPlayer = false;
  bool foundSpinner = false;
  for (const caligo::scene::Entity& entity : scene.Value().Entities()) {
    for (const caligo::scene::ScriptComponent& script : entity.scripts) {
      foundPlayer = foundPlayer || script.type == "PlayerMover";
      foundSpinner = foundSpinner || script.type == "Spinner";
    }
  }
  CHECK(foundPlayer);
  CHECK(foundSpinner);
}

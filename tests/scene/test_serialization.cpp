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
  return scene;
}

}

TEST_CASE("scene serialization round trips") {
  const caligo::scene::Scene original = BuildScene();
  const caligo::Result<std::string> saved = caligo::scene::SaveSceneToString(original);
  REQUIRE(saved);
  CHECK(saved.Value().find("\"format\": \"caligo.scene\"") != std::string::npos);
  CHECK(saved.Value().find("Café") != std::string::npos);

  const caligo::Result<caligo::scene::Scene> loaded =
      caligo::scene::LoadSceneFromString(saved.Value());
  REQUIRE(loaded);
  const caligo::scene::Scene& scene = loaded.Value();
  CHECK(scene.Guid() == original.Guid());
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
}

TEST_CASE("scene serialization rejects malformed input") {
  CHECK_FALSE(caligo::scene::LoadSceneFromString(""));
  CHECK_FALSE(caligo::scene::LoadSceneFromString("[]"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(R"({"format":"other","version":1})"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":2,"guid":"8f14e45fceea167a5a36dedd4bea2543","entities":[]})"));
  CHECK_FALSE(caligo::scene::LoadSceneFromString(
      R"({"format":"caligo.scene","version":1,"guid":"8f14e45fceea167a5a36dedd4bea2543","entities":[{"id":1,"name":"Sem transform","parent":0,"components":[]}]})"));
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
  CHECK(scene.Value().Entities().size() == 8);
}

#include <caligo/scene/Scene.h>
#include <cmath>
#include <doctest/doctest.h>
#include <limits>

namespace {

caligo::scene::EntityId CreateEntity(caligo::scene::Scene& scene, const char* name,
                                     caligo::scene::EntityId parent = caligo::scene::kNoEntity) {
  return scene.CreateEntity(name, parent);
}

}

TEST_CASE("scene builds hierarchy and world transforms") {
  caligo::scene::Scene scene;
  const caligo::scene::EntityId root = CreateEntity(scene, "Raiz");
  REQUIRE(root != caligo::scene::kNoEntity);
  const caligo::scene::EntityId child = CreateEntity(scene, "Filho", root);
  REQUIRE(child != caligo::scene::kNoEntity);

  const caligo::scene::Entity* childEntity = scene.Find(child);
  REQUIRE(childEntity != nullptr);
  CHECK(childEntity->parent == root);
  CHECK(scene.IsDescendant(root, child));
  CHECK_FALSE(scene.IsDescendant(child, root));

  caligo::scene::Transform parentTransform;
  parentTransform.position = caligo::math::Vec3{10.0f, 0.0f, 0.0f};
  REQUIRE(scene.SetTransform(root, parentTransform));

  caligo::scene::Transform childTransform;
  childTransform.position = caligo::math::Vec3{0.0f, 5.0f, 0.0f};
  REQUIRE(scene.SetTransform(child, childTransform));

  const caligo::math::Vec3 worldPosition =
      caligo::math::TransformPoint(scene.WorldMatrix(child), caligo::math::Vec3{0.0f, 0.0f, 0.0f});
  CHECK(worldPosition.x == doctest::Approx(10.0f));
  CHECK(worldPosition.y == doctest::Approx(5.0f));
  CHECK(worldPosition.z == doctest::Approx(0.0f));

  CHECK_FALSE(scene.SetParent(root, child));
  CHECK(scene.SetParent(child, caligo::scene::kNoEntity));
}

TEST_CASE("scene destroys subtrees") {
  caligo::scene::Scene scene;
  const caligo::scene::EntityId root = CreateEntity(scene, "Raiz");
  const caligo::scene::EntityId child = CreateEntity(scene, "Filho", root);
  const caligo::scene::EntityId grandchild = CreateEntity(scene, "Neto", child);
  REQUIRE(grandchild != caligo::scene::kNoEntity);

  const uint64_t revision = scene.Revision();
  CHECK(scene.DestroyEntity(root));
  CHECK(scene.Find(root) == nullptr);
  CHECK(scene.Find(child) == nullptr);
  CHECK(scene.Find(grandchild) == nullptr);
  CHECK(scene.Entities().empty());
  CHECK(scene.Revision() > revision);
  CHECK_FALSE(scene.DestroyEntity(root));
}

TEST_CASE("scene enforces hierarchy depth") {
  caligo::scene::Scene scene;
  caligo::scene::EntityId leaf = caligo::scene::kNoEntity;
  for (uint32_t index = 0; index < caligo::scene::kMaxHierarchyDepth; ++index) {
    leaf = CreateEntity(scene, "Nó", leaf);
    REQUIRE(leaf != caligo::scene::kNoEntity);
  }
  CHECK(scene.CreateEntity("Além", leaf) == caligo::scene::kNoEntity);
  CHECK(scene.Validate());

  const caligo::scene::EntityId other = CreateEntity(scene, "Outra raiz");
  REQUIRE(other != caligo::scene::kNoEntity);
  REQUIRE(CreateEntity(scene, "Filha", other) != caligo::scene::kNoEntity);
  CHECK_FALSE(scene.SetParent(other, leaf));
  CHECK(scene.Validate());
}

TEST_CASE("scene rejects hierarchy deeper than the limit") {
  caligo::scene::Scene scene;
  constexpr caligo::scene::EntityId kDeepest = caligo::scene::kMaxHierarchyDepth + 1;
  for (caligo::scene::EntityId id = kDeepest; id > 1; --id) {
    REQUIRE(scene.InsertEntity(id, "Nó", id - 1, caligo::scene::Transform{}));
  }
  REQUIRE(scene.InsertEntity(1, "Raiz", caligo::scene::kNoEntity, caligo::scene::Transform{}));
  CHECK_FALSE(scene.Validate());
}

TEST_CASE("scene validates edits") {
  caligo::scene::Scene scene;
  const caligo::scene::EntityId entity = CreateEntity(scene, "Entidade");
  REQUIRE(entity != caligo::scene::kNoEntity);
  CHECK(scene.Validate());

  CHECK(scene.Rename(entity, ""));
  CHECK(scene.Find(entity)->name == "Entidade");

  caligo::scene::Transform transform;
  transform.position = caligo::math::Vec3{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f};
  CHECK_FALSE(scene.SetTransform(entity, transform));

  CHECK(scene.CreateEntity("Órfã", 9999) == caligo::scene::kNoEntity);
  CHECK_FALSE(scene.InsertEntity(entity, "Duplicada", caligo::scene::kNoEntity,
                                 caligo::scene::Transform{}));
  CHECK(caligo::scene::IsValidGuid(scene.Guid()));
  CHECK(scene.Validate());
}

TEST_CASE("scene manages script components") {
  CHECK(caligo::scene::IsValidScriptType("Spinner"));
  CHECK(caligo::scene::IsValidScriptType("Caligo.Gameplay.PlayerMover"));
  CHECK(caligo::scene::IsValidScriptType("A1._B"));
  CHECK_FALSE(caligo::scene::IsValidScriptType(""));
  CHECK_FALSE(caligo::scene::IsValidScriptType("1Spinner"));
  CHECK_FALSE(caligo::scene::IsValidScriptType(".Spinner"));
  CHECK_FALSE(caligo::scene::IsValidScriptType("Spinner."));
  CHECK_FALSE(caligo::scene::IsValidScriptType("Spinner..Parte"));
  CHECK_FALSE(caligo::scene::IsValidScriptType("Spinner-Parte"));
  CHECK_FALSE(
      caligo::scene::IsValidScriptType(std::string(caligo::scene::kMaxScriptTypeLength + 1, 'A')));

  caligo::scene::Scene scene;
  const caligo::scene::EntityId entity = CreateEntity(scene, "Entidade");
  REQUIRE(entity != caligo::scene::kNoEntity);
  CHECK_FALSE(scene.AddScript(9999, "Spinner"));
  CHECK_FALSE(scene.AddScript(entity, "Spinner!"));
  CHECK(scene.AddScript(entity, "Spinner"));
  CHECK_FALSE(scene.AddScript(entity, "Spinner"));
  CHECK(scene.AddScript(entity, "PlayerMover"));
  REQUIRE(scene.Find(entity)->scripts.size() == 2);
  CHECK(scene.Find(entity)->scripts[0].type == "Spinner");
  CHECK(scene.Find(entity)->scripts[1].type == "PlayerMover");
  CHECK(scene.Validate());

  CHECK_FALSE(scene.RemoveScript(entity, "Desconhecido"));
  CHECK(scene.RemoveScript(entity, "Spinner"));
  REQUIRE(scene.Find(entity)->scripts.size() == 1);
  CHECK(scene.Find(entity)->scripts[0].type == "PlayerMover");

  for (size_t index = 1; index < caligo::scene::kMaxScriptsPerEntity; ++index) {
    REQUIRE(scene.AddScript(entity, std::string("Script") + std::to_string(index)));
  }
  CHECK_FALSE(scene.AddScript(entity, "ScriptExtra"));
  CHECK(scene.Validate());
}

TEST_CASE("scene validation rejects bad script components") {
  caligo::scene::Scene scene;
  const caligo::scene::EntityId entity = CreateEntity(scene, "Entidade");
  REQUIRE(entity != caligo::scene::kNoEntity);
  REQUIRE(scene.AddScript(entity, "Spinner"));
  REQUIRE(scene.Validate());

  scene.Find(entity)->scripts.front().type = "Spinner!";
  CHECK_FALSE(scene.Validate());
  scene.Find(entity)->scripts.front().type = "Spinner";
  scene.Find(entity)->scripts.push_back(caligo::scene::ScriptComponent{"Spinner"});
  CHECK_FALSE(scene.Validate());
}

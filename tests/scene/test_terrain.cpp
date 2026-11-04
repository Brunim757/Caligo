#include <caligo/scene/Scene.h>
#include <doctest/doctest.h>

TEST_CASE("o terreno guarda alturas e camadas") {
  caligo::scene::Scene scene;
  const caligo::scene::EntityId id = scene.CreateEntity("Relevo");
  REQUIRE(scene.FindTerrain(id) == nullptr);

  const uint64_t revision = scene.Revision();
  REQUIRE(scene.AddTerrain(id, 5, 32.0f));
  CHECK(scene.Revision() > revision);

  const caligo::scene::Terrain* terrain = scene.FindTerrain(id);
  REQUIRE(terrain != nullptr);
  CHECK(terrain->resolution == 5);
  CHECK(terrain->extent == doctest::Approx(32.0f));
  REQUIRE(terrain->heights.size() == 25);
  REQUIRE(terrain->layerWeights.size() == 25 * caligo::scene::kTerrainLayers);
  for (size_t sample = 0; sample < terrain->heights.size(); ++sample) {
    CHECK(terrain->heights[sample] == doctest::Approx(0.0f));
    CHECK(terrain->layerWeights[sample * caligo::scene::kTerrainLayers] == doctest::Approx(1.0f));
    for (uint32_t layer = 1; layer < caligo::scene::kTerrainLayers; ++layer) {
      CHECK(terrain->layerWeights[sample * caligo::scene::kTerrainLayers + layer] ==
            doctest::Approx(0.0f));
    }
  }
  CHECK(scene.Validate());
}

TEST_CASE("a edição do terreno marca a cena como modificada") {
  caligo::scene::Scene scene;
  const caligo::scene::EntityId id = scene.CreateEntity("Relevo");
  REQUIRE(scene.AddTerrain(id, 4, 8.0f));

  const uint64_t revision = scene.Revision();
  caligo::scene::Terrain* editable = scene.EditTerrain(id);
  REQUIRE(editable != nullptr);
  CHECK(scene.Revision() > revision);
  editable->heights[0] = 1.5f;
  CHECK(scene.FindTerrain(id)->heights[0] == doctest::Approx(1.5f));
  CHECK(scene.EditTerrain(caligo::scene::kNoEntity) == nullptr);
}

TEST_CASE("o terreno amostra a altura interpolada") {
  caligo::scene::Scene scene;
  const caligo::scene::EntityId id = scene.CreateEntity("Relevo");
  REQUIRE(scene.AddTerrain(id, 3, 2.0f));
  caligo::scene::Terrain* terrain = scene.EditTerrain(id);
  REQUIRE(terrain != nullptr);
  for (uint32_t z = 0; z < 3; ++z) {
    for (uint32_t x = 0; x < 3; ++x) {
      terrain->heights[z * 3 + x] = static_cast<float>(x);
    }
  }

  CHECK(caligo::scene::TerrainSpacing(*terrain) == doctest::Approx(1.0f));
  CHECK(caligo::scene::TerrainHeightAt(*terrain, -1.0f, 0.0f) == doctest::Approx(0.0f));
  CHECK(caligo::scene::TerrainHeightAt(*terrain, 0.0f, 0.0f) == doctest::Approx(1.0f));
  CHECK(caligo::scene::TerrainHeightAt(*terrain, 0.5f, 0.0f) == doctest::Approx(1.5f));
  CHECK(caligo::scene::TerrainHeightAt(*terrain, 1.0f, 1.0f) == doctest::Approx(2.0f));
  CHECK(caligo::scene::TerrainHeightAt(*terrain, 4.0f, 4.0f) == doctest::Approx(2.0f));
}

TEST_CASE("o terreno rejeita parâmetros inválidos") {
  caligo::scene::Scene scene;
  const caligo::scene::EntityId id = scene.CreateEntity("Relevo");
  CHECK_FALSE(scene.AddTerrain(id, 1, 32.0f));
  CHECK_FALSE(scene.AddTerrain(id, 4096, 32.0f));
  CHECK_FALSE(scene.AddTerrain(id, 8, 0.0f));
  CHECK_FALSE(scene.AddTerrain(id, 8, -4.0f));
  CHECK_FALSE(scene.AddTerrain(caligo::scene::kNoEntity, 8, 32.0f));
  REQUIRE(scene.AddTerrain(id, 8, 32.0f));
  CHECK_FALSE(scene.AddTerrain(id, 8, 32.0f));
  CHECK(scene.RemoveTerrain(id));
  CHECK(scene.FindTerrain(id) == nullptr);
  CHECK_FALSE(scene.RemoveTerrain(id));
}

TEST_CASE("a validação recusa terreno inconsistente") {
  caligo::scene::Scene scene;
  const caligo::scene::EntityId id = scene.CreateEntity("Relevo");
  caligo::scene::Terrain terrain;
  terrain.resolution = 3;
  terrain.extent = 4.0f;
  CHECK_FALSE(scene.SetTerrain(id, terrain));

  terrain.heights.assign(9, 0.0f);
  terrain.layerWeights.assign(9 * caligo::scene::kTerrainLayers, 0.0f);
  for (size_t sample = 0; sample < 9; ++sample) {
    terrain.layerWeights[sample * caligo::scene::kTerrainLayers] = 1.0f;
  }
  CHECK(scene.SetTerrain(id, terrain));
  CHECK(scene.FindTerrain(id) != nullptr);

  caligo::scene::Terrain* editable = scene.EditTerrain(id);
  REQUIRE(editable != nullptr);
  editable->heights.pop_back();
  CHECK_FALSE(scene.Validate());
}

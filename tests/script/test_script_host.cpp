#include <caligo/script/ScriptHost.h>
#include <doctest/doctest.h>
#include <filesystem>
#include <string>
#include <vector>

TEST_CASE("script host refuses to run without a runtime") {
  caligo::script::ScriptHost host;
  CHECK_FALSE(host.IsReady());

  const caligo::Result<void> initialized =
      host.Initialize(std::filesystem::path("__caligo_missing_runtime__"));
  CHECK_FALSE(initialized);
  CHECK_FALSE(host.IsReady());
  CHECK_FALSE(host.LastError().empty());

  const caligo::Result<std::vector<std::string>> types = host.ScriptTypes();
  CHECK_FALSE(types);

  caligo::scene::Scene scene;
  const caligo::Result<void> play = host.BeginPlay(scene);
  CHECK_FALSE(play);

  const caligo::Result<void> tick = host.Tick(1.0f / 60.0f);
  CHECK_FALSE(tick);

  const caligo::Result<void> reload = host.Reload();
  CHECK_FALSE(reload);

  host.EndPlay();
  host.Shutdown();
}

TEST_CASE("script input starts neutral") {
  const caligo::script::ScriptInput input;
  for (size_t index = 0; index < caligo::script::kScriptKeyCount; ++index) {
    CHECK_FALSE(input.down[index]);
    CHECK_FALSE(input.pressed[index]);
  }
}

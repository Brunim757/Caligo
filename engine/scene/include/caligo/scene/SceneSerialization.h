#pragma once

#include <caligo/core/Result.h>
#include <caligo/scene/Scene.h>
#include <string>
#include <string_view>

namespace caligo::scene {

[[nodiscard]] Result<std::string> SaveSceneToString(const Scene& scene);
[[nodiscard]] Result<Scene> LoadSceneFromString(std::string_view text);

}

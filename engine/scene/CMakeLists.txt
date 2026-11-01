add_library(caligo_scene STATIC
  src/Scene.cpp
  src/SceneSerialization.cpp
)

target_include_directories(caligo_scene PUBLIC include)

target_link_libraries(caligo_scene
  PUBLIC caligo_core
  PRIVATE nlohmann_json::nlohmann_json
)

caligo_configure_target(caligo_scene)

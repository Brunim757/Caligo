add_library(caligo_build STATIC
  src/Project.cpp
)

target_include_directories(caligo_build PUBLIC include)

target_link_libraries(caligo_build
  PUBLIC caligo_core
  PRIVATE caligo_platform nlohmann_json::nlohmann_json
)

caligo_configure_target(caligo_build)

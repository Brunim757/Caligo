add_library(caligo_script STATIC
  src/HostFxr.cpp
  src/ScriptHost.cpp
)

target_include_directories(caligo_script
  PUBLIC include
  PRIVATE src
)

target_link_libraries(caligo_script
  PUBLIC caligo_core caligo_scene
)

caligo_configure_target(caligo_script)

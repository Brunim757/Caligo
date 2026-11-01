add_library(caligo_render STATIC
  src/GraphicsDevice.cpp
)

target_include_directories(caligo_render PUBLIC include)

target_link_libraries(caligo_render
  PUBLIC caligo_core
  PUBLIC caligo_platform
  PRIVATE d3d11 dxgi
)

caligo_configure_target(caligo_render)

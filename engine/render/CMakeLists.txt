add_library(caligo_render STATIC
  src/GBufferPass.cpp
  src/GraphicsDevice.cpp
  src/ImageDiff.cpp
  src/LightingPass.cpp
  src/PostProcessor.cpp
  src/RhiBuffer.cpp
  src/RhiDevice.cpp
  src/RhiInputLayout.cpp
  src/RhiPipeline.cpp
  src/RhiSampler.cpp
  src/RhiShader.cpp
  src/RhiTexture.cpp
  src/RenderGraph.cpp
  src/SceneRenderer.cpp
  src/ShadowMap.cpp
  src/TaaRenderer.cpp
)

target_include_directories(caligo_render PUBLIC include)

target_link_libraries(caligo_render
  PUBLIC caligo_core
  PUBLIC caligo_platform
  PRIVATE d3d11 dxgi d3dcompiler
)

caligo_configure_target(caligo_render)

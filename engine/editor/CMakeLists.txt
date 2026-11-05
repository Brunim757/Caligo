add_library(caligo_editor STATIC
  src/ConsoleBuffer.cpp
  src/ConsolePanel.cpp
  src/Editor.cpp
  src/EditorCamera.cpp
  src/EditorMenu.cpp
  src/EditorPanels.h
  src/EditorPlay.cpp
  src/EditorProject.cpp
  src/EditorScene.cpp
  src/EditorState.h
  src/EditorUi.cpp
  src/HierarchyPanel.cpp
  src/InspectorPanel.cpp
  src/SkyTool.cpp
  src/TerrainTool.cpp
  src/ViewportCanvas.cpp
  src/ViewportPanel.cpp
)

target_include_directories(caligo_editor
  PUBLIC include
  PRIVATE src
)

target_link_libraries(caligo_editor
  PUBLIC caligo_core imgui
  PRIVATE caligo_platform caligo_render caligo_scene caligo_script caligo_build imguizmo d3d11 dxgi
)

target_compile_definitions(caligo_editor PRIVATE
  CALIGO_VERSION="${PROJECT_VERSION}"
  CALIGO_CONFIGURATION="$<CONFIG>"
)

caligo_configure_target(caligo_editor)

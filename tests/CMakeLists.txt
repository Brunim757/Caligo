add_executable(caligo_tests
  main.cpp
  core/test_result.cpp
  core/test_log.cpp
  core/test_time.cpp
  core/test_job_system.cpp
  core/test_math.cpp
  platform/test_input.cpp
  platform/test_file.cpp
  scene/test_scene.cpp
  scene/test_serialization.cpp
  script/test_script_host.cpp
  editor/test_console.cpp
)

target_link_libraries(caligo_tests PRIVATE
  caligo_core
  caligo_platform
  caligo_scene
  caligo_script
  caligo_editor
  doctest::doctest
)

target_compile_definitions(caligo_tests PRIVATE
  CALIGO_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content"
)

caligo_configure_target(caligo_tests)

add_test(NAME caligo_tests COMMAND caligo_tests)
add_test(NAME caligo_headless COMMAND caligo --headless --frames 120)
set_tests_properties(caligo_headless PROPERTIES TIMEOUT 60)
add_test(NAME caligo_script_smoke COMMAND caligo --script-smoke)
set_tests_properties(caligo_script_smoke PROPERTIES TIMEOUT 120)

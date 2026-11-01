add_executable(caligo
  src/main.cpp
  src/Application.cpp
)

add_dependencies(caligo caligo_scripts)
caligo_copy_scripts(caligo)

target_link_libraries(caligo PRIVATE caligo_editor caligo_platform caligo_script)

target_compile_definitions(caligo PRIVATE
  CALIGO_VERSION="${PROJECT_VERSION}"
  CALIGO_CONFIGURATION="$<CONFIG>"
)

add_custom_command(TARGET caligo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/content"
    "$<TARGET_FILE_DIR:caligo>/content"
)

caligo_configure_target(caligo)

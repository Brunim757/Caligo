find_program(CALIGO_DOTNET_EXECUTABLE NAMES dotnet)
if(NOT CALIGO_DOTNET_EXECUTABLE)
  message(FATAL_ERROR "The .NET SDK is required to build Caligo.")
endif()

if(NOT CALIGO_DOTNET_ROOT)
  execute_process(
    COMMAND "${CALIGO_DOTNET_EXECUTABLE}" --list-runtimes
    OUTPUT_VARIABLE _caligo_runtimes
    RESULT_VARIABLE _caligo_runtimes_status
    ERROR_QUIET
  )
  if(_caligo_runtimes_status EQUAL 0)
    string(REGEX MATCH "Microsoft.NETCore.App [^\r\n]*\\[([^\r\n\\]]+)\\]" _caligo_runtime
                         "${_caligo_runtimes}")
    if(CMAKE_MATCH_1)
      get_filename_component(_caligo_shared "${CMAKE_MATCH_1}" DIRECTORY)
      get_filename_component(_caligo_root "${_caligo_shared}" DIRECTORY)
      set(CALIGO_DOTNET_ROOT "${_caligo_root}" CACHE PATH "Private .NET runtime root" FORCE)
    endif()
  endif()
endif()

set(CALIGO_SCRIPTS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/scripts")
set(CALIGO_SCRIPTS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/scripts")
set(CALIGO_SCRIPTS_HOST_DIR "${CALIGO_SCRIPTS_OUTPUT_DIR}/host")
set(CALIGO_SCRIPTS_GAMEPLAY_DIR "${CALIGO_SCRIPTS_OUTPUT_DIR}/gameplay")

file(GLOB_RECURSE CALIGO_SCRIPTS_SOURCES CONFIGURE_DEPENDS
  "${CALIGO_SCRIPTS_SOURCE_DIR}/Caligo.Scripting/*.cs"
  "${CALIGO_SCRIPTS_SOURCE_DIR}/Caligo.Scripting.Loader/*.cs"
  "${CALIGO_SCRIPTS_SOURCE_DIR}/Caligo.Gameplay/*.cs"
  "${CALIGO_SCRIPTS_SOURCE_DIR}/Directory.Build.props"
)

add_custom_target(caligo_scripts
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CALIGO_SCRIPTS_HOST_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CALIGO_SCRIPTS_GAMEPLAY_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E env
          "DOTNET_CLI_TELEMETRY_OPTOUT=1"
          "DOTNET_NOLOGO=1"
          "DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1"
          "${CALIGO_DOTNET_EXECUTABLE}" publish
          "${CALIGO_SCRIPTS_SOURCE_DIR}/Caligo.Scripting.Loader/Caligo.Scripting.Loader.csproj"
          -c "$<CONFIG>" --nologo -o "${CALIGO_SCRIPTS_HOST_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E env
          "DOTNET_CLI_TELEMETRY_OPTOUT=1"
          "DOTNET_NOLOGO=1"
          "DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1"
          "${CALIGO_DOTNET_EXECUTABLE}" publish
          "${CALIGO_SCRIPTS_SOURCE_DIR}/Caligo.Gameplay/Caligo.Gameplay.csproj"
          -c "$<CONFIG>" --nologo -o "${CALIGO_SCRIPTS_GAMEPLAY_DIR}"
  DEPENDS ${CALIGO_SCRIPTS_SOURCES}
  COMMENT "Publishing managed scripts"
  VERBATIM
)

function(caligo_copy_scripts target)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CALIGO_SCRIPTS_HOST_DIR}" "$<TARGET_FILE_DIR:${target}>/runtime/host"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CALIGO_SCRIPTS_GAMEPLAY_DIR}" "$<TARGET_FILE_DIR:${target}>/runtime/gameplay"
    COMMENT "Copying managed scripts next to ${target}"
    VERBATIM
  )
  if(CALIGO_DOTNET_ROOT)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
              "${CALIGO_DOTNET_ROOT}/host/fxr"
              "$<TARGET_FILE_DIR:${target}>/runtime/dotnet/host/fxr"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
              "${CALIGO_DOTNET_ROOT}/shared/Microsoft.NETCore.App"
              "$<TARGET_FILE_DIR:${target}>/runtime/dotnet/shared/Microsoft.NETCore.App"
      COMMENT "Copying the private .NET runtime next to ${target}"
      VERBATIM
    )
  else()
    message(WARNING "CALIGO_DOTNET_ROOT is unknown; the private .NET runtime will not be copied.")
  endif()
endfunction()
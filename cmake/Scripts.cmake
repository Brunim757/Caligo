find_program(CALIGO_DOTNET_EXECUTABLE NAMES dotnet)
if(NOT CALIGO_DOTNET_EXECUTABLE)
  message(FATAL_ERROR "The .NET SDK is required to build Caligo.")
endif()

if(NOT CALIGO_DOTNET_ROOT)
  set(_caligo_runtime_candidates "")
  if(DEFINED ENV{DOTNET_ROOT})
    list(APPEND _caligo_runtime_candidates "$ENV{DOTNET_ROOT}")
  endif()
  get_filename_component(_caligo_dotnet_directory "${CALIGO_DOTNET_EXECUTABLE}" DIRECTORY)
  list(APPEND _caligo_runtime_candidates "${_caligo_dotnet_directory}")

  foreach(_caligo_candidate IN LISTS _caligo_runtime_candidates)
    if(EXISTS "${_caligo_candidate}/host/fxr"
       AND EXISTS "${_caligo_candidate}/shared/Microsoft.NETCore.App")
      set(CALIGO_DOTNET_ROOT "${_caligo_candidate}" CACHE PATH "Private .NET runtime root" FORCE)
      break()
    endif()
  endforeach()
endif()

if(NOT CALIGO_DOTNET_ROOT)
  message(FATAL_ERROR "The .NET runtime root could not be located. Set CALIGO_DOTNET_ROOT.")
endif()
if(NOT EXISTS "${CALIGO_DOTNET_ROOT}/host/fxr"
   OR NOT EXISTS "${CALIGO_DOTNET_ROOT}/shared/Microsoft.NETCore.App")
  message(FATAL_ERROR "CALIGO_DOTNET_ROOT does not contain a .NET runtime: ${CALIGO_DOTNET_ROOT}")
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
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CALIGO_DOTNET_ROOT}/host/fxr"
            "$<TARGET_FILE_DIR:${target}>/runtime/dotnet/host/fxr"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CALIGO_DOTNET_ROOT}/shared/Microsoft.NETCore.App"
            "$<TARGET_FILE_DIR:${target}>/runtime/dotnet/shared/Microsoft.NETCore.App"
    COMMENT "Copying managed scripts and the private .NET runtime next to ${target}"
    VERBATIM
  )
endfunction()
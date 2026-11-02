add_library(caligo_audio STATIC
  src/AudioClip.cpp
  src/AudioDevice.cpp
  src/MiniaudioImpl.cpp
  src/Mixer.cpp
)

target_include_directories(caligo_audio
  PUBLIC include
  PRIVATE src
)

target_link_libraries(caligo_audio
  PUBLIC caligo_core
  PRIVATE miniaudio ole32 uuid winmm
)

caligo_configure_target(caligo_audio)

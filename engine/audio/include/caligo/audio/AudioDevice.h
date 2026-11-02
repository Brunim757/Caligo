#pragma once

#include <caligo/audio/Mixer.h>
#include <caligo/core/Result.h>
#include <cstdint>
#include <memory>

namespace caligo::audio {

struct AudioDeviceDesc {
  uint32_t sampleRate = 48000;
  uint32_t channels = 2;
  bool headless = false;
};

class AudioDevice final {
public:
  struct Impl;

  [[nodiscard]] static Result<std::unique_ptr<AudioDevice>> Create(const AudioDeviceDesc& desc);

  ~AudioDevice();

  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;
  AudioDevice(AudioDevice&&) = delete;
  AudioDevice& operator=(AudioDevice&&) = delete;

  [[nodiscard]] Result<void> Start();
  [[nodiscard]] Result<void> Stop();
  [[nodiscard]] bool IsRunning() const noexcept;

  [[nodiscard]] Mixer& GetMixer() noexcept;
  [[nodiscard]] const Mixer& GetMixer() const noexcept;
  [[nodiscard]] uint32_t SampleRate() const noexcept;
  [[nodiscard]] uint32_t Channels() const noexcept;

private:
  AudioDevice(uint32_t sampleRate, uint32_t channels);

  std::unique_ptr<Impl> m_impl;
};

}

#pragma once

#include <caligo/core/Result.h>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace caligo::audio {

class AudioClip final {
public:
  struct Impl;

  [[nodiscard]] static Result<std::shared_ptr<AudioClip>>
  CreateFromPcm(std::span<const float> samples, uint32_t channels, uint32_t sampleRate);
  [[nodiscard]] static Result<std::shared_ptr<AudioClip>> CreateFromFile(const std::string& path);

  AudioClip();
  ~AudioClip();

  AudioClip(const AudioClip&) = delete;
  AudioClip& operator=(const AudioClip&) = delete;
  AudioClip(AudioClip&&) = delete;
  AudioClip& operator=(AudioClip&&) = delete;

  [[nodiscard]] const float* Samples() const noexcept;
  [[nodiscard]] uint64_t FrameCount() const noexcept;
  [[nodiscard]] uint32_t Channels() const noexcept;
  [[nodiscard]] uint32_t SampleRate() const noexcept;
  [[nodiscard]] float DurationSeconds() const noexcept;
  [[nodiscard]] float Sample(uint64_t frame, uint32_t channel) const noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}

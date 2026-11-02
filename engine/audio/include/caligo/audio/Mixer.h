#pragma once

#include <caligo/audio/AudioClip.h>
#include <caligo/core/Math.h>
#include <cstdint>
#include <memory>

namespace caligo::audio {

struct AudioListener {
  math::Vec3 position{0.0f, 0.0f, 0.0f};
  math::Vec3 forward{0.0f, 0.0f, 1.0f};
  math::Vec3 up{0.0f, 1.0f, 0.0f};
};

struct AudioVoiceDesc {
  std::shared_ptr<const AudioClip> clip;
  math::Vec3 position{0.0f, 0.0f, 0.0f};
  float volume = 1.0f;
  float pitch = 1.0f;
  float minDistance = 1.0f;
  float maxDistance = 100.0f;
  bool spatial = true;
  bool loop = false;
};

class Mixer final {
public:
  using VoiceId = uint64_t;
  static constexpr VoiceId kInvalidVoice = 0;

  explicit Mixer(uint32_t sampleRate = 48000, uint32_t channels = 2);
  ~Mixer();

  Mixer(const Mixer&) = delete;
  Mixer& operator=(const Mixer&) = delete;
  Mixer(Mixer&&) = delete;
  Mixer& operator=(Mixer&&) = delete;

  [[nodiscard]] VoiceId Play(const AudioVoiceDesc& voice);
  void Stop(VoiceId voice);
  void StopAll();
  [[nodiscard]] bool IsPlaying(VoiceId voice) const;
  void SetListener(const AudioListener& listener);
  [[nodiscard]] AudioListener Listener() const;

  void Mix(float* output, uint32_t frameCount);

  [[nodiscard]] uint32_t ActiveVoiceCount() const;
  [[nodiscard]] uint32_t SampleRate() const noexcept;
  [[nodiscard]] uint32_t Channels() const noexcept;
  [[nodiscard]] uint64_t RenderedFrames() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}

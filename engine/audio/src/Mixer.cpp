#include <algorithm>
#include <atomic>
#include <caligo/audio/Mixer.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

namespace caligo::audio {

namespace {

constexpr uint32_t kMaximumClipChannels = 8;

struct Gains {
  float distance = 1.0f;
  float left = 1.0f;
  float right = 1.0f;
};

Gains ComputeGains(const AudioListener& listener, const math::Vec3& source, float minDistance,
                   float maxDistance, bool spatial) {
  Gains gains;
  if (!spatial) {
    return gains;
  }

  const math::Vec3 offset = math::Subtract(source, listener.position);
  const float distance = math::Length(offset);
  const float near = std::max(minDistance, 0.0f);
  const float far = std::max(maxDistance, near + math::kEpsilon);
  if (distance >= far) {
    return Gains{0.0f, 0.0f, 0.0f};
  }

  const float reference = std::max(distance, near);
  const float fade = math::Clamp((far - distance) / (far - near), 0.0f, 1.0f);
  gains.distance = (near / reference) * fade;

  math::Vec3 direction = listener.forward;
  if (distance > math::kEpsilon) {
    direction = math::Scale(offset, 1.0f / distance);
  }
  math::Vec3 right = math::Cross(listener.up, listener.forward);
  if (math::Length(right) <= math::kEpsilon) {
    right = math::Vec3{1.0f, 0.0f, 0.0f};
  } else {
    right = math::Normalize(right);
  }
  const float pan = math::Clamp(math::Dot(direction, right), -1.0f, 1.0f);
  const float angle = (pan + 1.0f) * 0.25f * math::kPi;
  gains.left = std::cos(angle);
  gains.right = std::sin(angle);
  return gains;
}

}

struct Mixer::Impl {
  struct Voice {
    std::shared_ptr<const AudioClip> clip;
    math::Vec3 position{};
    double frame = 0.0;
    double step = 1.0;
    float volume = 1.0f;
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    uint32_t generation = 1;
    bool spatial = true;
    bool loop = false;
    bool active = false;
  };

  uint32_t sampleRate = 48000;
  uint32_t channels = 2;
  AudioListener listener;
  std::vector<Voice> voices;
  std::vector<uint32_t> freeIndices;
  std::atomic<uint64_t> renderedFrames{0};
  mutable std::mutex mutex;
};

Mixer::Mixer(uint32_t sampleRate, uint32_t channels) : m_impl(std::make_unique<Impl>()) {
  m_impl->sampleRate = sampleRate == 0 ? 48000 : sampleRate;
  m_impl->channels = channels == 1 ? 1 : 2;
}

Mixer::~Mixer() = default;

Mixer::VoiceId Mixer::Play(const AudioVoiceDesc& voice) {
  if (voice.clip == nullptr || voice.clip->FrameCount() == 0) {
    return kInvalidVoice;
  }

  std::lock_guard<std::mutex> lock(m_impl->mutex);
  uint32_t index = 0;
  if (!m_impl->freeIndices.empty()) {
    index = m_impl->freeIndices.back();
    m_impl->freeIndices.pop_back();
  } else {
    m_impl->voices.emplace_back();
    index = static_cast<uint32_t>(m_impl->voices.size() - 1);
  }

  Impl::Voice& slot = m_impl->voices[index];
  slot.clip = voice.clip;
  slot.position = voice.position;
  slot.frame = 0.0;
  const double pitch = voice.pitch > 0.0f ? static_cast<double>(voice.pitch) : 1.0;
  slot.step = pitch * static_cast<double>(voice.clip->SampleRate()) /
              static_cast<double>(m_impl->sampleRate);
  slot.volume = voice.volume;
  slot.minDistance = voice.minDistance;
  slot.maxDistance = voice.maxDistance;
  slot.spatial = voice.spatial;
  slot.loop = voice.loop;
  slot.active = true;
  ++slot.generation;
  return (static_cast<VoiceId>(slot.generation) << 32) | static_cast<VoiceId>(index);
}

void Mixer::Stop(VoiceId voice) {
  std::lock_guard<std::mutex> lock(m_impl->mutex);
  const uint32_t index = static_cast<uint32_t>(voice & 0xFFFFFFFFu);
  const uint32_t generation = static_cast<uint32_t>(voice >> 32);
  if (index >= m_impl->voices.size()) {
    return;
  }
  Impl::Voice& slot = m_impl->voices[index];
  if (!slot.active || slot.generation != generation) {
    return;
  }
  slot.active = false;
  slot.clip.reset();
  m_impl->freeIndices.push_back(index);
}

void Mixer::StopAll() {
  std::lock_guard<std::mutex> lock(m_impl->mutex);
  for (uint32_t index = 0; index < m_impl->voices.size(); ++index) {
    if (m_impl->voices[index].active) {
      m_impl->voices[index].active = false;
      m_impl->voices[index].clip.reset();
      m_impl->freeIndices.push_back(index);
    }
  }
}

bool Mixer::IsPlaying(VoiceId voice) const {
  std::lock_guard<std::mutex> lock(m_impl->mutex);
  const uint32_t index = static_cast<uint32_t>(voice & 0xFFFFFFFFu);
  const uint32_t generation = static_cast<uint32_t>(voice >> 32);
  if (index >= m_impl->voices.size()) {
    return false;
  }
  const Impl::Voice& slot = m_impl->voices[index];
  return slot.active && slot.generation == generation;
}

void Mixer::SetListener(const AudioListener& listener) {
  std::lock_guard<std::mutex> lock(m_impl->mutex);
  m_impl->listener = listener;
}

AudioListener Mixer::Listener() const {
  std::lock_guard<std::mutex> lock(m_impl->mutex);
  return m_impl->listener;
}

void Mixer::Mix(float* output, uint32_t frameCount) {
  if (output == nullptr || frameCount == 0) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_impl->mutex);
  const uint32_t channels = m_impl->channels;
  const size_t total = static_cast<size_t>(frameCount) * channels;
  for (size_t sample = 0; sample < total; ++sample) {
    output[sample] = 0.0f;
  }

  for (uint32_t index = 0; index < m_impl->voices.size(); ++index) {
    Impl::Voice& slot = m_impl->voices[index];
    if (!slot.active || slot.clip == nullptr) {
      continue;
    }

    const AudioClip& clip = *slot.clip;
    const uint64_t clipFrames = clip.FrameCount();
    if (clipFrames == 0) {
      slot.active = false;
      slot.clip.reset();
      m_impl->freeIndices.push_back(index);
      continue;
    }

    const uint32_t clipChannels = clip.Channels();
    const Gains gains = ComputeGains(m_impl->listener, slot.position, slot.minDistance,
                                     slot.maxDistance, slot.spatial);
    bool finished = false;

    for (uint32_t frame = 0; frame < frameCount; ++frame) {
      if (!slot.loop && slot.frame >= static_cast<double>(clipFrames)) {
        finished = true;
        break;
      }

      double wrapped = slot.frame;
      if (slot.loop) {
        wrapped = std::fmod(wrapped, static_cast<double>(clipFrames));
      }
      const uint64_t base = static_cast<uint64_t>(wrapped);
      const float fraction = static_cast<float>(wrapped - static_cast<double>(base));
      uint64_t next = base + 1;
      if (next >= clipFrames) {
        next = slot.loop ? 0 : clipFrames - 1;
      }

      float source[kMaximumClipChannels] = {};
      for (uint32_t channel = 0; channel < clipChannels; ++channel) {
        const float first = clip.Sample(base, channel);
        const float second = clip.Sample(next, channel);
        source[channel] = first + (second - first) * fraction;
      }

      if (channels == 1) {
        float sum = 0.0f;
        for (uint32_t channel = 0; channel < clipChannels; ++channel) {
          sum += source[channel];
        }
        output[frame] += (sum / static_cast<float>(clipChannels)) * gains.distance * slot.volume;
      } else if (clipChannels == 1) {
        output[static_cast<size_t>(frame) * 2] += source[0] * gains.left * slot.volume;
        output[static_cast<size_t>(frame) * 2 + 1] += source[0] * gains.right * slot.volume;
      } else {
        output[static_cast<size_t>(frame) * 2] += source[0] * gains.left * slot.volume;
        output[static_cast<size_t>(frame) * 2 + 1] += source[1] * gains.right * slot.volume;
      }

      slot.frame += slot.step;
    }

    if (finished) {
      slot.active = false;
      slot.clip.reset();
      m_impl->freeIndices.push_back(index);
    }
  }

  for (size_t sample = 0; sample < total; ++sample) {
    output[sample] = math::Clamp(output[sample], -1.0f, 1.0f);
  }
  m_impl->renderedFrames.fetch_add(frameCount, std::memory_order_relaxed);
}

uint32_t Mixer::ActiveVoiceCount() const {
  std::lock_guard<std::mutex> lock(m_impl->mutex);
  uint32_t count = 0;
  for (const Impl::Voice& slot : m_impl->voices) {
    if (slot.active) {
      ++count;
    }
  }
  return count;
}

uint32_t Mixer::SampleRate() const noexcept {
  return m_impl->sampleRate;
}

uint32_t Mixer::Channels() const noexcept {
  return m_impl->channels;
}

uint64_t Mixer::RenderedFrames() const noexcept {
  return m_impl->renderedFrames.load(std::memory_order_relaxed);
}

}

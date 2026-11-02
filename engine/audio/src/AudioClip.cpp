#include "MiniaudioApi.h"
#include <caligo/audio/AudioClip.h>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace caligo::audio {

namespace {

constexpr uint32_t kDecodeFrames = 4096;
constexpr uint32_t kMaximumChannels = 8;

}

struct AudioClip::Impl {
  std::vector<float> samples;
  uint32_t channels = 0;
  uint32_t sampleRate = 0;
};

AudioClip::AudioClip() : m_impl(std::make_unique<Impl>()) {}

AudioClip::~AudioClip() = default;

Result<std::shared_ptr<AudioClip>>
AudioClip::CreateFromPcm(std::span<const float> samples, uint32_t channels, uint32_t sampleRate) {
  if (channels == 0 || channels > kMaximumChannels) {
    return Error{"audio clip needs between one and eight channels", 1};
  }
  if (sampleRate == 0) {
    return Error{"audio clip needs a positive sample rate", 2};
  }
  if (samples.empty() || samples.size() % channels != 0) {
    return Error{"audio clip samples must be a non-empty multiple of the channel count", 3};
  }
  auto clip = std::make_shared<AudioClip>();
  clip->m_impl->samples.assign(samples.begin(), samples.end());
  clip->m_impl->channels = channels;
  clip->m_impl->sampleRate = sampleRate;
  return clip;
}

Result<std::shared_ptr<AudioClip>> AudioClip::CreateFromFile(const std::string& path) {
  const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
  ma_decoder decoder{};
  if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS) {
    return Error{"audio clip could not open the file", 1};
  }

  const uint32_t channels = decoder.outputChannels;
  const uint32_t sampleRate = decoder.outputSampleRate;
  if (channels == 0 || channels > kMaximumChannels) {
    ma_decoder_uninit(&decoder);
    return Error{"audio clip file has an unsupported channel count", 3};
  }
  std::vector<float> samples;
  std::vector<float> buffer(static_cast<size_t>(kDecodeFrames) * channels);
  while (true) {
    const ma_uint64 read =
        ma_decoder_read_pcm_frames(&decoder, buffer.data(), kDecodeFrames, nullptr);
    if (read == 0) {
      break;
    }
    samples.insert(samples.end(), buffer.begin(),
                   buffer.begin() + static_cast<size_t>(read) * channels);
  }
  ma_decoder_uninit(&decoder);

  if (samples.empty()) {
    return Error{"audio clip file had no samples", 2};
  }

  auto clip = std::make_shared<AudioClip>();
  clip->m_impl->samples = std::move(samples);
  clip->m_impl->channels = channels;
  clip->m_impl->sampleRate = sampleRate;
  return clip;
}

const float* AudioClip::Samples() const noexcept {
  return m_impl->samples.data();
}

uint64_t AudioClip::FrameCount() const noexcept {
  if (m_impl->channels == 0) {
    return 0;
  }
  return m_impl->samples.size() / m_impl->channels;
}

uint32_t AudioClip::Channels() const noexcept {
  return m_impl->channels;
}

uint32_t AudioClip::SampleRate() const noexcept {
  return m_impl->sampleRate;
}

float AudioClip::DurationSeconds() const noexcept {
  if (m_impl->sampleRate == 0) {
    return 0.0f;
  }
  return static_cast<float>(FrameCount()) / static_cast<float>(m_impl->sampleRate);
}

float AudioClip::Sample(uint64_t frame, uint32_t channel) const noexcept {
  if (channel >= m_impl->channels || frame >= FrameCount()) {
    return 0.0f;
  }
  return m_impl->samples[static_cast<size_t>(frame) * m_impl->channels + channel];
}

}

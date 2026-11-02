#include <windows.h>

#include "Miniaudio.h"
#include <caligo/audio/AudioDevice.h>
#include <memory>
#include <utility>

namespace caligo::audio {

namespace {

constexpr uint32_t kMaximumChannels = 2;

}

struct AudioDevice::Impl {
  Impl(uint32_t sampleRate, uint32_t channels) : mixer(sampleRate, channels) {}

  Mixer mixer;
  ma_device device{};
  ma_context context{};
  bool hasContext = false;
  bool deviceInitialized = false;
  bool running = false;
};

namespace {

void OnAudioData(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
  static_cast<void>(input);
  auto* self = static_cast<AudioDevice::Impl*>(device->pUserData);
  if (self == nullptr || output == nullptr) {
    return;
  }
  self->mixer.Mix(static_cast<float*>(output), frameCount);
}

}

AudioDevice::AudioDevice(uint32_t sampleRate, uint32_t channels)
    : m_impl(std::make_unique<Impl>(sampleRate, channels)) {}

AudioDevice::~AudioDevice() {
  if (m_impl->running) {
    ma_device_stop(&m_impl->device);
  }
  if (m_impl->deviceInitialized) {
    ma_device_uninit(&m_impl->device);
  }
  if (m_impl->hasContext) {
    ma_context_uninit(&m_impl->context);
  }
}

Result<std::unique_ptr<AudioDevice>> AudioDevice::Create(const AudioDeviceDesc& desc) {
  if (desc.channels == 0 || desc.channels > kMaximumChannels) {
    return Error{"audio device supports one or two channels", 1};
  }
  if (desc.sampleRate == 0) {
    return Error{"audio device needs a positive sample rate", 2};
  }

  auto device = std::unique_ptr<AudioDevice>(new AudioDevice(desc.sampleRate, desc.channels));

  ma_context* context = nullptr;
  if (desc.headless) {
    ma_backend backends[] = {ma_backend_null};
    if (ma_context_init(backends, 1, nullptr, &device->m_impl->context) != MA_SUCCESS) {
      return Error{"audio device could not create the headless context", 3};
    }
    device->m_impl->hasContext = true;
    context = &device->m_impl->context;
  }

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = desc.channels;
  config.sampleRate = desc.sampleRate;
  config.dataCallback = &OnAudioData;
  config.pUserData = device->m_impl.get();

  if (ma_device_init(context, &config, &device->m_impl->device) != MA_SUCCESS) {
    if (device->m_impl->hasContext) {
      ma_context_uninit(&device->m_impl->context);
      device->m_impl->hasContext = false;
    }
    return Error{"audio device could not open the playback device", 4};
  }
  device->m_impl->deviceInitialized = true;
  return std::move(device);
}

Result<void> AudioDevice::Start() {
  if (m_impl->running) {
    return {};
  }
  if (!m_impl->deviceInitialized) {
    return Error{"audio device is not initialized", 1};
  }
  if (ma_device_start(&m_impl->device) != MA_SUCCESS) {
    return Error{"audio device could not start", 2};
  }
  m_impl->running = true;
  return {};
}

Result<void> AudioDevice::Stop() {
  if (!m_impl->running) {
    return {};
  }
  ma_device_stop(&m_impl->device);
  m_impl->running = false;
  return {};
}

bool AudioDevice::IsRunning() const noexcept {
  return m_impl->running;
}

Mixer& AudioDevice::GetMixer() noexcept {
  return m_impl->mixer;
}

const Mixer& AudioDevice::GetMixer() const noexcept {
  return m_impl->mixer;
}

uint32_t AudioDevice::SampleRate() const noexcept {
  return m_impl->mixer.SampleRate();
}

uint32_t AudioDevice::Channels() const noexcept {
  return m_impl->mixer.Channels();
}

}

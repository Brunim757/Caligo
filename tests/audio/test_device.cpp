#include <caligo/audio/AudioDevice.h>
#include <doctest/doctest.h>

TEST_CASE("um dispositivo de audio sem hardware abre e roda") {
  caligo::audio::AudioDeviceDesc desc;
  desc.headless = true;
  const auto device = caligo::audio::AudioDevice::Create(desc);
  REQUIRE(device.HasValue());
  CHECK(device.Value()->SampleRate() == 48000);
  CHECK(device.Value()->Channels() == 2);
  CHECK_FALSE(device.Value()->IsRunning());
  REQUIRE(device.Value()->Start().HasValue());
  CHECK(device.Value()->IsRunning());
  REQUIRE(device.Value()->Stop().HasValue());
  CHECK_FALSE(device.Value()->IsRunning());
}

TEST_CASE("um dispositivo de audio respeita a taxa e os canais pedidos") {
  caligo::audio::AudioDeviceDesc desc;
  desc.headless = true;
  desc.sampleRate = 44100;
  desc.channels = 1;
  const auto device = caligo::audio::AudioDevice::Create(desc);
  REQUIRE(device.HasValue());
  CHECK(device.Value()->SampleRate() == 44100);
  CHECK(device.Value()->Channels() == 1);
  CHECK(device.Value()->GetMixer().SampleRate() == 44100);
  CHECK(device.Value()->GetMixer().Channels() == 1);
}

TEST_CASE("um dispositivo de audio recusa formatos invalidos") {
  caligo::audio::AudioDeviceDesc desc;
  desc.headless = true;
  desc.channels = 4;
  CHECK_FALSE(caligo::audio::AudioDevice::Create(desc).HasValue());
  desc.channels = 2;
  desc.sampleRate = 0;
  CHECK_FALSE(caligo::audio::AudioDevice::Create(desc).HasValue());
}

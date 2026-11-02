#include <caligo/audio/AudioClip.h>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

void WriteU16(std::ofstream& file, uint16_t value) {
  const char bytes[2] = {static_cast<char>(value & 0xFFu), static_cast<char>((value >> 8) & 0xFFu)};
  file.write(bytes, 2);
}

void WriteU32(std::ofstream& file, uint32_t value) {
  const char bytes[4] = {static_cast<char>(value & 0xFFu), static_cast<char>((value >> 8) & 0xFFu),
                         static_cast<char>((value >> 16) & 0xFFu),
                         static_cast<char>((value >> 24) & 0xFFu)};
  file.write(bytes, 4);
}

std::string WriteMonoWav(const std::vector<int16_t>& samples, uint32_t sampleRate) {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "caligo_audio_clip_test.wav";
  std::ofstream file(path, std::ios::binary);
  const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
  file.write("RIFF", 4);
  WriteU32(file, 36 + dataBytes);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  WriteU32(file, 16);
  WriteU16(file, 1);
  WriteU16(file, 1);
  WriteU32(file, sampleRate);
  WriteU32(file, sampleRate * static_cast<uint32_t>(sizeof(int16_t)));
  WriteU16(file, static_cast<uint16_t>(sizeof(int16_t)));
  WriteU16(file, 16);
  file.write("data", 4);
  WriteU32(file, dataBytes);
  for (int16_t sample : samples) {
    WriteU16(file, static_cast<uint16_t>(sample));
  }
  file.close();
  return path.string();
}

}

TEST_CASE("um clipe de amostras cruas expoe o formato e as amostras") {
  const std::vector<float> samples{0.0f, 0.25f, 0.5f, 0.75f, 1.0f, -1.0f};
  const caligo::Result<std::shared_ptr<caligo::audio::AudioClip>> clip =
      caligo::audio::AudioClip::CreateFromPcm(samples, 2, 48000);
  REQUIRE(clip.HasValue());
  CHECK(clip.Value()->FrameCount() == 3);
  CHECK(clip.Value()->Channels() == 2);
  CHECK(clip.Value()->SampleRate() == 48000);
  CHECK(clip.Value()->Sample(0, 0) == doctest::Approx(0.0f));
  CHECK(clip.Value()->Sample(1, 1) == doctest::Approx(0.75f));
  CHECK(clip.Value()->Sample(2, 0) == doctest::Approx(1.0f));
  CHECK(clip.Value()->DurationSeconds() == doctest::Approx(3.0f / 48000.0f));
}

TEST_CASE("um clipe de amostras cruas recusa formatos invalidos") {
  const std::vector<float> samples{0.0f, 0.5f, 1.0f};
  CHECK_FALSE(caligo::audio::AudioClip::CreateFromPcm(samples, 0, 48000).HasValue());
  CHECK_FALSE(caligo::audio::AudioClip::CreateFromPcm(samples, 2, 0).HasValue());
  CHECK_FALSE(caligo::audio::AudioClip::CreateFromPcm(samples, 2, 48000).HasValue());
  CHECK_FALSE(
      caligo::audio::AudioClip::CreateFromPcm(std::span<const float>{}, 1, 48000).HasValue());
}

TEST_CASE("um clipe devolve silencio fora dos limites") {
  const std::vector<float> samples{0.25f, 0.5f};
  const auto clip = caligo::audio::AudioClip::CreateFromPcm(samples, 1, 44100);
  REQUIRE(clip.HasValue());
  CHECK(clip.Value()->Sample(2, 0) == doctest::Approx(0.0f));
  CHECK(clip.Value()->Sample(0, 1) == doctest::Approx(0.0f));
}

TEST_CASE("um clipe carrega amostras de um arquivo wav") {
  const std::vector<int16_t> samples{0, 16384, -16384, 32767};
  const std::string path = WriteMonoWav(samples, 22050);
  const auto clip = caligo::audio::AudioClip::CreateFromFile(path);
  REQUIRE(clip.HasValue());
  CHECK(clip.Value()->Channels() == 1);
  CHECK(clip.Value()->SampleRate() == 22050);
  CHECK(clip.Value()->FrameCount() == 4);
  CHECK(clip.Value()->Sample(1, 0) == doctest::Approx(0.5f).epsilon(0.01));
  CHECK(clip.Value()->Sample(3, 0) == doctest::Approx(1.0f).epsilon(0.01));
}

TEST_CASE("um clipe recusa um arquivo inexistente") {
  CHECK_FALSE(caligo::audio::AudioClip::CreateFromFile("nao_existe.wav").HasValue());
}

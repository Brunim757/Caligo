#include <caligo/audio/Mixer.h>
#include <cmath>
#include <cstdint>
#include <doctest/doctest.h>
#include <memory>
#include <vector>

namespace {

std::shared_ptr<const caligo::audio::AudioClip> MakeClip(const std::vector<float>& samples,
                                                         uint32_t channels, uint32_t sampleRate) {
  const auto clip = caligo::audio::AudioClip::CreateFromPcm(samples, channels, sampleRate);
  REQUIRE(clip.HasValue());
  return clip.Value();
}

}

TEST_CASE("um som sem espacializacao toca igual nos dois canais") {
  caligo::audio::Mixer mixer(48000, 2);
  caligo::audio::AudioVoiceDesc voice;
  voice.clip = MakeClip(std::vector<float>(480, 1.0f), 1, 48000);
  voice.spatial = false;
  voice.volume = 0.5f;

  const caligo::audio::Mixer::VoiceId id = mixer.Play(voice);
  CHECK(id != caligo::audio::Mixer::kInvalidVoice);
  REQUIRE(mixer.IsPlaying(id));
  CHECK(mixer.ActiveVoiceCount() == 1);

  std::vector<float> output(10 * 2, 0.0f);
  mixer.Mix(output.data(), 10);
  CHECK(output[0] == doctest::Approx(0.5f));
  CHECK(output[1] == doctest::Approx(0.5f));
  CHECK(mixer.RenderedFrames() == 10);

  mixer.Stop(id);
  CHECK_FALSE(mixer.IsPlaying(id));
  CHECK(mixer.ActiveVoiceCount() == 0);
}

TEST_CASE("um som termina quando o clipe acaba") {
  caligo::audio::Mixer mixer(48000, 2);
  caligo::audio::AudioVoiceDesc voice;
  voice.clip = MakeClip(std::vector<float>(4, 1.0f), 1, 48000);
  voice.spatial = false;

  const caligo::audio::Mixer::VoiceId id = mixer.Play(voice);
  std::vector<float> output(10 * 2, 0.0f);
  mixer.Mix(output.data(), 10);
  CHECK_FALSE(mixer.IsPlaying(id));
  CHECK(mixer.ActiveVoiceCount() == 0);
  CHECK(output[0] == doctest::Approx(1.0f));
  CHECK(output[6] == doctest::Approx(1.0f));
  CHECK(output[8] == doctest::Approx(0.0f));
}

TEST_CASE("um som em repeticao continua tocando") {
  caligo::audio::Mixer mixer(48000, 2);
  caligo::audio::AudioVoiceDesc voice;
  voice.clip = MakeClip(std::vector<float>(4, 1.0f), 1, 48000);
  voice.spatial = false;
  voice.loop = true;

  const caligo::audio::Mixer::VoiceId id = mixer.Play(voice);
  std::vector<float> output(10 * 2, 0.0f);
  mixer.Mix(output.data(), 10);
  CHECK(mixer.IsPlaying(id));
  CHECK(output[0] == doctest::Approx(1.0f));
  CHECK(output[18] == doctest::Approx(1.0f));
}

TEST_CASE("a distancia atenua o som e o silencia no limite") {
  caligo::audio::Mixer mixer(48000, 2);
  caligo::audio::AudioVoiceDesc voice;
  voice.clip = MakeClip(std::vector<float>(480, 1.0f), 1, 48000);
  voice.spatial = true;

  voice.position = caligo::math::Vec3{0.0f, 0.0f, 0.0f};
  const caligo::audio::Mixer::VoiceId nearVoice = mixer.Play(voice);
  std::vector<float> nearOutput(2, 0.0f);
  mixer.Mix(nearOutput.data(), 1);
  const float nearLevel = nearOutput[0];
  mixer.Stop(nearVoice);

  voice.position = caligo::math::Vec3{0.0f, 0.0f, 10.0f};
  const caligo::audio::Mixer::VoiceId middleVoice = mixer.Play(voice);
  std::vector<float> middleOutput(2, 0.0f);
  mixer.Mix(middleOutput.data(), 1);
  const float middleLevel = middleOutput[0];
  mixer.Stop(middleVoice);

  voice.position = caligo::math::Vec3{0.0f, 0.0f, 200.0f};
  static_cast<void>(mixer.Play(voice));
  std::vector<float> farOutput(2, 0.0f);
  mixer.Mix(farOutput.data(), 1);
  const float farLevel = farOutput[0];

  CHECK(nearLevel == doctest::Approx(0.70710678f));
  CHECK(middleLevel == doctest::Approx(0.06428243f));
  CHECK(farLevel == doctest::Approx(0.0f));
  CHECK(nearLevel > middleLevel);
  CHECK(middleLevel > farLevel);
}

TEST_CASE("o som a direita pesa no canal direito") {
  caligo::audio::Mixer mixer(48000, 2);
  caligo::audio::AudioVoiceDesc voice;
  voice.clip = MakeClip(std::vector<float>(480, 1.0f), 1, 48000);
  voice.position = caligo::math::Vec3{5.0f, 0.0f, 0.0f};

  static_cast<void>(mixer.Play(voice));
  std::vector<float> output(2, 0.0f);
  mixer.Mix(output.data(), 1);
  CHECK(output[1] > output[0]);
  CHECK(std::abs(output[0]) < 1.0e-3f);
  CHECK(output[1] == doctest::Approx(0.19191919f));
}

TEST_CASE("um som estereo mantem os canais alinhados") {
  caligo::audio::Mixer mixer(48000, 2);
  caligo::audio::AudioVoiceDesc voice;
  voice.clip = MakeClip(std::vector<float>{1.0f, 0.0f}, 2, 48000);
  voice.spatial = false;

  static_cast<void>(mixer.Play(voice));
  std::vector<float> output(2, 0.0f);
  mixer.Mix(output.data(), 1);
  CHECK(output[0] == doctest::Approx(1.0f));
  CHECK(output[1] == doctest::Approx(0.0f));
}

TEST_CASE("um som estereo em um mixer mono e somado") {
  caligo::audio::Mixer mixer(48000, 1);
  caligo::audio::AudioVoiceDesc voice;
  voice.clip = MakeClip(std::vector<float>{1.0f, 0.0f}, 2, 48000);
  voice.spatial = false;

  static_cast<void>(mixer.Play(voice));
  std::vector<float> output(1, 0.0f);
  mixer.Mix(output.data(), 1);
  CHECK(output[0] == doctest::Approx(0.5f));
}

TEST_CASE("a taxa de amostragem do clipe controla a velocidade") {
  caligo::audio::Mixer mixer(48000, 2);
  caligo::audio::AudioVoiceDesc voice;
  voice.clip = MakeClip(std::vector<float>{0.0f, 0.25f, 0.5f, 0.75f}, 1, 24000);
  voice.spatial = false;

  static_cast<void>(mixer.Play(voice));
  std::vector<float> output(4 * 2, 0.0f);
  mixer.Mix(output.data(), 4);
  CHECK(output[0] == doctest::Approx(0.0f));
  CHECK(output[2] == doctest::Approx(0.125f));
  CHECK(output[4] == doctest::Approx(0.25f));
  CHECK(output[6] == doctest::Approx(0.375f));
}

TEST_CASE("a soma de varios sons fica limitada") {
  caligo::audio::Mixer mixer(48000, 2);
  caligo::audio::AudioVoiceDesc voice;
  voice.clip = MakeClip(std::vector<float>(480, 1.0f), 1, 48000);
  voice.spatial = false;

  static_cast<void>(mixer.Play(voice));
  static_cast<void>(mixer.Play(voice));
  std::vector<float> output(2, 0.0f);
  mixer.Mix(output.data(), 1);
  CHECK(output[0] == doctest::Approx(1.0f));
  CHECK(output[1] == doctest::Approx(1.0f));
}

TEST_CASE("um som sem clipe nao comeca") {
  caligo::audio::Mixer mixer(48000, 2);
  caligo::audio::AudioVoiceDesc voice;
  CHECK(mixer.Play(voice) == caligo::audio::Mixer::kInvalidVoice);
  mixer.Stop(caligo::audio::Mixer::kInvalidVoice);
  CHECK(mixer.ActiveVoiceCount() == 0);
}

TEST_CASE("parar todos os sons limpa o mixer") {
  caligo::audio::Mixer mixer(48000, 2);
  caligo::audio::AudioVoiceDesc voice;
  voice.clip = MakeClip(std::vector<float>(480, 1.0f), 1, 48000);
  static_cast<void>(mixer.Play(voice));
  static_cast<void>(mixer.Play(voice));
  CHECK(mixer.ActiveVoiceCount() == 2);
  mixer.StopAll();
  CHECK(mixer.ActiveVoiceCount() == 0);
}

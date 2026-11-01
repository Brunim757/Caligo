#pragma once

#include <array>
#include <caligo/core/Result.h>
#include <caligo/scene/Scene.h>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace caligo::script {

enum class ScriptKey : int {
  W = 0,
  A,
  S,
  D,
  Space,
  LeftShift,
  Up,
  Down,
  Left,
  Right,
  Escape,
  Count
};

inline constexpr size_t kScriptKeyCount = static_cast<size_t>(ScriptKey::Count);

struct ScriptInput {
  std::array<bool, kScriptKeyCount> down{};
  std::array<bool, kScriptKeyCount> pressed{};
};

class ScriptHost final {
public:
  ScriptHost();
  ~ScriptHost();

  ScriptHost(const ScriptHost&) = delete;
  ScriptHost& operator=(const ScriptHost&) = delete;
  ScriptHost(ScriptHost&&) = delete;
  ScriptHost& operator=(ScriptHost&&) = delete;

  [[nodiscard]] Result<void> Initialize(const std::filesystem::path& executableDirectory);
  [[nodiscard]] Result<void> Reload();
  void Shutdown() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;
  [[nodiscard]] const std::string& LastError() const noexcept;

  [[nodiscard]] Result<std::vector<std::string>> ScriptTypes() const;

  [[nodiscard]] Result<void> BeginPlay(scene::Scene& scene);
  [[nodiscard]] Result<void> Tick(float deltaSeconds);
  void EndPlay() noexcept;

  void SetInput(const ScriptInput* input) noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}

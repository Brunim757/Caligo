#include "HostFxr.h"
#include "NativeScriptBindings.h"
#include <array>
#include <caligo/core/Log.h>
#include <caligo/script/ScriptHost.h>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace caligo::script {

namespace {

using InitializeFn = int32_t (*)(const NativeScriptBindings*, const char*);
using CountFn = int32_t (*)();
using NameFn = int32_t (*)(int32_t, char*, int32_t);
using CreateFn = int32_t (*)(uint64_t, const char*, int32_t);
using UpdateFn = int32_t (*)(float);
using PlainFn = int32_t (*)();
using ErrorFn = int32_t (*)(char*, int32_t);

constexpr std::wstring_view kLoaderType =
    L"Caligo.Scripting.Loader.Bootstrap, Caligo.Scripting.Loader";

std::string PathText(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

template <typename Function>
bool ResolveEntry(const HostFxr& hostFxr, const std::filesystem::path& assembly,
                  const wchar_t* method, Function& out) {
  const Result<HostFxr::EntryPoint> entry =
      hostFxr.Resolve(assembly, std::wstring(kLoaderType), method);
  if (!entry) {
    return false;
  }
#pragma warning(suppress : 4191)
  out = reinterpret_cast<Function>(entry.Value());
  return true;
}

}

struct ScriptHost::Impl {
  std::filesystem::path executableDirectory;
  std::filesystem::path runtimeDirectory;
  std::filesystem::path hostDirectory;
  std::filesystem::path gameplayDirectory;
  std::filesystem::path loaderAssembly;
  HostFxr hostFxr;

  InitializeFn initialize = nullptr;
  CountFn scriptTypeCount = nullptr;
  NameFn scriptTypeName = nullptr;
  CreateFn createScript = nullptr;
  PlainFn start = nullptr;
  UpdateFn update = nullptr;
  PlainFn stop = nullptr;
  PlainFn reset = nullptr;
  ErrorFn lastError = nullptr;

  scene::Scene* scene = nullptr;
  const ScriptInput* input = nullptr;
  bool runtimeLoaded = false;
  bool ready = false;
  bool worldStarted = false;
  std::string message;

  static int32_t OnIsKeyDown(void* user, int32_t key);
  static int32_t OnIsKeyPressed(void* user, int32_t key);
  static int32_t OnGetTransform(void* user, uint64_t entity, float* position, float* rotation,
                                float* scale);
  static int32_t OnSetTransform(void* user, uint64_t entity, const float* position,
                                const float* rotation, const float* scale);
  static void OnLog(void* user, int32_t severity, const char* text, int32_t length);

  [[nodiscard]] std::string ReadManagedError() const {
    if (lastError == nullptr) {
      return message;
    }
    std::array<char, 1024> buffer{};
    const int32_t written = lastError(buffer.data(), static_cast<int32_t>(buffer.size()));
    if (written <= 0) {
      return message;
    }
    return std::string(buffer.data(), static_cast<size_t>(written));
  }

  [[nodiscard]] Result<void> InitializeManaged() {
    NativeScriptBindings bindings;
    bindings.user = this;
    bindings.isKeyDown = &Impl::OnIsKeyDown;
    bindings.isKeyPressed = &Impl::OnIsKeyPressed;
    bindings.getTransform = &Impl::OnGetTransform;
    bindings.setTransform = &Impl::OnSetTransform;
    bindings.log = &Impl::OnLog;
    const std::string path = PathText(gameplayDirectory);
    if (initialize(&bindings, path.c_str()) != 0) {
      message = ReadManagedError();
      ready = false;
      return Error{message.empty() ? "script host refused the gameplay module" : message, 20};
    }
    ready = true;
    message.clear();
    return {};
  }

  void ResetManaged() noexcept {
    if (reset != nullptr) {
      reset();
    }
    ready = false;
    worldStarted = false;
    scene = nullptr;
    input = nullptr;
  }
};

int32_t ScriptHost::Impl::OnIsKeyDown(void* user, int32_t key) {
  const auto* self = static_cast<const Impl*>(user);
  if (self == nullptr || self->input == nullptr) {
    return 0;
  }
  const size_t index = static_cast<size_t>(key);
  return index < kScriptKeyCount && self->input->down[index] ? 1 : 0;
}

int32_t ScriptHost::Impl::OnIsKeyPressed(void* user, int32_t key) {
  const auto* self = static_cast<const Impl*>(user);
  if (self == nullptr || self->input == nullptr) {
    return 0;
  }
  const size_t index = static_cast<size_t>(key);
  return index < kScriptKeyCount && self->input->pressed[index] ? 1 : 0;
}

int32_t ScriptHost::Impl::OnGetTransform(void* user, uint64_t entity, float* position,
                                         float* rotation, float* scale) {
  const auto* self = static_cast<const Impl*>(user);
  if (self == nullptr || self->scene == nullptr || position == nullptr || rotation == nullptr ||
      scale == nullptr) {
    return 0;
  }
  const scene::Entity* found = self->scene->Find(entity);
  if (found == nullptr) {
    return 0;
  }
  position[0] = found->transform.position.x;
  position[1] = found->transform.position.y;
  position[2] = found->transform.position.z;
  rotation[0] = found->transform.rotation.x;
  rotation[1] = found->transform.rotation.y;
  rotation[2] = found->transform.rotation.z;
  rotation[3] = found->transform.rotation.w;
  scale[0] = found->transform.scale.x;
  scale[1] = found->transform.scale.y;
  scale[2] = found->transform.scale.z;
  return 1;
}

int32_t ScriptHost::Impl::OnSetTransform(void* user, uint64_t entity, const float* position,
                                         const float* rotation, const float* scale) {
  auto* self = static_cast<Impl*>(user);
  if (self == nullptr || self->scene == nullptr || position == nullptr || rotation == nullptr ||
      scale == nullptr) {
    return 0;
  }
  scene::Transform transform;
  transform.position = math::Vec3{position[0], position[1], position[2]};
  transform.rotation = math::Quat{rotation[0], rotation[1], rotation[2], rotation[3]};
  transform.scale = math::Vec3{scale[0], scale[1], scale[2]};
  return self->scene->SetTransform(entity, transform) ? 1 : 0;
}

void ScriptHost::Impl::OnLog(void* user, int32_t severity, const char* text, int32_t length) {
  if (user == nullptr || text == nullptr || length <= 0) {
    return;
  }
  const std::string_view message(text, static_cast<size_t>(length));
  switch (severity) {
  case 0:
    Log(LogLevel::Trace, "{}", message);
    break;
  case 1:
    Log(LogLevel::Debug, "{}", message);
    break;
  case 3:
    Log(LogLevel::Warn, "{}", message);
    break;
  case 4:
    Log(LogLevel::Error, "{}", message);
    break;
  case 5:
    Log(LogLevel::Fatal, "{}", message);
    break;
  default:
    Log(LogLevel::Info, "{}", message);
    break;
  }
}

ScriptHost::ScriptHost() : m_impl(std::make_unique<Impl>()) {}

ScriptHost::~ScriptHost() {
  Shutdown();
}

bool ScriptHost::IsReady() const noexcept {
  return m_impl->ready;
}

const std::string& ScriptHost::LastError() const noexcept {
  return m_impl->message;
}

void ScriptHost::SetInput(const ScriptInput* input) noexcept {
  m_impl->input = input;
}

Result<void> ScriptHost::Initialize(const std::filesystem::path& executableDirectory) {
  Impl& impl = *m_impl;
  impl.executableDirectory = executableDirectory;
  impl.runtimeDirectory = executableDirectory / "runtime" / "dotnet";
  impl.hostDirectory = executableDirectory / "runtime" / "host";
  impl.gameplayDirectory = executableDirectory / "runtime" / "gameplay";
  impl.loaderAssembly = impl.hostDirectory / "Caligo.Scripting.Loader.dll";
  const std::filesystem::path runtimeConfig =
      impl.hostDirectory / "Caligo.Scripting.Loader.runtimeconfig.json";

  if (!std::filesystem::exists(impl.loaderAssembly) || !std::filesystem::exists(runtimeConfig)) {
    impl.message = std::format("script runtime was not found under {}",
                               PathText(executableDirectory / "runtime"));
    return Error{impl.message, 1};
  }

  const Result<void> loaded = impl.hostFxr.Load(impl.runtimeDirectory, runtimeConfig);
  if (!loaded) {
    impl.message = loaded.GetError().message;
    return Error{impl.message, 2};
  }

  bool resolved = ResolveEntry(impl.hostFxr, impl.loaderAssembly, L"Initialize", impl.initialize);
  resolved = ResolveEntry(impl.hostFxr, impl.loaderAssembly, L"GetScriptTypeCount",
                          impl.scriptTypeCount) &&
             resolved;
  resolved =
      ResolveEntry(impl.hostFxr, impl.loaderAssembly, L"GetScriptTypeName", impl.scriptTypeName) &&
      resolved;
  resolved = ResolveEntry(impl.hostFxr, impl.loaderAssembly, L"CreateScript", impl.createScript) &&
             resolved;
  resolved = ResolveEntry(impl.hostFxr, impl.loaderAssembly, L"Start", impl.start) && resolved;
  resolved = ResolveEntry(impl.hostFxr, impl.loaderAssembly, L"Update", impl.update) && resolved;
  resolved = ResolveEntry(impl.hostFxr, impl.loaderAssembly, L"Stop", impl.stop) && resolved;
  resolved = ResolveEntry(impl.hostFxr, impl.loaderAssembly, L"Reset", impl.reset) && resolved;
  resolved =
      ResolveEntry(impl.hostFxr, impl.loaderAssembly, L"GetLastError", impl.lastError) && resolved;
  if (!resolved) {
    impl.hostFxr.Unload();
    impl.message = "script host entry points could not be resolved";
    return Error{impl.message, 3};
  }
  impl.runtimeLoaded = true;

  const Result<void> managed = impl.InitializeManaged();
  if (!managed) {
    return Error{impl.message, 4};
  }
  return {};
}

Result<void> ScriptHost::Reload() {
  Impl& impl = *m_impl;
  if (!impl.runtimeLoaded) {
    if (impl.executableDirectory.empty()) {
      return Error{"script runtime is not loaded", 10};
    }
    return Initialize(impl.executableDirectory);
  }
  impl.ResetManaged();
  const Result<void> managed = impl.InitializeManaged();
  if (!managed) {
    return Error{impl.message, 11};
  }
  return {};
}

void ScriptHost::Shutdown() noexcept {
  if (m_impl == nullptr) {
    return;
  }
  Impl& impl = *m_impl;
  impl.ResetManaged();
  impl.hostFxr.Unload();
  impl.runtimeLoaded = false;
  impl.initialize = nullptr;
  impl.scriptTypeCount = nullptr;
  impl.scriptTypeName = nullptr;
  impl.createScript = nullptr;
  impl.start = nullptr;
  impl.update = nullptr;
  impl.stop = nullptr;
  impl.reset = nullptr;
  impl.lastError = nullptr;
}

Result<std::vector<std::string>> ScriptHost::ScriptTypes() const {
  const Impl& impl = *m_impl;
  if (!impl.ready || impl.scriptTypeCount == nullptr || impl.scriptTypeName == nullptr) {
    return Error{"script host is not ready", 20};
  }
  const int32_t count = impl.scriptTypeCount();
  if (count < 0) {
    return Error{impl.ReadManagedError(), 21};
  }
  std::vector<std::string> names;
  names.reserve(static_cast<size_t>(count));
  for (int32_t index = 0; index < count; ++index) {
    std::array<char, 256> buffer{};
    const int32_t written =
        impl.scriptTypeName(index, buffer.data(), static_cast<int32_t>(buffer.size()));
    if (written < 0) {
      return Error{impl.ReadManagedError(), 22};
    }
    names.emplace_back(buffer.data(), static_cast<size_t>(written));
  }
  return names;
}

Result<void> ScriptHost::BeginPlay(scene::Scene& scene) {
  Impl& impl = *m_impl;
  if (!impl.runtimeLoaded) {
    return Error{"script runtime is not loaded", 30};
  }
  const ScriptInput* input = impl.input;
  impl.ResetManaged();
  const Result<void> managed = impl.InitializeManaged();
  if (!managed) {
    return Error{impl.message, 31};
  }
  impl.scene = &scene;
  impl.input = input;
  for (const scene::Entity& entity : scene.Entities()) {
    for (const scene::ScriptComponent& script : entity.scripts) {
      const int32_t status = impl.createScript(entity.id, script.type.c_str(),
                                               static_cast<int32_t>(script.type.size()));
      if (status != 0) {
        const std::string detail = impl.ReadManagedError();
        impl.ResetManaged();
        return Error{std::format("script '{}' could not start: {}", script.type, detail), 32};
      }
    }
  }
  if (impl.start() != 0) {
    const std::string detail = impl.ReadManagedError();
    impl.ResetManaged();
    return Error{detail.empty() ? "script world could not start" : detail, 33};
  }
  impl.worldStarted = true;
  return {};
}

Result<void> ScriptHost::Tick(float deltaSeconds) {
  Impl& impl = *m_impl;
  if (!impl.worldStarted || impl.update == nullptr) {
    return Error{"play mode is not running", 40};
  }
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
    return Error{"script delta is not valid", 41};
  }
  if (impl.update(deltaSeconds) != 0) {
    const std::string detail = impl.ReadManagedError();
    return Error{detail.empty() ? "script update failed" : detail, 42};
  }
  return {};
}

void ScriptHost::EndPlay() noexcept {
  Impl& impl = *m_impl;
  if (!impl.worldStarted) {
    impl.scene = nullptr;
    impl.input = nullptr;
    return;
  }
  if (impl.stop != nullptr) {
    impl.stop();
  }
  impl.worldStarted = false;
  impl.scene = nullptr;
  impl.input = nullptr;
}

}

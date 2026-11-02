#pragma once

#include <caligo/core/Result.h>
#include <cstdint>
#include <filesystem>
#include <string>

namespace caligo::script {

class HostFxr final {
public:
  using EntryPoint = void (*)(void);
  using CloseFn = int32_t (*)(void*);

  HostFxr();
  ~HostFxr();

  HostFxr(const HostFxr&) = delete;
  HostFxr& operator=(const HostFxr&) = delete;
  HostFxr(HostFxr&&) = delete;
  HostFxr& operator=(HostFxr&&) = delete;

  [[nodiscard]] Result<void> Load(const std::filesystem::path& runtimeDirectory,
                                  const std::filesystem::path& runtimeConfigPath);
  void Unload() noexcept;
  [[nodiscard]] bool IsLoaded() const noexcept;

  [[nodiscard]] Result<EntryPoint> Resolve(const std::filesystem::path& assemblyPath,
                                           const std::wstring& typeName,
                                           const std::wstring& methodName) const;

private:
  void* m_module = nullptr;
  void* m_context = nullptr;
  void* m_load = nullptr;
  CloseFn m_close = nullptr;
};

}
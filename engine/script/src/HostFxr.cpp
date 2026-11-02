#include "HostFxr.h"

#include <windows.h>

#include <algorithm>
#include <format>
#include <vector>

namespace caligo::script {

namespace {

using InitializeForConfigFn = int32_t (*)(const wchar_t*, const void*, void**);
using GetDelegateFn = int32_t (*)(void*, int32_t, void**);
using LoadAssemblyFn = int32_t (*)(const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*,
                                   void*, void**);

constexpr int32_t kLoadAssemblyAndGetFunctionPointer = 5;
constexpr intptr_t kUnmanagedCallersOnlyValue = -1;

struct InitializeParameters {
  size_t size = 0;
  const wchar_t* hostPath = nullptr;
  const wchar_t* dotnetRoot = nullptr;
};

template <typename Function> Function LoadProcedure(void* module, const char* name) {
#pragma warning(suppress : 4191)
  return reinterpret_cast<Function>(GetProcAddress(static_cast<HMODULE>(module), name));
}

std::filesystem::path FindHostFxr(const std::filesystem::path& runtimeDirectory) {
  const std::filesystem::path root = runtimeDirectory / "host" / "fxr";
  std::error_code error;
  std::vector<std::filesystem::path> candidates;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(root, error)) {
    if (!entry.is_directory()) {
      continue;
    }
    const std::filesystem::path candidate = entry.path() / "hostfxr.dll";
    if (std::filesystem::exists(candidate)) {
      candidates.push_back(candidate);
    }
  }
  if (candidates.empty()) {
    return {};
  }
  std::sort(candidates.begin(), candidates.end());
  return candidates.back();
}

std::string PathText(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

}

HostFxr::HostFxr() = default;

HostFxr::~HostFxr() {
  Unload();
}

bool HostFxr::IsLoaded() const noexcept {
  return m_module != nullptr && m_load != nullptr;
}

Result<void> HostFxr::Load(const std::filesystem::path& runtimeDirectory,
                           const std::filesystem::path& runtimeConfigPath) {
  if (m_module != nullptr) {
    return Error{"script runtime is already loaded", 1};
  }
  const std::filesystem::path hostFxr = FindHostFxr(runtimeDirectory);
  if (hostFxr.empty()) {
    return Error{std::format("hostfxr was not found under {}", PathText(runtimeDirectory)), 2};
  }

  m_module = LoadLibraryW(hostFxr.c_str());
  if (m_module == nullptr) {
    return Error{std::format("hostfxr could not be loaded from {}", PathText(hostFxr)), 3};
  }

  const InitializeForConfigFn initialize =
      LoadProcedure<InitializeForConfigFn>(m_module, "hostfxr_initialize_for_runtime_config");
  const GetDelegateFn getDelegate =
      LoadProcedure<GetDelegateFn>(m_module, "hostfxr_get_runtime_delegate");
  m_close = LoadProcedure<CloseFn>(m_module, "hostfxr_close");
  if (initialize == nullptr || getDelegate == nullptr || m_close == nullptr) {
    Unload();
    return Error{"hostfxr is missing a required entry point", 4};
  }

  InitializeParameters parameters;
  parameters.size = sizeof(InitializeParameters);
  const std::wstring dotnetRoot = runtimeDirectory.wstring();
  parameters.dotnetRoot = dotnetRoot.c_str();
  int32_t initialized = initialize(runtimeConfigPath.c_str(), &parameters, &m_context);
  if (initialized != 0 || m_context == nullptr) {
    Unload();
    return Error{std::format("script runtime initialization failed (0x{:08x})", initialized), 5};
  }

  int32_t delegated = getDelegate(m_context, kLoadAssemblyAndGetFunctionPointer, &m_load);
  if (delegated != 0 || m_load == nullptr) {
    Unload();
    return Error{std::format("script runtime delegate failed (0x{:08x})", delegated), 6};
  }
  return {};
}

void HostFxr::Unload() noexcept {
  if (m_context != nullptr && m_close != nullptr) {
    m_close(m_context);
  }
  m_context = nullptr;
  m_load = nullptr;
  m_close = nullptr;
  if (m_module != nullptr) {
    FreeLibrary(static_cast<HMODULE>(m_module));
    m_module = nullptr;
  }
}

Result<HostFxr::EntryPoint> HostFxr::Resolve(const std::filesystem::path& assemblyPath,
                                             const std::wstring& typeName,
                                             const std::wstring& methodName) const {
  if (m_load == nullptr) {
    return Error{"script runtime is not loaded", 10};
  }
#pragma warning(suppress : 4191)
  const auto load = reinterpret_cast<LoadAssemblyFn>(m_load);
  void* function = nullptr;
  const int32_t resolved =
      load(assemblyPath.c_str(), typeName.c_str(), methodName.c_str(),
           reinterpret_cast<const wchar_t*>(kUnmanagedCallersOnlyValue), nullptr, &function);
  if (resolved != 0 || function == nullptr) {
    return Error{std::format("entry point could not be resolved (0x{:08x})", resolved), 11};
  }
#pragma warning(suppress : 4191)
  return reinterpret_cast<EntryPoint>(function);
}

}
#include <windows.h>

#include <caligo/platform/System.h>
#include <cstdint>
#include <format>
#include <shlobj.h>
#include <string>

namespace caligo::platform {

namespace {

Result<std::filesystem::path> KnownFolder(REFKNOWNFOLDERID folder) {
  PWSTR raw = nullptr;
  const HRESULT result = SHGetKnownFolderPath(folder, KF_FLAG_DEFAULT, nullptr, &raw);
  if (FAILED(result) || raw == nullptr) {
    if (raw != nullptr) {
      CoTaskMemFree(raw);
    }
    return Error{
        std::format("SHGetKnownFolderPath failed: 0x{:08X}", static_cast<uint32_t>(result)), 1};
  }
  const std::filesystem::path path(raw);
  CoTaskMemFree(raw);
  return path;
}

}

void ConfigureProcess() noexcept {
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

float SystemDpiScale() noexcept {
  const UINT dpi = GetDpiForSystem();
  return dpi > 0 ? static_cast<float>(dpi) / 96.0f : 1.0f;
}

Result<std::filesystem::path> WindowsDirectory() {
  std::wstring buffer(32768, L'\0');
  const UINT length = GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
  if (length == 0) {
    return Error{std::format("GetWindowsDirectoryW failed with error {}", GetLastError()), 1};
  }
  if (static_cast<size_t>(length) >= buffer.size()) {
    return Error{"Windows directory path is too long", 2};
  }
  buffer.resize(length);
  return std::filesystem::path(buffer);
}

Result<std::filesystem::path> DocumentsDirectory() {
  return KnownFolder(FOLDERID_Documents);
}

Result<std::filesystem::path> UserDataDirectory() {
  const Result<std::filesystem::path> roaming = KnownFolder(FOLDERID_RoamingAppData);
  if (!roaming) {
    return roaming.GetError();
  }
  return roaming.Value() / "Caligo";
}

}

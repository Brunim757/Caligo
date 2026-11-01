#include <caligo/platform/System.h>
#include <format>
#include <string>
#include <windows.h>

namespace caligo::platform {

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

}

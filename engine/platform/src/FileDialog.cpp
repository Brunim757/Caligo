#include <windows.h>

#include <commdlg.h>

#include <algorithm>
#include <caligo/platform/FileDialog.h>
#include <cstddef>
#include <format>
#include <limits>
#include <shlobj.h>
#include <string>
#include <vector>

namespace caligo::platform {

namespace {

std::wstring ToWide(std::string_view text) {
  if (text.empty() || text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return {};
  }
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) {
    return {};
  }
  std::wstring result(static_cast<size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                      result.data(), length);
  return result;
}

std::wstring PathToWide(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return ToWide(std::string_view(reinterpret_cast<const char*>(text.data()), text.size()));
}

Result<std::optional<std::filesystem::path>>
RunDialog(std::string_view title, std::string_view filter, std::string_view defaultExtension,
          std::string_view suggestedFileName, const std::filesystem::path& initialDirectory,
          void* ownerWindow, bool save) {
  const std::wstring titleWide = ToWide(title);
  const std::wstring filterWide = ToWide(filter);
  const std::wstring extensionWide = ToWide(defaultExtension);
  const std::wstring directoryWide =
      initialDirectory.empty() ? std::wstring{} : PathToWide(initialDirectory);

  std::vector<wchar_t> buffer(32768, L'\0');
  if (save && !suggestedFileName.empty()) {
    const std::wstring suggestedWide = ToWide(suggestedFileName);
    const size_t count = std::min(suggestedWide.size(), buffer.size() - 1);
    std::copy_n(suggestedWide.begin(), static_cast<std::ptrdiff_t>(count), buffer.begin());
  }
  OPENFILENAMEW dialog{};
  dialog.lStructSize = static_cast<DWORD>(sizeof(dialog));
  dialog.hwndOwner = static_cast<HWND>(ownerWindow);
  dialog.lpstrFilter = filterWide.empty() ? nullptr : filterWide.c_str();
  dialog.lpstrFile = buffer.data();
  dialog.nMaxFile = static_cast<DWORD>(buffer.size());
  dialog.lpstrTitle = titleWide.empty() ? nullptr : titleWide.c_str();
  dialog.lpstrInitialDir = directoryWide.empty() ? nullptr : directoryWide.c_str();
  dialog.lpstrDefExt = extensionWide.empty() ? nullptr : extensionWide.c_str();
  dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_EXPLORER;
  if (save) {
    dialog.Flags |= OFN_OVERWRITEPROMPT;
  } else {
    dialog.Flags |= OFN_FILEMUSTEXIST;
  }

  const BOOL accepted = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
  if (accepted != FALSE) {
    return std::optional<std::filesystem::path>{std::filesystem::path(buffer.data())};
  }

  const DWORD error = CommDlgExtendedError();
  if (error == 0) {
    return std::optional<std::filesystem::path>{};
  }
  return Error{std::format("file dialog failed with error {}", static_cast<uint32_t>(error)), 1};
}

}

Result<std::optional<std::filesystem::path>>
ShowOpenFileDialog(std::string_view title, std::string_view filter,
                   const std::filesystem::path& initialDirectory, void* ownerWindow) {
  return RunDialog(title, filter, {}, {}, initialDirectory, ownerWindow, false);
}

Result<std::optional<std::filesystem::path>>
ShowSaveFileDialog(std::string_view title, std::string_view filter,
                   std::string_view defaultExtension, std::string_view suggestedFileName,
                   const std::filesystem::path& initialDirectory, void* ownerWindow) {
  return RunDialog(title, filter, defaultExtension, suggestedFileName, initialDirectory,
                   ownerWindow, true);
}

Result<std::optional<std::filesystem::path>>
ShowOpenFolderDialog(std::string_view title, const std::filesystem::path& initialDirectory,
                     void* ownerWindow) {
  (void)initialDirectory;
  const std::wstring titleWide = ToWide(title);
  BROWSEINFOW info{};
  info.hwndOwner = static_cast<HWND>(ownerWindow);
  info.lpszTitle = titleWide.empty() ? nullptr : titleWide.c_str();
  info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  LPITEMIDLIST pidl = SHBrowseForFolderW(&info);
  if (pidl == nullptr) {
    return std::optional<std::filesystem::path>{};
  }
  wchar_t path[MAX_PATH];
  if (SHGetPathFromIDListW(pidl, path)) {
    CoTaskMemFree(pidl);
    return std::optional<std::filesystem::path>{std::filesystem::path(path)};
  }
  CoTaskMemFree(pidl);
  return std::optional<std::filesystem::path>{};
}

}

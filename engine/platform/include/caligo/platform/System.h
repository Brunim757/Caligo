#pragma once

#include <caligo/core/Result.h>
#include <filesystem>

namespace caligo::platform {

void ConfigureProcess() noexcept;
[[nodiscard]] float SystemDpiScale() noexcept;
[[nodiscard]] Result<std::filesystem::path> WindowsDirectory();
[[nodiscard]] Result<std::filesystem::path> DocumentsDirectory();
[[nodiscard]] Result<std::filesystem::path> UserDataDirectory();

}

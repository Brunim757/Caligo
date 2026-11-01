#pragma once

#include <caligo/core/Result.h>
#include <filesystem>
#include <optional>
#include <string_view>

namespace caligo::platform {

[[nodiscard]] Result<std::optional<std::filesystem::path>>
ShowOpenFileDialog(std::string_view title, std::string_view filter,
                   const std::filesystem::path& initialDirectory = {}, void* ownerWindow = nullptr);

[[nodiscard]] Result<std::optional<std::filesystem::path>>
ShowSaveFileDialog(std::string_view title, std::string_view filter,
                   std::string_view defaultExtension, std::string_view suggestedFileName = {},
                   const std::filesystem::path& initialDirectory = {}, void* ownerWindow = nullptr);

}

#pragma once

#include <cstdint>
#include <format>
#include <string>

namespace caligo::render {

inline std::string Failure(const char* operation, long result) {
  return std::format("{} failed with HRESULT {:#010x}", operation, static_cast<uint32_t>(result));
}

}

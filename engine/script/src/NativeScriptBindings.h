#pragma once

#include <cstdint>

namespace caligo::script {

struct NativeScriptBindings {
  void* user = nullptr;
  int32_t (*isKeyDown)(void* user, int32_t key) = nullptr;
  int32_t (*isKeyPressed)(void* user, int32_t key) = nullptr;
  int32_t (*getTransform)(void* user, uint64_t entity, float* position, float* rotation,
                          float* scale) = nullptr;
  int32_t (*setTransform)(void* user, uint64_t entity, const float* position, const float* rotation,
                          const float* scale) = nullptr;
  void (*log)(void* user, int32_t severity, const char* message, int32_t length) = nullptr;
};

}

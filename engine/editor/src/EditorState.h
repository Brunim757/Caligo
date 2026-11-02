#pragma once

#include <array>
#include <caligo/core/Log.h>
#include <caligo/core/Math.h>
#include <caligo/core/Time.h>
#include <caligo/editor/ConsoleBuffer.h>
#include <caligo/platform/Window.h>
#include <caligo/render/GraphicsDevice.h>
#include <caligo/render/SceneRenderer.h>
#include <caligo/scene/Scene.h>
#include <caligo/script/ScriptHost.h>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

namespace caligo::editor {

enum class GizmoOperation { Translate, Rotate, Scale };
enum class GizmoSpace { Local, World };

struct Camera {
  math::Vec3 target{0.0f, 0.0f, 0.0f};
  float yaw = 45.0f;
  float pitch = 28.0f;
  float distance = 14.0f;

  [[nodiscard]] math::Vec3 Position() const noexcept;
  [[nodiscard]] math::Mat4 ViewMatrix() const noexcept;
  [[nodiscard]] math::Mat4 ProjectionMatrix(float aspect) const noexcept;
  void Focus(const math::Vec3& point) noexcept;
};

enum class PendingAction { None, NewScene, OpenScene, Quit };

struct PlaySession {
  bool active = false;
  bool paused = false;
  bool stepRequested = false;
  uint64_t ticks = 0;
  scene::Scene snapshot;
  FrameTimer timer;
  Clock clock;
};

struct EditorState {
  platform::Window* window = nullptr;
  render::GraphicsDevice graphics;
  render::SceneRenderer sceneRenderer;
  bool viewportErrorLogged = false;
  scene::Scene scene;
  scene::EntityId selected = scene::kNoEntity;
  std::filesystem::path scenePath;
  uint64_t savedRevision = 0;

  bool initialized = false;
  bool imguiContext = false;
  bool win32Backend = false;
  bool dx11Backend = false;
  bool layoutBuilt = false;
  bool allowClose = false;

  float dpiScale = 1.0f;
  ImFont* consoleFont = nullptr;
  Camera camera;
  GizmoOperation operation = GizmoOperation::Translate;
  GizmoSpace space = GizmoSpace::Local;
  bool orbiting = false;
  bool panning = false;

  math::Vec3 eulerCache{0.0f, 0.0f, 0.0f};
  scene::EntityId eulerOwner = scene::kNoEntity;
  uint64_t eulerRevision = 0;
  std::array<char, 128> nameBuffer{};
  scene::EntityId nameOwner = scene::kNoEntity;

  PendingAction pendingAction = PendingAction::None;
  bool discardPopupRequested = false;
  bool errorPopupRequested = false;
  bool showAboutPopup = false;
  std::string errorTitle;
  std::string errorMessage;

  std::shared_ptr<ConsoleBuffer> console;
  ConsoleSink* consoleSink = nullptr;
  bool autoScrollConsole = true;
  LogLevel consoleMinimumLevel = LogLevel::Trace;
  std::string windowTitle;

  script::ScriptHost scriptHost;
  std::vector<std::string> scriptTypes;
  script::ScriptInput scriptInput;
  PlaySession play;
};

[[nodiscard]] inline bool IsDirty(const EditorState& state) noexcept {
  if (state.play.active) {
    return state.play.snapshot.Revision() != state.savedRevision;
  }
  return state.scene.Revision() != state.savedRevision;
}

[[nodiscard]] inline float Pixels(const EditorState& state, float value) noexcept {
  return value * (state.dpiScale > 0.0f ? state.dpiScale : 1.0f);
}

}

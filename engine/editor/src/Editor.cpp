#include "EditorPanels.h"
#include "EditorState.h"

#include <ImGuizmo.h>
#include <algorithm>
#include <caligo/core/Log.h>
#include <caligo/editor/Editor.h>
#include <caligo/platform/File.h>
#include <caligo/platform/System.h>
#include <filesystem>
#include <format>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <memory>
#include <string>
#include <utility>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message,
                                                             WPARAM wParam, LPARAM lParam);

namespace caligo::editor {

namespace {

bool HandleWindowMessage(void* user, uint32_t message, uintptr_t wParam, intptr_t lParam) {
  auto* state = static_cast<EditorState*>(user);
  if (state == nullptr || state->window == nullptr) {
    return false;
  }
  const HWND window = static_cast<HWND>(state->window->NativeHandle());
  if (window == nullptr) {
    return false;
  }
  return ImGui_ImplWin32_WndProcHandler(window, message, static_cast<WPARAM>(wParam),
                                        static_cast<LPARAM>(lParam)) != 0;
}

bool HandleWindowClose(void* user) {
  auto* state = static_cast<EditorState*>(user);
  if (state == nullptr) {
    return true;
  }
  if (state->allowClose) {
    return true;
  }
  if (IsDirty(*state)) {
    RequestAction(*state, PendingAction::Quit);
    return false;
  }
  state->allowClose = true;
  return true;
}

std::filesystem::path DefaultScenePath() {
  const Result<std::filesystem::path> executable = platform::ExecutableDirectory();
  if (!executable) {
    return {};
  }
  const std::filesystem::path path = executable.Value() / "content" / "scenes" / "exemplo.cscene";
  return platform::FileExists(path) ? path : std::filesystem::path{};
}

}

Editor::Editor() = default;

Editor::~Editor() {
  Shutdown();
}

Result<void> Editor::Initialize(platform::Window& window, const std::filesystem::path& scenePath) {
  if (m_state != nullptr) {
    return Error{"editor is already initialized", 1};
  }
  if (window.NativeHandle() == nullptr) {
    return Error{"editor needs an open window", 2};
  }

  m_state = std::make_unique<EditorState>();
  m_state->window = &window;
  m_state->autoScrollConsole = true;

  const uint32_t width = std::max(window.Width(), 1u);
  const uint32_t height = std::max(window.Height(), 1u);
  const Result<void> graphics = m_state->graphics.Create(window.NativeHandle(), width, height);
  if (!graphics) {
    const std::string message = graphics.GetError().message;
    Shutdown();
    return Error{std::format("graphics device failed: {}", message), 3};
  }

  if (!InitializeImGui(*m_state)) {
    Shutdown();
    return Error{"editor interface failed to initialize", 4};
  }

  m_state->console = std::make_shared<ConsoleBuffer>();
  auto sink = std::make_unique<ConsoleSink>(m_state->console);
  m_state->consoleSink = sink.get();
  GetLogger().AddSink(std::move(sink));

  window.SetMessageHandler(&HandleWindowMessage, m_state.get());
  window.SetCloseHandler(&HandleWindowClose, m_state.get());

  const std::filesystem::path path = scenePath.empty() ? DefaultScenePath() : scenePath;
  if (path.empty() || !LoadSceneFile(*m_state, path)) {
    NewScene(*m_state);
  }

  UpdateWindowTitle(*m_state);
  m_state->initialized = true;
  CALIGO_LOG_INFO("editor pronto: {} x {}", width, height);
  return {};
}

bool Editor::Frame() {
  if (m_state == nullptr || !m_state->initialized) {
    return false;
  }

  m_state->window->PollEvents();
  if (m_state->window->ShouldClose()) {
    return false;
  }

  const uint32_t width = m_state->window->Width();
  const uint32_t height = m_state->window->Height();
  if (width == 0 || height == 0) {
    return true;
  }

  const float dpiScale = m_state->window->DpiScale();
  if (dpiScale > 0.0f && dpiScale != m_state->dpiScale) {
    m_state->dpiScale = dpiScale;
    ApplyTheme(dpiScale);
  }

  const Result<void> resized = m_state->graphics.Resize(width, height);
  if (!resized) {
    CALIGO_LOG_ERROR("não foi possível redimensionar a viewport: {}", resized.GetError().message);
    return false;
  }

  ImGui_ImplWin32_NewFrame();
  ImGui_ImplDX11_NewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();

  DrawMainMenu(*m_state);
  DrawDockSpace(*m_state);
  DrawHierarchy(*m_state);
  DrawInspector(*m_state);
  DrawViewport(*m_state);
  DrawConsole(*m_state);
  DrawModals(*m_state);

  ImGui::Render();
  m_state->graphics.BeginFrame(0.075f, 0.086f, 0.105f, 1.0f);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

  const Result<void> presented = m_state->graphics.EndFrame();
  if (!presented) {
    CALIGO_LOG_ERROR("não foi possível apresentar o quadro: {}", presented.GetError().message);
    return false;
  }

  UpdateWindowTitle(*m_state);
  return true;
}

void Editor::Shutdown() noexcept {
  if (m_state == nullptr) {
    return;
  }

  if (m_state->consoleSink != nullptr) {
    GetLogger().RemoveSink(m_state->consoleSink);
    m_state->consoleSink = nullptr;
  }
  if (m_state->window != nullptr) {
    m_state->window->SetMessageHandler(nullptr, nullptr);
    m_state->window->SetCloseHandler(nullptr, nullptr);
  }

  ShutdownImGui(*m_state);
  m_state->graphics.Destroy();
  m_state->initialized = false;
  m_state->window = nullptr;
  m_state.reset();
}

}

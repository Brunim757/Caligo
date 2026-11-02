#include <caligo/editor/ViewportCanvas.h>
#include <doctest/doctest.h>
#include <imgui.h>

namespace {

constexpr float kWindowWidth = 1280.0f;
constexpr float kWindowHeight = 720.0f;
constexpr float kToolbarHeight = 40.0f;

class ImGuiFrame {
public:
  ImGuiFrame() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(kWindowWidth, kWindowHeight);
    io.DeltaTime = 1.0f / 60.0f;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.Fonts->AddFontDefault();
  }

  ~ImGuiFrame() {
    ImGui::DestroyContext();
  }

  ImGuiFrame(const ImGuiFrame&) = delete;
  ImGuiFrame& operator=(const ImGuiFrame&) = delete;

  void BeginFrame(float mouseX, float mouseY, bool mouseDown) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(mouseX, mouseY);
    io.AddMouseButtonEvent(0, mouseDown);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(kWindowWidth, kWindowHeight));
    ImGui::Begin("viewport-teste", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoScrollbar);
    ImGui::Dummy(ImVec2(0.0f, kToolbarHeight));
  }

  [[nodiscard]] caligo::editor::ViewportCanvas Canvas() const {
    return caligo::editor::ResolveViewportCanvas(ImGui::GetCursorScreenPos(),
                                                 ImGui::GetContentRegionAvail());
  }

  void EndFrame() {
    ImGui::End();
    ImGui::EndFrame();
  }
};

}

TEST_CASE("o canvas do viewport não bloqueia a ativação do gizmo") {
  ImGuiFrame frame;
  frame.BeginFrame(400.0f, 300.0f, false);
  frame.EndFrame();

  frame.BeginFrame(400.0f, 300.0f, false);
  const caligo::editor::ViewportCanvas canvas = frame.Canvas();
  CHECK(canvas.hovered);
  CHECK_FALSE(ImGui::IsAnyItemHovered());
  CHECK_FALSE(ImGui::IsAnyItemActive());
  frame.EndFrame();
}

TEST_CASE("o canvas do viewport não captura o clique destinado ao gizmo") {
  ImGuiFrame frame;
  frame.BeginFrame(400.0f, 300.0f, false);
  frame.EndFrame();

  frame.BeginFrame(400.0f, 300.0f, true);
  const caligo::editor::ViewportCanvas canvas = frame.Canvas();
  CHECK(canvas.hovered);
  CHECK_FALSE(ImGui::IsAnyItemActive());
  CHECK_FALSE(ImGui::IsAnyItemHovered());
  frame.EndFrame();
}

TEST_CASE("o canvas do viewport responde apenas dentro da sua área") {
  SUBCASE("barra de ferramentas") {
    ImGuiFrame frame;
    frame.BeginFrame(400.0f, kToolbarHeight * 0.5f, false);
    frame.EndFrame();

    frame.BeginFrame(400.0f, kToolbarHeight * 0.5f, false);
    CHECK_FALSE(frame.Canvas().hovered);
    frame.EndFrame();
  }

  SUBCASE("fora do viewport") {
    ImGuiFrame frame;
    frame.BeginFrame(-4.0f, 300.0f, false);
    frame.EndFrame();

    frame.BeginFrame(-4.0f, 300.0f, false);
    CHECK_FALSE(frame.Canvas().hovered);
    frame.EndFrame();
  }
}
#include "EditorPanels.h"

#include <ImGuizmo.h>
#include <caligo/platform/File.h>
#include <caligo/platform/System.h>
#include <d3d11.h>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace caligo::editor {

namespace {

void SetColor(ImGuiCol slot, int red, int green, int blue, int alpha = 255) {
  ImGui::GetStyle().Colors[slot] =
      ImGui::ColorConvertU32ToFloat4(IM_COL32(red, green, blue, alpha));
}

}

void ApplyTheme(float dpiScale) {
  ImGuiStyle& style = ImGui::GetStyle();
  style = ImGuiStyle{};
  ImGui::StyleColorsDark();

  style.WindowPadding = ImVec2(10.0f, 10.0f);
  style.FramePadding = ImVec2(9.0f, 5.0f);
  style.ItemSpacing = ImVec2(8.0f, 6.0f);
  style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
  style.CellPadding = ImVec2(6.0f, 4.0f);
  style.IndentSpacing = 18.0f;
  style.ScrollbarSize = 15.0f;
  style.GrabMinSize = 12.0f;
  style.WindowBorderSize = 1.0f;
  style.ChildBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.TabBorderSize = 0.0f;
  style.WindowRounding = 6.0f;
  style.ChildRounding = 4.0f;
  style.FrameRounding = 4.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 4.0f;
  style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
  style.WindowMenuButtonPosition = ImGuiDir_None;
  style.SeparatorTextBorderSize = 1.0f;
  style.SeparatorTextPadding = ImVec2(16.0f, 4.0f);
  style.WindowMinSize = ImVec2(220.0f, 140.0f);

  SetColor(ImGuiCol_Text, 232, 235, 240);
  SetColor(ImGuiCol_TextDisabled, 130, 136, 148);
  SetColor(ImGuiCol_WindowBg, 24, 26, 31);
  SetColor(ImGuiCol_ChildBg, 21, 23, 27);
  SetColor(ImGuiCol_PopupBg, 29, 32, 38);
  SetColor(ImGuiCol_Border, 45, 49, 57);
  SetColor(ImGuiCol_BorderShadow, 0, 0, 0, 0);
  SetColor(ImGuiCol_FrameBg, 38, 42, 49);
  SetColor(ImGuiCol_FrameBgHovered, 49, 54, 63);
  SetColor(ImGuiCol_FrameBgActive, 61, 68, 79);
  SetColor(ImGuiCol_TitleBg, 20, 22, 26);
  SetColor(ImGuiCol_TitleBgActive, 32, 36, 43);
  SetColor(ImGuiCol_TitleBgCollapsed, 20, 22, 26);
  SetColor(ImGuiCol_MenuBarBg, 27, 30, 36);
  SetColor(ImGuiCol_ScrollbarBg, 20, 22, 26);
  SetColor(ImGuiCol_ScrollbarGrab, 55, 60, 70);
  SetColor(ImGuiCol_ScrollbarGrabHovered, 72, 78, 90);
  SetColor(ImGuiCol_ScrollbarGrabActive, 90, 98, 112);
  SetColor(ImGuiCol_CheckMark, 0, 162, 255);
  SetColor(ImGuiCol_SliderGrab, 0, 162, 255);
  SetColor(ImGuiCol_SliderGrabActive, 70, 190, 255);
  SetColor(ImGuiCol_Button, 48, 53, 62);
  SetColor(ImGuiCol_ButtonHovered, 62, 69, 81);
  SetColor(ImGuiCol_ButtonActive, 78, 87, 102);
  SetColor(ImGuiCol_Header, 26, 104, 164);
  SetColor(ImGuiCol_HeaderHovered, 33, 126, 198);
  SetColor(ImGuiCol_HeaderActive, 20, 86, 138);
  SetColor(ImGuiCol_Separator, 45, 49, 57);
  SetColor(ImGuiCol_SeparatorHovered, 0, 162, 255);
  SetColor(ImGuiCol_SeparatorActive, 0, 190, 255);
  SetColor(ImGuiCol_ResizeGrip, 70, 76, 88, 90);
  SetColor(ImGuiCol_ResizeGripHovered, 0, 162, 255, 150);
  SetColor(ImGuiCol_ResizeGripActive, 0, 190, 255, 190);
  SetColor(ImGuiCol_TabHovered, 55, 61, 72);
  SetColor(ImGuiCol_Tab, 30, 33, 39);
  SetColor(ImGuiCol_TabSelected, 43, 48, 57);
  SetColor(ImGuiCol_TabSelectedOverline, 0, 162, 255);
  SetColor(ImGuiCol_TabDimmed, 25, 27, 32);
  SetColor(ImGuiCol_TabDimmedSelected, 35, 39, 46);
  SetColor(ImGuiCol_TabDimmedSelectedOverline, 70, 76, 88);
  SetColor(ImGuiCol_DockingPreview, 0, 162, 255, 140);
  SetColor(ImGuiCol_DockingEmptyBg, 19, 21, 25);
  SetColor(ImGuiCol_PlotLines, 0, 162, 255);
  SetColor(ImGuiCol_TableHeaderBg, 33, 36, 43);
  SetColor(ImGuiCol_TableBorderStrong, 52, 57, 67);
  SetColor(ImGuiCol_TableBorderLight, 40, 44, 52);
  SetColor(ImGuiCol_TableRowBg, 255, 255, 255, 0);
  SetColor(ImGuiCol_TableRowBgAlt, 255, 255, 255, 5);
  SetColor(ImGuiCol_TextSelectedBg, 0, 120, 215, 110);
  SetColor(ImGuiCol_DragDropTarget, 0, 162, 255, 220);
  SetColor(ImGuiCol_NavCursor, 0, 162, 255, 180);
  SetColor(ImGuiCol_ModalWindowDimBg, 0, 0, 0, 150);

  const float scale = dpiScale > 0.0f ? dpiScale : 1.0f;
  style.ScaleAllSizes(scale);
  style.FontScaleDpi = scale;
}

bool InitializeImGui(EditorState& state) {
  if (ImGui::GetCurrentContext() != nullptr) {
    return false;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  state.imguiContext = true;

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigDpiScaleFonts = false;
  io.ConfigDpiScaleViewports = false;
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;

  const float dpiScale = state.window->DpiScale() > 0.0f ? state.window->DpiScale() : 1.0f;
  state.dpiScale = dpiScale;
  ApplyTheme(dpiScale);

  const Result<std::filesystem::path> windowsDirectory = platform::WindowsDirectory();
  if (windowsDirectory) {
    const std::filesystem::path fontPath = windowsDirectory.Value() / "Fonts" / "segoeui.ttf";
    if (platform::FileExists(fontPath)) {
      io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 17.0f);
    }
  }
  if (io.Fonts->Fonts.Size == 0) {
    ImFontConfig config;
    config.SizePixels = 17.0f;
    io.Fonts->AddFontDefault(&config);
  }
  state.consoleFont = io.Fonts->Fonts[0];

  if (windowsDirectory) {
    const std::filesystem::path monoPath = windowsDirectory.Value() / "Fonts" / "consola.ttf";
    if (platform::FileExists(monoPath)) {
      ImFont* consoleFont = io.Fonts->AddFontFromFileTTF(monoPath.string().c_str(), 15.5f);
      if (consoleFont != nullptr) {
        state.consoleFont = consoleFont;
      }
    }
  }

  if (!ImGui_ImplWin32_Init(state.window->NativeHandle())) {
    ShutdownImGui(state);
    return false;
  }
  state.win32Backend = true;

  if (!ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(state.graphics.DeviceHandle()),
                           static_cast<ID3D11DeviceContext*>(state.graphics.ContextHandle()))) {
    ShutdownImGui(state);
    return false;
  }
  state.dx11Backend = true;

  ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
  return true;
}

void ShutdownImGui(EditorState& state) noexcept {
  if (state.dx11Backend) {
    ImGui_ImplDX11_Shutdown();
    state.dx11Backend = false;
  }
  if (state.win32Backend) {
    ImGui_ImplWin32_Shutdown();
    state.win32Backend = false;
  }
  if (state.imguiContext) {
    ImGui::DestroyContext();
    state.imguiContext = false;
  }
}

}

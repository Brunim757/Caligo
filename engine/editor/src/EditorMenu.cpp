#include "EditorPanels.h"
#include <ImGuizmo.h>
#include <caligo/core/Log.h>
#include <cstddef>
#include <imgui.h>
#include <imgui_internal.h>

namespace caligo::editor {

namespace {

constexpr char kDiscardPopup[] = "Descartar alterações?";
constexpr char kErrorPopup[] = "Erro";
constexpr char kAboutPopup[] = "Sobre a Caligo";

bool AnyPopupOpen() {
  return ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

void HandleShortcuts(EditorState& state) {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput || AnyPopupOpen() || ImGuizmo::IsUsing()) {
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
    if (state.play.active) {
      StopPlay(state);
    } else {
      StartPlay(state);
    }
    return;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) {
    TogglePlayPause(state);
    return;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) {
    RequestPlayStep(state);
    return;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_F8, false)) {
    ReloadScripts(state);
    return;
  }
  if (state.play.active) {
    return;
  }

  if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    static_cast<void>(SaveScene(state, true));
    return;
  }
  if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    static_cast<void>(SaveScene(state, false));
    return;
  }
  if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
    RequestAction(state, PendingAction::NewScene);
    return;
  }
  if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
    RequestAction(state, PendingAction::OpenScene);
    return;
  }
  if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
    RequestAction(state, PendingAction::Quit);
    return;
  }
  if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
    CreateEntity(state);
    return;
  }
  if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
      state.selected != scene::kNoEntity) {
    DeleteSelectedEntity(state);
  }
}

void BuildDockLayout(ImGuiID dockspaceId, const ImVec2& size) {
  ImGui::DockBuilderRemoveNode(dockspaceId);
  ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceId, size);

  ImGuiID center = dockspaceId;
  const ImGuiID bottom =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.27f, nullptr, &center);
  const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.21f, nullptr, &center);
  const ImGuiID right =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);

  ImGui::DockBuilderDockWindow("Cena", center);
  ImGui::DockBuilderDockWindow("Hierarquia", left);
  ImGui::DockBuilderDockWindow("Inspetor", right);
  ImGui::DockBuilderDockWindow("Console", bottom);
  ImGui::DockBuilderFinish(dockspaceId);
}

void DrawFileMenu(EditorState& state) {
  if (!ImGui::BeginMenu("Arquivo")) {
    return;
  }
  ImGui::BeginDisabled(state.play.active);
  if (ImGui::MenuItem("Nova cena", "Ctrl+N")) {
    RequestAction(state, PendingAction::NewScene);
  }
  if (ImGui::MenuItem("Abrir cena...", "Ctrl+O")) {
    RequestAction(state, PendingAction::OpenScene);
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Salvar", "Ctrl+S")) {
    static_cast<void>(SaveScene(state, false));
  }
  if (ImGui::MenuItem("Salvar como...", "Ctrl+Shift+S")) {
    static_cast<void>(SaveScene(state, true));
  }
  ImGui::EndDisabled();
  ImGui::Separator();
  if (ImGui::MenuItem("Sair", "Ctrl+Q")) {
    RequestAction(state, PendingAction::Quit);
  }
  ImGui::EndMenu();
}

void DrawEditMenu(EditorState& state) {
  if (!ImGui::BeginMenu("Editar")) {
    return;
  }
  ImGui::BeginDisabled(state.play.active);
  if (ImGui::MenuItem("Criar entidade", "Ctrl+Shift+N")) {
    CreateEntity(state);
  }
  if (ImGui::MenuItem("Excluir entidade", "Delete", false, state.selected != scene::kNoEntity)) {
    DeleteSelectedEntity(state);
  }
  ImGui::EndDisabled();
  ImGui::EndMenu();
}

void DrawRunMenu(EditorState& state) {
  if (!ImGui::BeginMenu("Executar")) {
    return;
  }
  if (ImGui::MenuItem(state.play.active ? "Parar" : "Iniciar", "F5")) {
    if (state.play.active) {
      StopPlay(state);
    } else {
      StartPlay(state);
    }
  }
  ImGui::BeginDisabled(!state.play.active);
  if (ImGui::MenuItem(state.play.paused ? "Continuar" : "Pausar", "F6")) {
    TogglePlayPause(state);
  }
  if (ImGui::MenuItem("Avançar um quadro", "F7", false, state.play.paused)) {
    RequestPlayStep(state);
  }
  ImGui::EndDisabled();
  ImGui::Separator();
  if (ImGui::MenuItem("Recarregar scripts", "F8")) {
    ReloadScripts(state);
  }
  ImGui::EndMenu();
}

void DrawViewMenu(EditorState& state) {
  if (!ImGui::BeginMenu("Exibir")) {
    return;
  }
  if (ImGui::MenuItem("Restaurar layout")) {
    ResetLayout(state);
  }
  ImGui::EndMenu();
}

void DrawHelpMenu(EditorState& state) {
  if (!ImGui::BeginMenu("Ajuda")) {
    return;
  }
  if (ImGui::MenuItem("Sobre a Caligo")) {
    state.showAboutPopup = true;
  }
  ImGui::EndMenu();
}

void DrawDiscardPopup(EditorState& state) {
  if (state.discardPopupRequested) {
    ImGui::OpenPopup(kDiscardPopup);
    state.discardPopupRequested = false;
  }

  if (!ImGui::BeginPopupModal(kDiscardPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::TextUnformatted("A cena tem alterações não salvas.");
  ImGui::TextDisabled("Salve antes de continuar ou descarte o que mudou.");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::Button("Salvar e continuar", ImVec2(Pixels(state, 178.0f), 0.0f))) {
    if (SaveScene(state, false)) {
      ExecuteAction(state, state.pendingAction);
      ImGui::CloseCurrentPopup();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Descartar", ImVec2(Pixels(state, 118.0f), 0.0f))) {
    ExecuteAction(state, state.pendingAction);
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancelar", ImVec2(Pixels(state, 108.0f), 0.0f))) {
    state.pendingAction = PendingAction::None;
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

void DrawErrorPopup(EditorState& state) {
  if (state.errorPopupRequested) {
    ImGui::OpenPopup(kErrorPopup);
    state.errorPopupRequested = false;
  }

  if (!ImGui::BeginPopupModal(kErrorPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
  if (!state.errorTitle.empty()) {
    ImGui::TextUnformatted(state.errorTitle.c_str());
    ImGui::Spacing();
  }
  ImGui::TextUnformatted(state.errorMessage.c_str());
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
  if (ImGui::Button("Entendi", ImVec2(Pixels(state, 120.0f), 0.0f))) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

void DrawAboutPopup(EditorState& state) {
  if (state.showAboutPopup) {
    ImGui::OpenPopup(kAboutPopup);
    state.showAboutPopup = false;
  }

  if (!ImGui::BeginPopupModal(kAboutPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::TextUnformatted("Caligo");
  ImGui::TextDisabled("Kizuri Studio");
  ImGui::Separator();
  ImGui::Text("Versão %s (%s)", CALIGO_VERSION, CALIGO_CONFIGURATION);
  ImGui::TextUnformatted("Windows 10/11 x64 | Direct3D 11");
  ImGui::TextUnformatted("Editor, cena e ferramentas de autoria.");
  ImGui::Spacing();
  if (ImGui::Button("Fechar", ImVec2(Pixels(state, 120.0f), 0.0f))) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

}

void DrawMainMenu(EditorState& state) {
  HandleShortcuts(state);

  if (!ImGui::BeginMainMenuBar()) {
    return;
  }
  DrawFileMenu(state);
  DrawEditMenu(state);
  DrawRunMenu(state);
  DrawViewMenu(state);
  DrawHelpMenu(state);

  const char* release = "R1";
  const float offset =
      ImGui::GetWindowWidth() - ImGui::CalcTextSize(release).x - Pixels(state, 16.0f);
  ImGui::SameLine(offset);
  ImGui::TextDisabled("%s", release);
  ImGui::EndMainMenuBar();
}

void DrawDockSpace(EditorState& state) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  constexpr ImGuiWindowFlags kHostFlags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus;
  ImGui::Begin("##caligo-dock-host", nullptr, kHostFlags);

  const ImGuiID dockspaceId = ImGui::GetID("CaligoDockSpace");
  if (!state.layoutBuilt) {
    BuildDockLayout(dockspaceId, viewport->WorkSize);
    state.layoutBuilt = true;
  }
  ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

  ImGui::End();
  ImGui::PopStyleVar(3);
}

void ResetLayout(EditorState& state) {
  state.layoutBuilt = false;
}

void DrawModals(EditorState& state) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  constexpr ImGuiWindowFlags kHostFlags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav;
  if (ImGui::Begin("##caligo-popup-host", nullptr, kHostFlags)) {
    DrawDiscardPopup(state);
    DrawErrorPopup(state);
    DrawAboutPopup(state);
  }
  ImGui::End();
}

}

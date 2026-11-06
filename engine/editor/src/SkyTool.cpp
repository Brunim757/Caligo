#include "SkyTool.h"

#include <caligo/render/Sky.h>
#include <imgui.h>
#include <span>

namespace caligo::editor {

namespace {

void SetSkyHours(EditorState& state, float hours) {
  scene::WorldEnvironment environment = state.scene.Environment();
  environment.skyHours = hours;
  static_cast<void>(state.scene.SetEnvironment(environment));
}

}

void DrawSkyToolbar(EditorState& state) {
  ImGui::BeginDisabled(state.play.active);
  ImGui::TextDisabled("Hora");
  ImGui::SameLine();
  float hours = state.scene.Environment().skyHours;
  ImGui::SetNextItemWidth(Pixels(state, 112.0f));
  if (ImGui::SliderFloat("##horas", &hours, 0.0f, 24.0f, "%.1f h")) {
    SetSkyHours(state, hours);
  }
  ImGui::SetItemTooltip("Hora do dia.");
  ImGui::SameLine();
  const std::span<const render::SkyPreset> presets = render::SkyPresets();
  if (ImGui::BeginCombo("##ceu", presets[render::NearestSkyPreset(hours)].name)) {
    for (const render::SkyPreset& preset : presets) {
      if (ImGui::Selectable(preset.name)) {
        SetSkyHours(state, preset.hours);
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SetItemTooltip("Preset de céu.");
  ImGui::EndDisabled();
}

}
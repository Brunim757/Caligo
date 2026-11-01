#include "EditorPanels.h"

#include <imgui.h>
#include <string>
#include <vector>

namespace caligo::editor {

namespace {

ImU32 LevelColor(LogLevel level) {
  switch (level) {
  case LogLevel::Trace:
    return IM_COL32(132, 140, 152, 255);
  case LogLevel::Debug:
    return IM_COL32(150, 158, 170, 255);
  case LogLevel::Info:
    return IM_COL32(214, 220, 229, 255);
  case LogLevel::Warn:
    return IM_COL32(240, 190, 92, 255);
  case LogLevel::Error:
    return IM_COL32(242, 112, 104, 255);
  case LogLevel::Fatal:
    return IM_COL32(255, 72, 72, 255);
  default:
    return IM_COL32(214, 220, 229, 255);
  }
}

const char* LevelLabel(LogLevel level) {
  switch (level) {
  case LogLevel::Trace:
    return "Rastreio";
  case LogLevel::Debug:
    return "Depuração";
  case LogLevel::Info:
    return "Informação";
  case LogLevel::Warn:
    return "Aviso";
  case LogLevel::Error:
    return "Erro";
  case LogLevel::Fatal:
    return "Fatal";
  default:
    return "Tudo";
  }
}

void DrawConsoleToolbar(EditorState& state) {
  if (ImGui::Button("Limpar")) {
    state.console->Clear();
  }
  ImGui::SameLine();
  ImGui::Checkbox("Rolagem automática", &state.autoScrollConsole);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(Pixels(state, 190.0f));
  if (ImGui::BeginCombo("##console-level", LevelLabel(state.consoleMinimumLevel))) {
    for (int value = static_cast<int>(LogLevel::Trace); value <= static_cast<int>(LogLevel::Fatal);
         ++value) {
      const LogLevel level = static_cast<LogLevel>(value);
      if (ImGui::Selectable(LevelLabel(level), state.consoleMinimumLevel == level)) {
        state.consoleMinimumLevel = level;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%zu linhas", state.console->Count());
}

void DrawConsoleEntry(const ConsoleEntry& entry, size_t index) {
  ImGui::PushID(static_cast<int>(index));
  ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(entry.level));
  ImGui::TextUnformatted(entry.text.c_str());
  ImGui::PopStyleColor();
  if (ImGui::BeginPopupContextItem("##console-entry")) {
    if (ImGui::MenuItem("Copiar linha")) {
      ImGui::SetClipboardText(entry.text.c_str());
    }
    ImGui::EndPopup();
  }
  ImGui::PopID();
}

}

void DrawConsole(EditorState& state) {
  if (!ImGui::Begin("Console")) {
    ImGui::End();
    return;
  }

  DrawConsoleToolbar(state);
  ImGui::Separator();

  const std::vector<ConsoleEntry> entries = state.console->Snapshot();
  std::vector<const ConsoleEntry*> visible;
  visible.reserve(entries.size());
  for (const ConsoleEntry& entry : entries) {
    if (entry.level >= state.consoleMinimumLevel) {
      visible.push_back(&entry);
    }
  }

  if (state.consoleFont != nullptr) {
    ImGui::PushFont(state.consoleFont, 0.0f);
  }
  ImGui::BeginChild("##console-lines", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                    ImGuiWindowFlags_HorizontalScrollbar);
  if (visible.empty()) {
    ImGui::TextDisabled("Nenhuma mensagem no filtro atual.");
  } else {
    for (size_t index = 0; index < visible.size(); ++index) {
      DrawConsoleEntry(*visible[index], index);
    }
    if (state.autoScrollConsole &&
        (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - Pixels(state, 4.0f) ||
         ImGui::GetScrollMaxY() <= 0.0f)) {
      ImGui::SetScrollHereY(1.0f);
    }
  }
  ImGui::EndChild();
  if (state.consoleFont != nullptr) {
    ImGui::PopFont();
  }

  ImGui::End();
}

}

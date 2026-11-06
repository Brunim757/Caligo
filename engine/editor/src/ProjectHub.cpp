#include "EditorPanels.h"
#include <algorithm>
#include <caligo/platform/FileDialog.h>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <imgui.h>
#include <string>
#include <string_view>

namespace caligo::editor {

namespace {

constexpr char kProjectFilter[] =
    "Projetos Caligo (*.cproject)\0*.cproject\0Todos os arquivos (*.*)\0*.*\0";

constexpr ImU32 kText = IM_COL32(232, 235, 240, 255);
constexpr ImU32 kTextDim = IM_COL32(130, 136, 148, 255);
constexpr ImU32 kAccent = IM_COL32(0, 162, 255, 255);
constexpr ImU32 kCardBg = IM_COL32(27, 30, 36, 255);
constexpr ImU32 kRowBg = IM_COL32(31, 34, 41, 255);
constexpr ImU32 kRowHover = IM_COL32(38, 43, 51, 255);
constexpr ImU32 kBorder = IM_COL32(45, 49, 57, 255);

ImU32 WithAlpha(ImU32 color, int alpha) {
  return (color & 0x00FFFFFFu) | (static_cast<ImU32>(alpha) << 24);
}

std::string PathText(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

char LowerAscii(char character) {
  return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a')
                                              : character;
}

bool ContainsInsensitive(std::string_view text, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  if (needle.size() > text.size()) {
    return false;
  }
  for (size_t index = 0; index + needle.size() <= text.size(); ++index) {
    bool equal = true;
    for (size_t offset = 0; offset < needle.size(); ++offset) {
      if (LowerAscii(text[index + offset]) != LowerAscii(needle[offset])) {
        equal = false;
        break;
      }
    }
    if (equal) {
      return true;
    }
  }
  return false;
}

void DrawFolderIcon(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
  const float width = size;
  const float height = size * 0.68f;
  const ImVec2 min(center.x - width * 0.5f, center.y - height * 0.5f + size * 0.08f);
  const float tabWidth = width * 0.44f;
  const float tabHeight = height * 0.26f;
  const float rounding = size * 0.09f;
  const ImVec2 tabMin(min.x, min.y - tabHeight + rounding);
  const ImVec2 tabMax(min.x + tabWidth, min.y + rounding);
  draw->AddRectFilled(tabMin, tabMax, color, rounding, ImDrawFlags_RoundCornersTop);
  draw->AddRectFilled(min, ImVec2(min.x + width, min.y + height), color, rounding);
}

void DrawChevron(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
  const ImVec2 first(center.x - size * 0.35f, center.y - size * 0.5f);
  const ImVec2 tip(center.x + size * 0.35f, center.y);
  const ImVec2 last(center.x - size * 0.35f, center.y + size * 0.5f);
  draw->AddLine(first, tip, color, 1.6f);
  draw->AddLine(tip, last, color, 1.6f);
}

void DrawLogo(EditorState& state, ImVec2 position, float size) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 max(position.x + size, position.y + size);
  draw->AddRectFilled(position, max, IM_COL32(0, 132, 220, 255), size * 0.26f);
  draw->AddRectFilled(position, ImVec2(max.x, position.y + size * 0.52f),
                      IM_COL32(255, 255, 255, 20), size * 0.26f, ImDrawFlags_RoundCornersTop);
  ImFont* font = state.hubTitleFont != nullptr ? state.hubTitleFont : ImGui::GetFont();
  const float fontSize = size * 0.64f;
  const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "C");
  draw->AddText(
      font, fontSize,
      ImVec2(position.x + (size - textSize.x) * 0.5f, position.y + (size - textSize.y) * 0.5f),
      IM_COL32(255, 255, 255, 255), "C");
}

void DrawHubBackground(ImVec2 position, ImVec2 size) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 max(position.x + size.x, position.y + size.y);
  draw->AddRectFilledMultiColor(position, max, IM_COL32(31, 35, 43, 255), IM_COL32(27, 30, 37, 255),
                                IM_COL32(15, 17, 21, 255), IM_COL32(18, 20, 25, 255));
  draw->AddRectFilledMultiColor(position, ImVec2(max.x, position.y + size.y * 0.55f),
                                IM_COL32(0, 122, 214, 22), IM_COL32(0, 122, 214, 10),
                                IM_COL32(0, 122, 214, 0), IM_COL32(0, 122, 214, 0));
  draw->AddRectFilled(position, ImVec2(max.x, position.y + 3.0f), kAccent);
}

void DrawBrand(EditorState& state, float left, float right, float top, float logoSize) {
  DrawLogo(state, ImVec2(left, top), logoSize);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  ImFont* titleFont = state.hubTitleFont != nullptr ? state.hubTitleFont : ImGui::GetFont();
  draw->AddText(titleFont, Pixels(state, 27.0f),
                ImVec2(left + logoSize + Pixels(state, 14.0f), top + Pixels(state, 1.0f)), kText,
                "Caligo");
  draw->AddText(ImGui::GetFont(), Pixels(state, 12.5f),
                ImVec2(left + logoSize + Pixels(state, 15.0f), top + Pixels(state, 34.0f)),
                kTextDim, "Kizuri Studio · Hub de projetos");
  const std::string version = std::format("R3 · v{}", CALIGO_VERSION);
  const ImVec2 versionSize =
      ImGui::GetFont()->CalcTextSizeA(Pixels(state, 13.0f), FLT_MAX, 0.0f, version.c_str());
  draw->AddText(ImGui::GetFont(), Pixels(state, 13.0f),
                ImVec2(right - versionSize.x, top + Pixels(state, 4.0f)), kTextDim,
                version.c_str());
}

void PushCardStyle(EditorState& state) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(kCardBg));
  ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(kBorder));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Pixels(state, 10.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(Pixels(state, 18.0f), Pixels(state, 16.0f)));
}

void PopCardStyle() {
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(2);
}

void TextCentered(const char* text) {
  const float width = ImGui::CalcTextSize(text).x;
  const float available = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (available - width) * 0.5f));
  ImGui::TextUnformatted(text);
}

void DrawEmptyRecent(EditorState& state, bool filtered) {
  const ImVec2 available = ImGui::GetContentRegionAvail();
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + available.y * 0.14f);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float iconSize = Pixels(state, 54.0f);
  DrawFolderIcon(ImGui::GetWindowDrawList(),
                 ImVec2(start.x + available.x * 0.5f, start.y + iconSize * 0.5f), iconSize,
                 WithAlpha(kAccent, 170));
  ImGui::Dummy(ImVec2(available.x, iconSize + Pixels(state, 14.0f)));
  ImGui::PushFont(state.hubHeadingFont, 18.0f);
  TextCentered(filtered ? "Nenhum projeto encontrado" : "Nenhum projeto ainda");
  ImGui::PopFont();
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kTextDim));
  TextCentered(filtered ? "Tente outro termo de busca."
                        : "Crie um projeto ao lado ou abra um .cproject existente.");
  ImGui::PopStyleColor();
}

bool DrawRecentRow(EditorState& state, const RecentProject& recent, int index, float width) {
  ImGui::PushID(index);
  const float rowHeight = Pixels(state, 62.0f);
  const ImVec2 min = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##recent-project", ImVec2(width, rowHeight));
  const bool hovered = ImGui::IsItemHovered();
  const bool open = hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 max(min.x + width, min.y + rowHeight);
  const float rounding = Pixels(state, 8.0f);
  draw->AddRectFilled(min, max, hovered ? kRowHover : kRowBg, rounding);
  draw->AddRect(min, max, hovered ? WithAlpha(kAccent, 120) : kBorder, rounding);
  const float tileSize = Pixels(state, 38.0f);
  const ImVec2 tileMin(min.x + Pixels(state, 11.0f), min.y + (rowHeight - tileSize) * 0.5f);
  draw->AddRectFilled(tileMin, ImVec2(tileMin.x + tileSize, tileMin.y + tileSize),
                      WithAlpha(kAccent, 30), Pixels(state, 8.0f));
  DrawFolderIcon(draw, ImVec2(tileMin.x + tileSize * 0.5f, tileMin.y + tileSize * 0.5f),
                 tileSize * 0.52f, kAccent);
  const float textX = tileMin.x + tileSize + Pixels(state, 12.0f);
  ImFont* bold = state.hubHeadingFont != nullptr ? state.hubHeadingFont : ImGui::GetFont();
  draw->AddText(bold, Pixels(state, 15.5f), ImVec2(textX, min.y + Pixels(state, 11.0f)), kText,
                recent.name.c_str());
  const std::string path = PathText(recent.path);
  draw->PushClipRect(ImVec2(textX, min.y), ImVec2(max.x - Pixels(state, 36.0f), max.y), true);
  draw->AddText(ImGui::GetFont(), Pixels(state, 12.0f), ImVec2(textX, min.y + Pixels(state, 33.0f)),
                kTextDim, path.c_str());
  draw->PopClipRect();
  DrawChevron(draw, ImVec2(max.x - Pixels(state, 18.0f), min.y + rowHeight * 0.5f),
              Pixels(state, 8.0f), hovered ? kText : kTextDim);
  if (hovered) {
    ImGui::SetTooltip("Abrir %s", path.c_str());
  }
  ImGui::PopID();
  return open;
}

void DrawRecentProjects(EditorState& state, float width, float height) {
  PushCardStyle(state);
  ImGui::BeginChild("##recent-projects", ImVec2(width, height), ImGuiChildFlags_Borders);
  ImGui::PushFont(state.hubHeadingFont, 18.0f);
  ImGui::TextUnformatted("Projetos recentes");
  ImGui::PopFont();
  const std::string count = std::format("{} projeto{}", state.recentProjects.size(),
                                        state.recentProjects.size() == 1 ? "" : "s");
  const float countWidth = ImGui::CalcTextSize(count.c_str()).x;
  const float countOffset =
      ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - countWidth;
  ImGui::SameLine(countOffset);
  ImGui::TextDisabled("%s", count.c_str());
  ImGui::TextDisabled("Selecione um projeto para abrir.");
  ImGui::Spacing();
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(Pixels(state, 10.0f), Pixels(state, 6.0f)));
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputTextWithHint("##project-search", "Buscar projeto...", state.projectSearch.data(),
                           state.projectSearch.size());
  ImGui::PopStyleVar();
  ImGui::Spacing();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::BeginChild("##recent-list", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoBackground);
  const std::string_view needle = state.projectSearch.data();
  const float rowWidth = ImGui::GetContentRegionAvail().x;
  size_t visible = 0;
  std::filesystem::path pendingOpen;
  for (size_t index = 0; index < state.recentProjects.size(); ++index) {
    const RecentProject& recent = state.recentProjects[index];
    if (!ContainsInsensitive(recent.name, needle) &&
        !ContainsInsensitive(PathText(recent.path), needle)) {
      continue;
    }
    if (visible > 0) {
      ImGui::Dummy(ImVec2(0.0f, Pixels(state, 6.0f)));
    }
    if (DrawRecentRow(state, recent, static_cast<int>(index), rowWidth)) {
      pendingOpen = recent.path;
    }
    ++visible;
  }
  if (visible == 0) {
    DrawEmptyRecent(state, !needle.empty());
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::EndChild();
  PopCardStyle();
  if (!pendingOpen.empty()) {
    OpenProject(state, pendingOpen);
  }
}

bool ChooseProjectFolder(EditorState& state) {
  const Result<std::optional<std::filesystem::path>> chosen = platform::ShowOpenFolderDialog(
      "Selecionar pasta do projeto", state.newProjectLocation,
      state.window != nullptr ? state.window->NativeHandle() : nullptr);
  if (!chosen) {
    ShowError(state, "Não foi possível abrir o seletor de pastas", chosen.GetError().message);
    return false;
  }
  if (!chosen.Value().has_value()) {
    return false;
  }
  state.newProjectLocation = *chosen.Value();
  return true;
}

void DrawNewProject(EditorState& state, float width, float height) {
  PushCardStyle(state);
  ImGui::BeginChild("##new-project", ImVec2(width, height), ImGuiChildFlags_Borders);
  ImGui::PushFont(state.hubHeadingFont, 18.0f);
  ImGui::TextUnformatted("Novo projeto");
  ImGui::PopFont();
  ImGui::TextDisabled("Comece um jogo novo com a estrutura da Caligo.");
  ImGui::Spacing();
  ImGui::Spacing();
  ImGui::TextDisabled("Nome do projeto");
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(Pixels(state, 10.0f), Pixels(state, 7.0f)));
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputText("##project-name", state.newProjectName.data(), state.newProjectName.size());
  ImGui::PopStyleVar();
  ImGui::Spacing();
  ImGui::TextDisabled("Pasta de destino");
  if (ImGui::Button("Escolher pasta...", ImVec2(-FLT_MIN, Pixels(state, 30.0f)))) {
    static_cast<void>(ChooseProjectFolder(state));
  }
  if (state.newProjectLocation.empty()) {
    ImGui::TextDisabled("Nenhuma pasta escolhida.");
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kTextDim));
    ImGui::TextWrapped("%s", PathText(state.newProjectLocation).c_str());
    ImGui::PopStyleColor();
  }
  ImGui::Spacing();
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 130, 220, 255));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 156, 250, 255));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 112, 190, 255));
  if (ImGui::Button("Criar projeto", ImVec2(-FLT_MIN, Pixels(state, 38.0f)))) {
    if (state.newProjectLocation.empty() && !ChooseProjectFolder(state)) {
      ShowError(state, "Escolha uma pasta", "Selecione onde o projeto será criado.");
    } else {
      CreateProject(state, state.newProjectLocation, state.newProjectName.data());
    }
  }
  ImGui::PopStyleColor(3);
  ImGui::Spacing();
  ImGui::SeparatorText("ou");
  ImGui::Spacing();
  if (ImGui::Button("Abrir projeto existente...", ImVec2(-FLT_MIN, Pixels(state, 34.0f)))) {
    const Result<std::optional<std::filesystem::path>> chosen = platform::ShowOpenFileDialog(
        "Abrir projeto", std::string_view(kProjectFilter, sizeof(kProjectFilter) - 1),
        std::filesystem::path{}, state.window != nullptr ? state.window->NativeHandle() : nullptr);
    if (chosen && chosen.Value().has_value()) {
      OpenProject(state, *chosen.Value());
    } else if (!chosen) {
      ShowError(state, "Não foi possível abrir o seletor de arquivos", chosen.GetError().message);
    }
  }
  const float footer = ImGui::GetWindowHeight() - Pixels(state, 26.0f);
  if (ImGui::GetCursorPosY() < footer) {
    ImGui::SetCursorPosY(footer);
  }
  ImGui::TextDisabled("Windows x64 · Direct3D 11 · R3");
  ImGui::EndChild();
  PopCardStyle();
}

}

void DrawProjectHub(EditorState& state) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse;
  ImGui::Begin("##caligo-hub", nullptr, kFlags);

  const ImVec2 origin = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  DrawHubBackground(origin, size);

  const float sideMargin = std::clamp(size.x * 0.06f, Pixels(state, 24.0f), Pixels(state, 96.0f));
  const float left = origin.x + sideMargin;
  const float right = origin.x + size.x - sideMargin;
  const float contentWidth = std::max(right - left, Pixels(state, 1.0f));
  const float logoTop = origin.y + Pixels(state, 30.0f);
  DrawBrand(state, left, right, logoTop, Pixels(state, 38.0f));

  const float aboutWidth = Pixels(state, 62.0f);
  ImGui::SetCursorScreenPos(ImVec2(right - aboutWidth, logoTop + Pixels(state, 26.0f)));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(Pixels(state, 10.0f), Pixels(state, 3.0f)));
  ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 255, 255, 8));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 20));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 30));
  if (ImGui::Button("Sobre", ImVec2(aboutWidth, 0.0f))) {
    state.showAboutPopup = true;
  }
  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar();

  const float headerLine = origin.y + Pixels(state, 94.0f);
  ImGui::GetWindowDrawList()->AddLine(ImVec2(left, headerLine), ImVec2(right, headerLine), kBorder,
                                      1.0f);

  const float contentTop = headerLine + Pixels(state, 20.0f);
  const float contentHeight =
      std::max(size.y - (contentTop - origin.y) - Pixels(state, 30.0f), Pixels(state, 120.0f));
  const float gap = Pixels(state, 20.0f);
  const float newWidth =
      std::min(std::clamp(contentWidth * 0.33f, Pixels(state, 280.0f), Pixels(state, 400.0f)),
               contentWidth * 0.5f);
  const float recentWidth = std::max(contentWidth - newWidth - gap, Pixels(state, 1.0f));

  ImGui::SetCursorScreenPos(ImVec2(left, contentTop));
  DrawRecentProjects(state, recentWidth, contentHeight);
  ImGui::SetCursorScreenPos(ImVec2(left + recentWidth + gap, contentTop));
  DrawNewProject(state, newWidth, contentHeight);

  ImGui::End();
  ImGui::PopStyleVar(3);
}

}

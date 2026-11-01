#include "EditorPanels.h"
#include <caligo/core/Log.h>
#include <imgui.h>
#include <string>
#include <vector>

namespace caligo::editor {

namespace {

std::vector<scene::EntityId> ChildrenOf(const EditorState& state, scene::EntityId parent) {
  std::vector<scene::EntityId> children;
  for (const scene::Entity& entity : state.scene.Entities()) {
    if (entity.parent == parent) {
      children.push_back(entity.id);
    }
  }
  return children;
}

void DrawEntityNode(EditorState& state, scene::EntityId id) {
  const scene::Entity* entity = state.scene.Find(id);
  if (entity == nullptr) {
    return;
  }

  const std::vector<scene::EntityId> children = ChildrenOf(state, id);
  const bool selected = state.selected == id;
  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                             ImGuiTreeNodeFlags_DefaultOpen;
  if (selected) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }
  if (children.empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf;
  }

  ImGui::PushID(static_cast<int>(id));
  const bool open = ImGui::TreeNodeEx("##entity", flags, "%s", entity->name.c_str());
  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
    SelectEntity(state, id);
  }

  if (!state.play.active && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
    ImGui::SetDragDropPayload("CALIGO_ENTITY", &id, sizeof(id));
    ImGui::TextUnformatted(entity->name.c_str());
    ImGui::EndDragDropSource();
  }
  if (!state.play.active && ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CALIGO_ENTITY")) {
      if (payload->Data != nullptr && payload->DataSize == static_cast<int>(sizeof(id))) {
        const scene::EntityId dragged = *static_cast<const scene::EntityId*>(payload->Data);
        if (dragged != id && !state.scene.SetParent(dragged, id)) {
          CALIGO_LOG_WARN("não foi possível mover a entidade {} para {}", dragged, id);
        }
      }
    }
    ImGui::EndDragDropTarget();
  }

  bool deleted = false;
  if (!state.play.active && ImGui::BeginPopupContextItem("##entity-menu")) {
    if (ImGui::MenuItem("Criar filha")) {
      SelectEntity(state, id);
      CreateEntity(state);
    }
    if (ImGui::MenuItem("Remover pai", nullptr, false, entity->parent != scene::kNoEntity)) {
      if (!state.scene.SetParent(id, scene::kNoEntity)) {
        CALIGO_LOG_WARN("não foi possível remover o pai da entidade {}", id);
      }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Excluir")) {
      SelectEntity(state, id);
      DeleteSelectedEntity(state);
      deleted = true;
    }
    ImGui::EndPopup();
  }

  if (open) {
    if (!deleted) {
      for (const scene::EntityId child : children) {
        DrawEntityNode(state, child);
      }
    }
    ImGui::TreePop();
  }
  ImGui::PopID();
}

}

void DrawHierarchy(EditorState& state) {
  if (!ImGui::Begin("Hierarquia")) {
    ImGui::End();
    return;
  }

  ImGui::BeginDisabled(state.play.active);
  if (ImGui::Button("+ Entidade")) {
    CreateEntity(state);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(state.selected == scene::kNoEntity || state.play.active);
  if (ImGui::Button("Excluir")) {
    DeleteSelectedEntity(state);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::TextDisabled("%zu", state.scene.Entities().size());
  ImGui::Separator();

  ImGui::BeginChild("##hierarchy-tree", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                    ImGuiWindowFlags_HorizontalScrollbar);
  const std::vector<scene::EntityId> roots = ChildrenOf(state, scene::kNoEntity);
  if (roots.empty()) {
    ImGui::TextDisabled("Nenhuma entidade na cena");
  } else {
    for (const scene::EntityId root : roots) {
      DrawEntityNode(state, root);
    }
  }
  ImGui::EndChild();
  ImGui::End();
}

}

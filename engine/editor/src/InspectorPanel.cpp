#include "EditorPanels.h"

#include <algorithm>
#include <caligo/core/Log.h>
#include <cmath>
#include <imgui.h>
#include <string>

namespace caligo::editor {

namespace {

void EnsureInspectorCaches(EditorState& state, const scene::Entity& entity) {
  if (state.nameOwner != entity.id) {
    const size_t count = std::min(entity.name.size(), state.nameBuffer.size() - 1);
    std::copy_n(entity.name.begin(), static_cast<std::ptrdiff_t>(count), state.nameBuffer.begin());
    state.nameBuffer[count] = '\0';
    state.nameOwner = entity.id;
  }
  if (state.eulerOwner != entity.id || state.eulerRevision != state.scene.Revision()) {
    state.eulerCache = math::EulerDegreesFromRotation(entity.transform.rotation);
    state.eulerOwner = entity.id;
    state.eulerRevision = state.scene.Revision();
  }
}

void DrawParentCombo(EditorState& state, const scene::Entity& entity) {
  const scene::Entity* parent = state.scene.Find(entity.parent);
  const char* preview = parent != nullptr ? parent->name.c_str() : "Nenhum";
  if (!ImGui::BeginCombo("Pai", preview)) {
    return;
  }

  if (ImGui::Selectable("Nenhum", entity.parent == scene::kNoEntity)) {
    if (!state.scene.SetParent(entity.id, scene::kNoEntity)) {
      CALIGO_LOG_WARN("não foi possível remover o pai da entidade {}", entity.id);
    }
  }
  for (const scene::Entity& candidate : state.scene.Entities()) {
    if (candidate.id == entity.id || state.scene.IsDescendant(entity.id, candidate.id)) {
      continue;
    }
    ImGui::PushID(static_cast<int>(candidate.id));
    if (ImGui::Selectable(candidate.name.c_str(), entity.parent == candidate.id)) {
      if (!state.scene.SetParent(entity.id, candidate.id)) {
        CALIGO_LOG_WARN("não foi possível definir {} como pai de {}", candidate.id, entity.id);
      }
      ImGui::PopID();
      break;
    }
    ImGui::PopID();
  }
  ImGui::EndCombo();
}

void DrawTransform(EditorState& state, const scene::Entity& entity) {
  scene::Transform transform = entity.transform;
  bool changed = false;
  bool rotationEdited = false;

  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.64f);
  if (ImGui::DragFloat3("Posição", &transform.position.x, 0.05f, 0.0f, 0.0f, "%.3f")) {
    changed = true;
  }

  math::Vec3 euler = state.eulerCache;
  if (ImGui::DragFloat3("Rotação", &euler.x, 0.5f, 0.0f, 0.0f, "%.1f")) {
    transform.rotation = math::RotationFromEulerDegrees(euler);
    state.eulerCache = euler;
    changed = true;
    rotationEdited = true;
  }

  if (ImGui::DragFloat3("Escala", &transform.scale.x, 0.05f, 0.001f, 10000.0f, "%.3f")) {
    changed = true;
  }
  ImGui::PopItemWidth();

  if (changed) {
    if (state.scene.SetTransform(entity.id, transform)) {
      if (rotationEdited) {
        state.eulerOwner = entity.id;
        state.eulerRevision = state.scene.Revision();
      }
    } else {
      CALIGO_LOG_WARN("transformação inválida para a entidade {}", entity.id);
    }
  }

  if (ImGui::Button("Redefinir transformação")) {
    if (!state.scene.SetTransform(entity.id, scene::Transform{})) {
      CALIGO_LOG_WARN("não foi possível redefinir a transformação da entidade {}", entity.id);
    }
    state.eulerOwner = scene::kNoEntity;
  }
}

}

void DrawInspector(EditorState& state) {
  if (!ImGui::Begin("Inspetor")) {
    ImGui::End();
    return;
  }

  scene::Entity* entity = state.scene.Find(state.selected);
  if (entity == nullptr) {
    state.selected = scene::kNoEntity;
    state.nameOwner = scene::kNoEntity;
    state.eulerOwner = scene::kNoEntity;
    ImGui::TextDisabled("Nenhuma entidade selecionada.");
    ImGui::Spacing();
    if (ImGui::Button("Criar entidade")) {
      CreateEntity(state);
    }
    ImGui::End();
    return;
  }

  EnsureInspectorCaches(state, *entity);

  ImGui::TextDisabled("ID %llu", static_cast<unsigned long long>(entity->id));
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputText("##name", state.nameBuffer.data(), state.nameBuffer.size());
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    if (!state.scene.Rename(entity->id, state.nameBuffer.data())) {
      CALIGO_LOG_WARN("não foi possível renomear a entidade {}", entity->id);
    }
    entity = state.scene.Find(state.selected);
    if (entity != nullptr) {
      state.nameOwner = scene::kNoEntity;
      EnsureInspectorCaches(state, *entity);
    }
  }

  ImGui::Spacing();
  DrawParentCombo(state, *entity);

  ImGui::SeparatorText("Componentes");
  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    entity = state.scene.Find(state.selected);
    if (entity != nullptr) {
      DrawTransform(state, *entity);
    }
  }

  ImGui::Separator();
  if (ImGui::Button("Excluir entidade")) {
    DeleteSelectedEntity(state);
  }

  ImGui::End();
}

}

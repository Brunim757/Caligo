#include <algorithm>
#include <caligo/scene/Scene.h>
#include <cmath>
#include <format>
#include <limits>
#include <random>
#include <unordered_set>
#include <utility>

namespace caligo::scene {

namespace {

bool SameVec3(const math::Vec3& left, const math::Vec3& right) noexcept {
  return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool SameQuat(const math::Quat& left, const math::Quat& right) noexcept {
  return left.x == right.x && left.y == right.y && left.z == right.z && left.w == right.w;
}

bool SameTransform(const Transform& left, const Transform& right) noexcept {
  return SameVec3(left.position, right.position) && SameQuat(left.rotation, right.rotation) &&
         SameVec3(left.scale, right.scale);
}

bool SameEnvironment(const WorldEnvironment& left, const WorldEnvironment& right) noexcept {
  return left.skyHours == right.skyHours;
}

bool IsValidEnvironment(const WorldEnvironment& environment) noexcept {
  return std::isfinite(environment.skyHours) && environment.skyHours >= 0.0f &&
         environment.skyHours <= 24.0f;
}

const Entity* FindEntity(const std::vector<Entity>& entities, EntityId id) noexcept {
  const auto found = std::find_if(entities.begin(), entities.end(),
                                  [id](const Entity& entity) { return entity.id == id; });
  return found == entities.end() ? nullptr : &(*found);
}

uint32_t HierarchyDepth(const std::vector<Entity>& entities, EntityId id) noexcept {
  uint32_t depth = 0;
  EntityId current = id;
  while (current != kNoEntity && depth <= kMaxHierarchyDepth) {
    const Entity* entity = FindEntity(entities, current);
    if (entity == nullptr) {
      break;
    }
    ++depth;
    current = entity->parent;
  }
  return depth;
}

uint32_t SubtreeHeight(const std::vector<Entity>& entities, EntityId id) noexcept {
  uint32_t height = 1;
  for (const Entity& entity : entities) {
    if (entity.id == id) {
      continue;
    }
    EntityId current = entity.id;
    uint32_t relativeDepth = 0;
    while (current != kNoEntity && relativeDepth <= kMaxHierarchyDepth) {
      if (current == id) {
        height = std::max(height, relativeDepth + 1);
        break;
      }
      ++relativeDepth;
      const Entity* found = FindEntity(entities, current);
      if (found == nullptr) {
        break;
      }
      current = found->parent;
    }
  }
  return height;
}

}

std::string GenerateGuid() {
  std::random_device device;
  std::mt19937_64 engine((static_cast<uint64_t>(device()) << 32) ^ static_cast<uint64_t>(device()));
  return std::format("{:016x}{:016x}", engine(), engine());
}

bool IsValidGuid(std::string_view guid) noexcept {
  if (guid.size() != 32) {
    return false;
  }
  for (const char character : guid) {
    const bool digit = character >= '0' && character <= '9';
    const bool lower = character >= 'a' && character <= 'f';
    if (!digit && !lower) {
      return false;
    }
  }
  return true;
}

bool IsValidScriptType(std::string_view type) noexcept {
  if (type.empty() || type.size() > kMaxScriptTypeLength) {
    return false;
  }
  const char first = type.front();
  const bool firstLetter = (first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z');
  if (!firstLetter) {
    return false;
  }
  bool previousDot = false;
  for (size_t index = 1; index < type.size(); ++index) {
    const char character = type[index];
    const bool letter =
        (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
    const bool digit = character >= '0' && character <= '9';
    if (letter || digit || character == '_') {
      previousDot = false;
      continue;
    }
    if (character == '.' && !previousDot) {
      previousDot = true;
      continue;
    }
    return false;
  }
  return !previousDot;
}

bool IsFinite(const math::Vec3& value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFinite(const math::Quat& value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
         std::isfinite(value.w);
}

bool IsFinite(const Transform& transform) noexcept {
  return IsFinite(transform.position) && IsFinite(transform.rotation) && IsFinite(transform.scale);
}

Scene::Scene() : m_guid(GenerateGuid()) {}

const std::string& Scene::Guid() const noexcept {
  return m_guid;
}

bool Scene::SetGuid(std::string_view guid) {
  if (!IsValidGuid(guid)) {
    return false;
  }
  m_guid.assign(guid);
  ++m_revision;
  return true;
}

const WorldEnvironment& Scene::Environment() const noexcept {
  return m_environment;
}

bool Scene::SetEnvironment(const WorldEnvironment& environment) {
  if (!IsValidEnvironment(environment)) {
    return false;
  }
  if (SameEnvironment(m_environment, environment)) {
    return true;
  }
  m_environment = environment;
  ++m_revision;
  return true;
}

EntityId Scene::CreateEntity(std::string_view name, EntityId parent) {
  if (parent != kNoEntity &&
      (Find(parent) == nullptr || HierarchyDepth(m_entities, parent) >= kMaxHierarchyDepth)) {
    return kNoEntity;
  }
  const EntityId id = m_nextId++;
  Entity entity;
  entity.id = id;
  entity.parent = parent;
  entity.name = name.empty() ? "Entidade" : std::string(name);
  m_entities.push_back(std::move(entity));
  ++m_revision;
  return id;
}

bool Scene::InsertEntity(EntityId id, std::string name, EntityId parent,
                         const Transform& transform) {
  if (id == kNoEntity || id == std::numeric_limits<EntityId>::max() || Find(id) != nullptr ||
      !IsFinite(transform)) {
    return false;
  }
  if (parent != kNoEntity && Find(parent) != nullptr &&
      HierarchyDepth(m_entities, parent) >= kMaxHierarchyDepth) {
    return false;
  }
  Entity entity;
  entity.id = id;
  entity.parent = parent;
  entity.name = name.empty() ? "Entidade" : std::move(name);
  entity.transform = transform;
  entity.transform.rotation = math::NormalizeRotation(transform.rotation);
  m_entities.push_back(std::move(entity));
  m_nextId = std::max(m_nextId, id + 1);
  ++m_revision;
  return true;
}

bool Scene::DestroyEntity(EntityId id) {
  if (Find(id) == nullptr) {
    return false;
  }

  std::vector<EntityId> removed{id};
  for (size_t index = 0; index < removed.size(); ++index) {
    for (const Entity& entity : m_entities) {
      if (entity.parent == removed[index]) {
        removed.push_back(entity.id);
      }
    }
  }

  const size_t previousSize = m_entities.size();
  m_entities.erase(std::remove_if(m_entities.begin(), m_entities.end(),
                                  [&removed](const Entity& entity) {
                                    return std::find(removed.begin(), removed.end(), entity.id) !=
                                           removed.end();
                                  }),
                   m_entities.end());
  if (m_entities.size() == previousSize) {
    return false;
  }
  ++m_revision;
  return true;
}

bool Scene::SetParent(EntityId id, EntityId parent) {
  Entity* entity = Find(id);
  if (entity == nullptr) {
    return false;
  }
  if (entity->parent == parent) {
    return true;
  }
  if (parent != kNoEntity &&
      (parent == id || Find(parent) == nullptr || IsDescendant(id, parent))) {
    return false;
  }
  if (parent != kNoEntity) {
    const uint32_t parentDepth = HierarchyDepth(m_entities, parent);
    const uint32_t height = SubtreeHeight(m_entities, id);
    if (parentDepth >= kMaxHierarchyDepth || parentDepth + height > kMaxHierarchyDepth) {
      return false;
    }
  }
  entity->parent = parent;
  ++m_revision;
  return true;
}

bool Scene::Rename(EntityId id, std::string_view name) {
  Entity* entity = Find(id);
  if (entity == nullptr) {
    return false;
  }
  if (entity->name == name) {
    return true;
  }
  entity->name = name.empty() ? "Entidade" : std::string(name);
  ++m_revision;
  return true;
}

bool Scene::SetTransform(EntityId id, const Transform& transform) {
  Entity* entity = Find(id);
  if (entity == nullptr || !IsFinite(transform)) {
    return false;
  }
  Transform normalized = transform;
  normalized.rotation = math::NormalizeRotation(transform.rotation);
  if (SameTransform(entity->transform, normalized)) {
    return true;
  }
  entity->transform = normalized;
  ++m_revision;
  return true;
}

bool Scene::AddScript(EntityId id, std::string_view type) {
  Entity* entity = Find(id);
  if (entity == nullptr || !IsValidScriptType(type) ||
      entity->scripts.size() >= kMaxScriptsPerEntity) {
    return false;
  }
  const bool duplicate =
      std::any_of(entity->scripts.begin(), entity->scripts.end(),
                  [type](const ScriptComponent& script) { return script.type == type; });
  if (duplicate) {
    return false;
  }
  ScriptComponent script;
  script.type.assign(type);
  entity->scripts.push_back(std::move(script));
  ++m_revision;
  return true;
}

bool Scene::RemoveScript(EntityId id, std::string_view type) {
  Entity* entity = Find(id);
  if (entity == nullptr) {
    return false;
  }
  const auto found =
      std::find_if(entity->scripts.begin(), entity->scripts.end(),
                   [type](const ScriptComponent& script) { return script.type == type; });
  if (found == entity->scripts.end()) {
    return false;
  }
  entity->scripts.erase(found);
  ++m_revision;
  return true;
}

Entity* Scene::Find(EntityId id) noexcept {
  for (Entity& entity : m_entities) {
    if (entity.id == id) {
      return &entity;
    }
  }
  return nullptr;
}

const Entity* Scene::Find(EntityId id) const noexcept {
  for (const Entity& entity : m_entities) {
    if (entity.id == id) {
      return &entity;
    }
  }
  return nullptr;
}

const std::vector<Entity>& Scene::Entities() const noexcept {
  return m_entities;
}

bool Scene::IsDescendant(EntityId ancestor, EntityId candidate) const noexcept {
  const Entity* entity = Find(candidate);
  if (entity == nullptr) {
    return false;
  }
  EntityId current = entity->parent;
  for (uint32_t depth = 0; current != kNoEntity && depth <= kMaxHierarchyDepth; ++depth) {
    if (current == ancestor) {
      return true;
    }
    entity = Find(current);
    if (entity == nullptr) {
      return false;
    }
    current = entity->parent;
  }
  return false;
}

math::Mat4 Scene::WorldMatrix(EntityId id) const noexcept {
  math::Mat4 result = math::Identity();
  EntityId current = id;
  for (uint32_t depth = 0; current != kNoEntity && depth <= kMaxHierarchyDepth; ++depth) {
    const Entity* entity = Find(current);
    if (entity == nullptr) {
      return result;
    }
    result = math::Multiply(result, math::ComposeTransform(entity->transform.position,
                                                           entity->transform.rotation,
                                                           entity->transform.scale));
    current = entity->parent;
  }
  return result;
}

Result<void> Scene::Validate() const {
  if (!IsValidGuid(m_guid)) {
    return Error{"scene GUID is invalid", 1};
  }
  if (!IsValidEnvironment(m_environment)) {
    return Error{"scene environment sky hours must be between 0 and 24", 12};
  }

  std::unordered_set<EntityId> ids;
  EntityId largestId = 0;
  for (const Entity& entity : m_entities) {
    if (entity.id == kNoEntity) {
      return Error{"scene contains an entity with id zero", 2};
    }
    if (!ids.insert(entity.id).second) {
      return Error{std::format("scene contains duplicate entity id {}", entity.id), 3};
    }
    if (!IsFinite(entity.transform)) {
      return Error{std::format("entity {} has a non-finite transform", entity.id), 4};
    }
    if (entity.scripts.size() > kMaxScriptsPerEntity) {
      return Error{std::format("entity {} has too many script components", entity.id), 9};
    }
    for (size_t index = 0; index < entity.scripts.size(); ++index) {
      const std::string& type = entity.scripts[index].type;
      if (!IsValidScriptType(type)) {
        return Error{std::format("entity {} has an invalid script type '{}'", entity.id, type), 10};
      }
      for (size_t other = 0; other < index; ++other) {
        if (entity.scripts[other].type == type) {
          return Error{std::format("entity {} repeats the script type '{}'", entity.id, type), 11};
        }
      }
    }
    largestId = std::max(largestId, entity.id);
  }

  for (const Entity& entity : m_entities) {
    if (entity.parent != kNoEntity && ids.find(entity.parent) == ids.end()) {
      return Error{std::format("entity {} references missing parent {}", entity.id, entity.parent),
                   5};
    }
  }

  for (const Entity& entity : m_entities) {
    EntityId current = entity.id;
    for (uint32_t depth = 0; current != kNoEntity; ++depth) {
      if (depth >= kMaxHierarchyDepth) {
        return Error{std::format("entity {} hierarchy is deeper than {} levels", entity.id,
                                 kMaxHierarchyDepth),
                     6};
      }
      const Entity* parentEntity = Find(current);
      if (parentEntity == nullptr) {
        return Error{std::format("entity {} references missing ancestor {}", entity.id, current),
                     7};
      }
      current = parentEntity->parent;
    }
  }

  if (m_nextId <= largestId) {
    return Error{"scene next entity id is not ahead of the existing ids", 8};
  }
  return {};
}

uint64_t Scene::Revision() const noexcept {
  return m_revision;
}

}

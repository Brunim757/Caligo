#pragma once

#include <caligo/core/Math.h>
#include <caligo/core/Result.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace caligo::scene {

using EntityId = uint64_t;

inline constexpr EntityId kNoEntity = 0;
inline constexpr uint32_t kMaxHierarchyDepth = 256;
inline constexpr size_t kMaxScriptsPerEntity = 32;
inline constexpr size_t kMaxScriptTypeLength = 128;

struct Transform {
  math::Vec3 position{0.0f, 0.0f, 0.0f};
  math::Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
  math::Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct ScriptComponent {
  std::string type;
};

struct WorldEnvironment {
  float skyHours = 12.0f;
};

struct Entity {
  EntityId id = kNoEntity;
  EntityId parent = kNoEntity;
  std::string name;
  Transform transform;
  std::vector<ScriptComponent> scripts;
};

[[nodiscard]] std::string GenerateGuid();
[[nodiscard]] bool IsValidGuid(std::string_view guid) noexcept;
[[nodiscard]] bool IsValidScriptType(std::string_view type) noexcept;
[[nodiscard]] bool IsFinite(const math::Vec3& value) noexcept;
[[nodiscard]] bool IsFinite(const math::Quat& value) noexcept;
[[nodiscard]] bool IsFinite(const Transform& transform) noexcept;

class Scene final {
public:
  Scene();

  [[nodiscard]] const std::string& Guid() const noexcept;
  [[nodiscard]] bool SetGuid(std::string_view guid);

  [[nodiscard]] const WorldEnvironment& Environment() const noexcept;
  [[nodiscard]] bool SetEnvironment(const WorldEnvironment& environment);

  [[nodiscard]] EntityId CreateEntity(std::string_view name, EntityId parent = kNoEntity);
  [[nodiscard]] bool InsertEntity(EntityId id, std::string name, EntityId parent,
                                  const Transform& transform);
  [[nodiscard]] bool DestroyEntity(EntityId id);
  [[nodiscard]] bool SetParent(EntityId id, EntityId parent);
  [[nodiscard]] bool Rename(EntityId id, std::string_view name);
  [[nodiscard]] bool SetTransform(EntityId id, const Transform& transform);
  [[nodiscard]] bool AddScript(EntityId id, std::string_view type);
  [[nodiscard]] bool RemoveScript(EntityId id, std::string_view type);

  [[nodiscard]] Entity* Find(EntityId id) noexcept;
  [[nodiscard]] const Entity* Find(EntityId id) const noexcept;
  [[nodiscard]] const std::vector<Entity>& Entities() const noexcept;

  [[nodiscard]] bool IsDescendant(EntityId ancestor, EntityId candidate) const noexcept;
  [[nodiscard]] math::Mat4 WorldMatrix(EntityId id) const noexcept;
  [[nodiscard]] Result<void> Validate() const;

  [[nodiscard]] uint64_t Revision() const noexcept;

private:
  std::vector<Entity> m_entities;
  std::string m_guid;
  WorldEnvironment m_environment;
  EntityId m_nextId = 1;
  uint64_t m_revision = 0;
};

}

#include <algorithm>
#include <caligo/scene/SceneSerialization.h>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

namespace caligo::scene {

namespace {

using Json = nlohmann::ordered_json;

const Json* Member(const Json& object, const char* key) {
  if (!object.is_object()) {
    return nullptr;
  }
  const Json::const_iterator found = object.find(key);
  return found == object.end() ? nullptr : &(*found);
}

bool ReadFloat(const Json& value, float& result) {
  if (!value.is_number()) {
    return false;
  }
  const double number = value.get<double>();
  if (!std::isfinite(number) || number < -std::numeric_limits<float>::max() ||
      number > std::numeric_limits<float>::max()) {
    return false;
  }
  result = static_cast<float>(number);
  return true;
}

bool ReadVec3(const Json& value, math::Vec3& result) {
  if (!value.is_array() || value.size() != 3) {
    return false;
  }
  math::Vec3 parsed{};
  if (!ReadFloat(value[0], parsed.x) || !ReadFloat(value[1], parsed.y) ||
      !ReadFloat(value[2], parsed.z)) {
    return false;
  }
  result = parsed;
  return true;
}

bool ReadQuat(const Json& value, math::Quat& result) {
  if (!value.is_array() || value.size() != 4) {
    return false;
  }
  math::Quat parsed{};
  if (!ReadFloat(value[0], parsed.x) || !ReadFloat(value[1], parsed.y) ||
      !ReadFloat(value[2], parsed.z) || !ReadFloat(value[3], parsed.w)) {
    return false;
  }
  const float lengthSquared =
      parsed.x * parsed.x + parsed.y * parsed.y + parsed.z * parsed.z + parsed.w * parsed.w;
  if (!(lengthSquared > 1.0e-8f)) {
    return false;
  }
  result = math::NormalizeRotation(parsed);
  return true;
}

Result<Transform> ReadTransform(const Json& component, EntityId entityId) {
  Transform transform;
  const Json* position = Member(component, "position");
  const Json* rotation = Member(component, "rotation");
  const Json* scale = Member(component, "scale");
  if (position == nullptr || !ReadVec3(*position, transform.position)) {
    return Error{std::format("entity {} has an invalid transform position", entityId), 10};
  }
  if (rotation == nullptr || !ReadQuat(*rotation, transform.rotation)) {
    return Error{std::format("entity {} has an invalid transform rotation", entityId), 11};
  }
  if (scale == nullptr || !ReadVec3(*scale, transform.scale)) {
    return Error{std::format("entity {} has an invalid transform scale", entityId), 12};
  }
  return transform;
}

Result<std::pair<EntityId, EntityId>> ReadIdentity(const Json& entity) {
  const Json* idValue = Member(entity, "id");
  if (idValue == nullptr || !idValue->is_number_unsigned()) {
    return Error{"scene entity id must be an unsigned integer", 20};
  }
  const uint64_t id = idValue->get<uint64_t>();
  if (id == kNoEntity) {
    return Error{"scene entity id zero is invalid", 21};
  }
  const Json* parentValue = Member(entity, "parent");
  if (parentValue == nullptr || !parentValue->is_number_unsigned()) {
    return Error{std::format("entity {} parent must be an unsigned integer", id), 22};
  }
  return std::pair<EntityId, EntityId>{id, parentValue->get<uint64_t>()};
}

Result<WorldEnvironment> ReadEnvironment(const Json& root) {
  const Json* environment = Member(root, "environment");
  if (environment == nullptr || !environment->is_object()) {
    return Error{"scene environment must be an object", 50};
  }
  const Json* hours = Member(*environment, "hours");
  WorldEnvironment parsed;
  if (hours == nullptr || !ReadFloat(*hours, parsed.skyHours) || parsed.skyHours < 0.0f ||
      parsed.skyHours > 24.0f) {
    return Error{"scene environment hours must be between 0 and 24", 51};
  }
  return parsed;
}

Result<Terrain> ReadTerrain(const Json& component, EntityId id) {
  const Json* resolutionValue = Member(component, "resolution");
  if (resolutionValue == nullptr || !resolutionValue->is_number_unsigned()) {
    return Error{std::format("entity {} terrain resolution must be an unsigned integer", id), 53};
  }
  const uint64_t resolution = resolutionValue->get<uint64_t>();
  if (resolution < kTerrainMinResolution || resolution > kTerrainMaxResolution) {
    return Error{std::format("entity {} terrain resolution is out of range", id), 54};
  }
  Terrain terrain;
  terrain.resolution = static_cast<uint32_t>(resolution);
  const Json* extentValue = Member(component, "extent");
  if (extentValue == nullptr || !ReadFloat(*extentValue, terrain.extent)) {
    return Error{std::format("entity {} terrain extent is invalid", id), 55};
  }

  const size_t sampleCount = static_cast<size_t>(terrain.resolution) * terrain.resolution;
  const Json* heights = Member(component, "heights");
  if (heights == nullptr || !heights->is_array() || heights->size() != sampleCount) {
    return Error{std::format("entity {} terrain heights count is invalid", id), 56};
  }
  terrain.heights.resize(sampleCount);
  for (size_t sample = 0; sample < sampleCount; ++sample) {
    if (!ReadFloat((*heights)[sample], terrain.heights[sample])) {
      return Error{std::format("entity {} terrain has an invalid height", id), 57};
    }
  }

  const Json* layers = Member(component, "layers");
  if (layers == nullptr || !layers->is_array() || layers->size() != sampleCount * kTerrainLayers) {
    return Error{std::format("entity {} terrain layer weights count is invalid", id), 58};
  }
  terrain.layerWeights.resize(sampleCount * kTerrainLayers);
  for (size_t index = 0; index < terrain.layerWeights.size(); ++index) {
    if (!ReadFloat((*layers)[index], terrain.layerWeights[index])) {
      return Error{std::format("entity {} terrain has an invalid layer weight", id), 59};
    }
  }

  const Json* colors = Member(component, "layerColors");
  if (colors == nullptr || !colors->is_array() || colors->size() != kTerrainLayers) {
    return Error{std::format("entity {} terrain layer colors count is invalid", id), 60};
  }
  for (uint32_t layer = 0; layer < kTerrainLayers; ++layer) {
    if (!ReadVec3((*colors)[layer], terrain.layerColors[layer])) {
      return Error{std::format("entity {} terrain has an invalid layer color", id), 61};
    }
  }

  if (!IsValidTerrain(terrain)) {
    return Error{std::format("entity {} terrain is invalid", id), 62};
  }
  return terrain;
}

Result<std::string> ReadName(const Json& entity, EntityId id) {
  const Json* name = Member(entity, "name");
  if (name == nullptr || !name->is_string()) {
    return Error{std::format("entity {} name must be a string", id), 23};
  }
  return name->get<std::string>();
}

struct EntityComponents {
  Transform transform;
  std::vector<std::string> scripts;
  std::optional<Terrain> terrain;
};

Result<EntityComponents> ReadComponents(const Json& entity, EntityId id) {
  const Json* components = Member(entity, "components");
  if (components == nullptr || !components->is_array()) {
    return Error{std::format("entity {} components must be an array", id), 24};
  }
  bool hasTransform = false;
  EntityComponents parsed;
  for (const Json& component : *components) {
    const Json* type = Member(component, "type");
    if (type == nullptr || !type->is_string()) {
      return Error{std::format("entity {} has a component without a type", id), 25};
    }
    const std::string typeName = type->get<std::string>();
    if (typeName == "caligo.transform") {
      if (hasTransform) {
        return Error{std::format("entity {} has more than one transform", id), 27};
      }
      Result<Transform> transform = ReadTransform(component, id);
      if (!transform) {
        return transform.GetError();
      }
      parsed.transform = transform.Value();
      hasTransform = true;
      continue;
    }
    if (typeName == "caligo.script") {
      const Json* scriptType = Member(component, "class");
      if (scriptType == nullptr || !scriptType->is_string()) {
        return Error{std::format("entity {} script class must be a string", id), 29};
      }
      const std::string className = scriptType->get<std::string>();
      if (!IsValidScriptType(className)) {
        return Error{std::format("entity {} has an invalid script class '{}'", id, className), 31};
      }
      if (parsed.scripts.size() >= kMaxScriptsPerEntity) {
        return Error{std::format("entity {} has too many script components", id), 32};
      }
      const bool duplicate =
          std::any_of(parsed.scripts.begin(), parsed.scripts.end(),
                      [&className](const std::string& existing) { return existing == className; });
      if (duplicate) {
        return Error{std::format("entity {} repeats the script class '{}'", id, className), 33};
      }
      parsed.scripts.push_back(className);
      continue;
    }
    if (typeName == "caligo.terrain") {
      if (parsed.terrain.has_value()) {
        return Error{std::format("entity {} has more than one terrain", id), 63};
      }
      Result<Terrain> terrain = ReadTerrain(component, id);
      if (!terrain) {
        return terrain.GetError();
      }
      parsed.terrain = std::move(terrain).Value();
      continue;
    }
    return Error{std::format("entity {} uses unsupported component {}", id, typeName), 26};
  }
  if (!hasTransform) {
    return Error{std::format("entity {} is missing the transform component", id), 28};
  }
  return parsed;
}

Result<Entity> ReadEntity(const Json& value) {
  if (!value.is_object()) {
    return Error{"scene entity must be an object", 30};
  }
  Result<std::pair<EntityId, EntityId>> identity = ReadIdentity(value);
  if (!identity) {
    return identity.GetError();
  }
  const EntityId id = identity.Value().first;
  const EntityId parent = identity.Value().second;
  Result<std::string> name = ReadName(value, id);
  if (!name) {
    return name.GetError();
  }
  Result<EntityComponents> components = ReadComponents(value, id);
  if (!components) {
    return components.GetError();
  }

  Entity entity;
  entity.id = id;
  entity.parent = parent;
  entity.name = std::move(name).Value();
  entity.transform = components.Value().transform;
  entity.scripts.reserve(components.Value().scripts.size());
  for (std::string& script : components.Value().scripts) {
    ScriptComponent component;
    component.type = std::move(script);
    entity.scripts.push_back(std::move(component));
  }
  if (components.Value().terrain.has_value()) {
    entity.terrain = std::move(components.Value().terrain);
  }
  return entity;
}

}

Result<std::string> SaveSceneToString(const Scene& scene) {
  const Result<void> valid = scene.Validate();
  if (!valid) {
    return valid.GetError();
  }

  Json root = Json::object();
  root["format"] = "caligo.scene";
  root["version"] = 3;
  root["guid"] = scene.Guid();
  root["environment"] = Json::object();
  root["environment"]["hours"] = scene.Environment().skyHours;
  root["entities"] = Json::array();

  std::vector<const Entity*> sorted;
  sorted.reserve(scene.Entities().size());
  for (const Entity& entity : scene.Entities()) {
    sorted.push_back(&entity);
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const Entity* left, const Entity* right) { return left->id < right->id; });

  for (const Entity* entity : sorted) {
    Json transform = Json::object();
    transform["type"] = "caligo.transform";
    transform["position"] = Json::array(
        {entity->transform.position.x, entity->transform.position.y, entity->transform.position.z});
    transform["rotation"] =
        Json::array({entity->transform.rotation.x, entity->transform.rotation.y,
                     entity->transform.rotation.z, entity->transform.rotation.w});
    transform["scale"] = Json::array(
        {entity->transform.scale.x, entity->transform.scale.y, entity->transform.scale.z});

    Json components = Json::array({std::move(transform)});
    for (const ScriptComponent& script : entity->scripts) {
      Json component = Json::object();
      component["type"] = "caligo.script";
      component["class"] = script.type;
      components.push_back(std::move(component));
    }
    if (entity->terrain.has_value()) {
      const Terrain& terrain = entity->terrain.value();
      Json component = Json::object();
      component["type"] = "caligo.terrain";
      component["resolution"] = terrain.resolution;
      component["extent"] = terrain.extent;
      Json heights = Json::array();
      for (const float height : terrain.heights) {
        heights.push_back(height);
      }
      component["heights"] = std::move(heights);
      Json weights = Json::array();
      for (const float weight : terrain.layerWeights) {
        weights.push_back(weight);
      }
      component["layers"] = std::move(weights);
      Json colors = Json::array();
      for (const math::Vec3& color : terrain.layerColors) {
        colors.push_back(Json::array({color.x, color.y, color.z}));
      }
      component["layerColors"] = std::move(colors);
      components.push_back(std::move(component));
    }

    Json serialized = Json::object();
    serialized["id"] = entity->id;
    serialized["name"] = entity->name;
    serialized["parent"] = entity->parent;
    serialized["components"] = std::move(components);
    root["entities"].push_back(std::move(serialized));
  }

  return root.dump(2, ' ', false, Json::error_handler_t::replace) + "\n";
}

Result<Scene> LoadSceneFromString(std::string_view text) {
  Json root = Json::parse(text.begin(), text.end(), nullptr, false);
  if (root.is_discarded()) {
    return Error{"scene is not valid JSON", 40};
  }
  if (!root.is_object()) {
    return Error{"scene root must be an object", 41};
  }

  const Json* format = Member(root, "format");
  if (format == nullptr || !format->is_string() || format->get<std::string>() != "caligo.scene") {
    return Error{"scene format must be caligo.scene", 42};
  }

  const Json* version = Member(root, "version");
  if (version == nullptr || (!version->is_number_integer() && !version->is_number_unsigned())) {
    return Error{"scene version must be an integer", 43};
  }
  const double versionNumber = version->get<double>();
  if (versionNumber != 1.0 && versionNumber != 2.0 && versionNumber != 3.0) {
    return Error{"scene version is not supported", 44};
  }

  const Json* guid = Member(root, "guid");
  if (guid == nullptr || !guid->is_string() || !IsValidGuid(guid->get<std::string>())) {
    return Error{"scene GUID is invalid", 45};
  }

  const Json* entities = Member(root, "entities");
  if (entities == nullptr || !entities->is_array()) {
    return Error{"scene entities must be an array", 46};
  }

  Scene scene;
  if (!scene.SetGuid(guid->get<std::string>())) {
    return Error{"scene GUID is invalid", 47};
  }
  if (versionNumber >= 3.0) {
    const Result<WorldEnvironment> environment = ReadEnvironment(root);
    if (!environment) {
      return environment.GetError();
    }
    if (!scene.SetEnvironment(environment.Value())) {
      return Error{"scene environment could not be applied", 52};
    }
  }

  for (const Json& value : *entities) {
    Result<Entity> entity = ReadEntity(value);
    if (!entity) {
      return entity.GetError();
    }
    const Entity& parsed = entity.Value();
    if (!scene.InsertEntity(parsed.id, parsed.name, parsed.parent, parsed.transform)) {
      return Error{std::format("entity {} could not be inserted; ids must be unique", parsed.id),
                   48};
    }
    for (const ScriptComponent& script : parsed.scripts) {
      if (!scene.AddScript(parsed.id, script.type)) {
        return Error{
            std::format("entity {} script '{}' could not be attached", parsed.id, script.type), 49};
      }
    }
    if (parsed.terrain.has_value() && !scene.SetTerrain(parsed.id, parsed.terrain.value())) {
      return Error{std::format("entity {} terrain could not be applied", parsed.id), 64};
    }
  }

  const Result<void> valid = scene.Validate();
  if (!valid) {
    return valid.GetError();
  }
  return scene;
}

}

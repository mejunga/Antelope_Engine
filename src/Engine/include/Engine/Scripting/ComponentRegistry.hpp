#pragma once

#include <entt/entt.hpp>
#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>


namespace Antelope
{
    enum class ScriptFieldType { Float, Int, Bool, Vec3, String, Unknown };

    struct ScriptFieldDescriptor
    {
        std::string Name;
        ScriptFieldType Type;
        size_t Offset;
    };

    struct ComponentDescriptor
    {
        std::string Name;
        std::vector<ScriptFieldDescriptor> Fields;

        std::function<void(entt::registry&, entt::entity)> Add;
        std::function<void(entt::registry&, entt::entity)> Remove;
        std::function<bool(entt::registry&, entt::entity)> Has;
        std::function<void*(entt::registry&, entt::entity)> Get;

        std::function<void(YAML::Emitter&, entt::registry&, entt::entity)> Serialize;
        std::function<void(const YAML::Node&, entt::registry&, entt::entity)> Deserialize;
    };

    class ComponentRegistry
    {
        public:
            static void Register(const ComponentDescriptor& descriptor);
            static void Clear();

            static void Add (entt::registry& registry, entt::entity entity, const std::string& name);
            static void Remove(entt::registry& registry, entt::entity entity, const std::string& name);
            static bool Has (entt::registry& registry, entt::entity entity, const std::string& name);
            static void* Get (entt::registry& registry, entt::entity entity, const std::string& name);

            static void SerializeAll (YAML::Emitter& out, entt::registry& registry, entt::entity entity);
            static void DeserializeAll(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity);

            static bool IsRegistered(const std::string& name);
            static const std::vector<std::string>& GetRegisteredNames();
            static const ComponentDescriptor* GetDescriptor(const std::string& name);

            static entt::meta_ctx& GetContext();

        private:
            static std::unordered_map<std::string, ComponentDescriptor>& Descriptors();
            static std::vector<std::string>& Names();
    };
}
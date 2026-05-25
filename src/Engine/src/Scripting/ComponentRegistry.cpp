#include <Engine/Scripting/ComponentRegistry.hpp>
#include <Engine/Debug/Log.hpp>

namespace Antelope
{
    static entt::meta_ctx s_ScriptContext;

    std::unordered_map<std::string, ComponentDescriptor>& ComponentRegistry::Descriptors()
    {
        static std::unordered_map<std::string, ComponentDescriptor> map;
        return map;
    }

    std::vector<std::string>& ComponentRegistry::Names()
    {
        static std::vector<std::string> names;
        return names;
    }

    void ComponentRegistry::Register(const ComponentDescriptor& descriptor)
    {
        if (!Descriptors().count(descriptor.Name))
        {
            Names().push_back(descriptor.Name);
        }

        Descriptors()[descriptor.Name] = descriptor;
        AE_ENGINE_TRACE("ComponentRegistry: Registered '{0}'.", descriptor.Name);
    }

    void ComponentRegistry::Clear()
    {
        Descriptors().clear();
        Names().clear();
        s_ScriptContext = entt::meta_ctx{};
        AE_ENGINE_TRACE("ComponentRegistry: Cleared.");
    }

    void ComponentRegistry::Add(entt::registry& registry, entt::entity entity, const std::string& name)
    {
        auto it { Descriptors().find(name) };

        if (it == Descriptors().end())
        {
            AE_ENGINE_WARN("ComponentRegistry: Add — '{0}' not registered.", name);
            return;
        }

        it->second.Add(registry, entity);
    }

    void ComponentRegistry::Remove(entt::registry& registry, entt::entity entity, const std::string& name)
    {
        auto it { Descriptors().find(name) };
        if (it != Descriptors().end()) { it->second.Remove(registry, entity); }
    }

    bool ComponentRegistry::Has(entt::registry& registry, entt::entity entity, const std::string& name)
    {
        auto it { Descriptors().find(name) };
        if (it == Descriptors().end()) { return false; }
        return it->second.Has(registry, entity);
    }

    void* ComponentRegistry::Get(entt::registry& registry, entt::entity entity, const std::string& name)
    {
        auto it { Descriptors().find(name) };
        if (it == Descriptors().end()) { return nullptr; }
        return it->second.Get(registry, entity);
    }

    void ComponentRegistry::SerializeAll(YAML::Emitter& out, entt::registry& registry, entt::entity entity)
    {
        for (const auto& name : Names())
        {
            auto& desc { Descriptors().at(name) };

            if (desc.Has(registry, entity))
            {
                desc.Serialize(out, registry, entity);
            }
        }
    }

    void ComponentRegistry::DeserializeAll(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
    {
        for (const auto& name : Names())
        {
            if (auto node { entityNode[name] })
            {
                Descriptors().at(name).Deserialize(node, registry, entity);
            }
        }
    }

    bool ComponentRegistry::IsRegistered(const std::string& name)
    {
        return Descriptors().count(name) > 0;
    }

    const std::vector<std::string>& ComponentRegistry::GetRegisteredNames()
    {
        return Names();
    }

    const ComponentDescriptor* ComponentRegistry::GetDescriptor(const std::string& name)
    {
        auto& descs { Descriptors() };
        auto it { descs.find(name) };
        if (it == descs.end()) { return nullptr; }
        return &it->second;
    }

    entt::meta_ctx& ComponentRegistry::GetContext()
    {
        return s_ScriptContext;
    }
}
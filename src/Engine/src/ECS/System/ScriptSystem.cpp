#include <Engine/ECS/System/ScriptSystem.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Scripting/Script.hpp>
#include <Engine/Scripting/ScriptLibrary.hpp>
#include <Engine/Scripting/ScriptDLLInterface.hpp>
#include <Engine/Scripting/ComponentRegistry.hpp>
#include <Engine/Debug/Log.hpp>


namespace Antelope
{
    std::vector<ScriptSystem::SystemInstance>& ScriptSystem::Systems()
    {
        static std::vector<SystemInstance> systems;
        return systems;
    }

    void ScriptSystem::RegisterComponentsFromDLL()
    {
        auto& lib { ScriptLibrary::Get() };

        if (!lib.IsLoaded()) { return; }

        using GetDescsFn = ComponentDescriptorC*(*)(uint32_t*);
        auto getDescs { reinterpret_cast<GetDescsFn>(lib.GetSymbol("GetComponentDescriptors")) };

        if (!getDescs) { return; }

        uint32_t count { 0 };
        ComponentDescriptorC* cDescs { getDescs(&count) };

        for (uint32_t i { 0 }; i < count; ++i)
        {
            const ComponentDescriptorC& c { cDescs[i] };

            ComponentDescriptor desc;
            desc.Name = c.Name;

            for (uint32_t f { 0 }; f < c.FieldCount; ++f)
            {
                ScriptFieldDescriptor field;
                field.Name = c.Fields[f].Name;
                field.Type = static_cast<ScriptFieldType>(c.Fields[f].Type);
                field.Offset = c.Fields[f].Offset;
                desc.Fields.push_back(field);
            }

            auto addFn { c.Add };
            auto removeFn { c.Remove };
            auto hasFn { c.Has };
            auto getFn { c.Get };
            auto serializeFn { c.Serialize };
            auto deserializeFn { c.Deserialize };

            desc.Add = [addFn] (entt::registry& r, entt::entity e) { addFn(&r, e); };
            desc.Remove = [removeFn] (entt::registry& r, entt::entity e) { removeFn(&r, e); };
            desc.Has = [hasFn] (entt::registry& r, entt::entity e) -> bool { return hasFn(&r, e); };
            desc.Get = [getFn] (entt::registry& r, entt::entity e) -> void* { return getFn(&r, e); };
            desc.Serialize = [serializeFn] (YAML::Emitter& out, entt::registry& r, entt::entity e)      { serializeFn(&out, &r, e); };
            desc.Deserialize = [deserializeFn](const YAML::Node& node, entt::registry& r, entt::entity e)  { deserializeFn(&node, &r, e); };

            ComponentRegistry::Register(desc);
        }
    }

    void ScriptSystem::InstantiateSystems(World& world)
    {
        auto& lib { ScriptLibrary::Get() };
        auto names { lib.GetRegisteredSystems() };

        for (const auto& name : names)
        {
            auto createFn { lib.GetSystemCreateFn(name) };
            auto destroyFn { lib.GetSystemDestroyFn(name) };

            if (!createFn) { AE_ENGINE_WARN("ScriptSystem: No factory for system '{0}'.", name); continue; }

            SystemInstance inst;
            inst.ClassName = name;
            inst.Instance = createFn();
            inst.DestroyFn = destroyFn;
            inst.Instance->OnStart(world);
            Systems().push_back(std::move(inst));
        }
    }

    void ScriptSystem::DestroyAllSystems(World& world)
    {
        for (auto& sys : Systems())
        {
            sys.Instance->OnStop(world);
            if (sys.DestroyFn) { sys.DestroyFn(sys.Instance); }
        }

        Systems().clear();
    }

    void ScriptSystem::DestroyAllScripts(World& world)
    {
        auto& registry { world.GetRegistry() };

        for (auto [entity, sc] : registry.view<ScriptComponent>().each())
        {
            if (!sc.Instance) { continue; }

            sc.Instance->OnDestroy();
            if (sc.DestroyScript) { sc.DestroyScript(sc.Instance); }
            sc.Instance = nullptr;
        }
    }

    void ScriptSystem::LoadDLL(const std::string& dllPath)
    {
        ScriptLibrary::Get().Load(dllPath);
        RegisterComponentsFromDLL();
    }

    void ScriptSystem::OnRuntimeStart(World& world)
    {
        InstantiateSystems(world);

        auto& registry { world.GetRegistry() };
        auto& lib { ScriptLibrary::Get() };

        for (auto [entity, sc] : registry.view<ScriptComponent>().each())
        {
            if (sc.ScriptClassName.empty()) { continue; }

            sc.InstantiateScript = lib.GetScriptCreateFn(sc.ScriptClassName);
            sc.DestroyScript = lib.GetScriptDestroyFn(sc.ScriptClassName);

            if (!sc.InstantiateScript)
            {
                AE_ENGINE_WARN("ScriptSystem: No factory for '{0}'.", sc.ScriptClassName);
                continue;
            }

            sc.Instance = sc.InstantiateScript();
            sc.Instance->m_Entity = Entity{ entity, &world };
            sc.Instance->OnCreate();
        }
    }

    void ScriptSystem::OnUpdate(World& world, float dt)
    {
        for (auto& sys : Systems()) { sys.Instance->OnUpdate(world, dt); }

        auto& registry { world.GetRegistry() };

        for (auto [entity, sc] : registry.view<ScriptComponent>().each())
        {
            if (sc.Instance) { sc.Instance->OnUpdate(dt); }
        }
    }

    void ScriptSystem::OnRuntimeStop(World& world)
    {
        DestroyAllScripts(world);
        DestroyAllSystems(world);
    }

    void ScriptSystem::HotReload(World& world)
    {
        DestroyAllScripts(world);
        DestroyAllSystems(world);
        ComponentRegistry::Clear();

        auto& lib { ScriptLibrary::Get() };
        std::string path { lib.GetPath() };
        lib.Unload();
        lib.Load(path);

        RegisterComponentsFromDLL();

        if (world.IsSimulating())
        {
            InstantiateSystems(world);

            auto& registry { world.GetRegistry() };

            for (auto [entity, sc] : registry.view<ScriptComponent>().each())
            {
                if (sc.ScriptClassName.empty()) { continue; }

                sc.InstantiateScript = lib.GetScriptCreateFn(sc.ScriptClassName);
                sc.DestroyScript = lib.GetScriptDestroyFn(sc.ScriptClassName);

                if (sc.InstantiateScript)
                {
                    sc.Instance = sc.InstantiateScript();
                    sc.Instance->m_Entity = Entity{ entity, &world };
                    sc.Instance->OnCreate();
                }
            }
        }

        AE_ENGINE_INFO("ScriptSystem: Hot-reload complete.");
    }
}
#pragma once

#include <Engine/Scripting/GameSystem.hpp>
#include <Engine/Scripting/ScriptLibrary.hpp>

#include <string>
#include <vector>


namespace Antelope
{
    class World;

    class ScriptSystem
    {
        public:
            static void LoadDLL(const std::string& dllPath);
            static void OnRuntimeStart(World& world);
            static void OnUpdate(World& world, float dt);
            static void OnRuntimeStop(World& world);
            static void HotReload(World& world);

        private:
            struct SystemInstance
            {
                std::string ClassName;
                GameSystem* Instance { nullptr };
                ScriptLibrary::SystemDestroyFn DestroyFn { nullptr };
            };

            static std::vector<SystemInstance>& Systems();

            static void RegisterComponentsFromDLL();
            static void InstantiateSystems(World& world);
            static void DestroyAllSystems(World& world);
            static void DestroyAllScripts(World& world);
    };
}
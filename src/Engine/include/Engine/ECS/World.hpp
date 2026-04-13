#pragma once

#include <Engine/Renderer/Graphics/Model.hpp>

#include <entt/entt.hpp>

#include <string>
#include <memory>


namespace Antelope
{
    class Entity;
    class PhysicsContext;
#ifdef ANTELOPE_EDITOR_MODE
    class EditorCamera;
#endif

    class World
    {
        public:
            World();
            ~World();

            Entity CreateEntity(const std::string& name = std::string());
            void DestroyEntity(Entity entity);
            Entity SpawnModel(const ModelData& modelData, const std::string& rootName);
            Entity SpawnModel(const ModelData& modelData, const std::string& rootName, Entity parentEntity);
            void MarkTransformDirty(Entity entity);
            void MarkHierarchyDirty() { m_HierarchyDirty = true; }

            void OnSimulationStart();
            void OnSimulationStop();
            void StepSimulation(float timeStep);
        #ifdef ANTELOPE_EDITOR_MODE
            void OnUpdateEditor(float timeStep, const EditorCamera& camera);
        #endif
            void OnUpdateRuntime(float timeStep);
            
            inline entt::registry& GetRegistry() { return m_Registry; }
            inline PhysicsContext* GetPhysicsContext() { return m_PhysicsContext.get(); }
            inline bool IsSimulating() const { return m_IsSimulating; }

        private:
            Entity SpawnModelNodeRecursive(const ModelNode& node, const ModelData& modelData, Entity parentEntity);

        private:
            std::unique_ptr<PhysicsContext> m_PhysicsContext;

            entt::registry m_Registry;
            bool m_HierarchyDirty { true };
            bool m_IsSimulating { false }; 

            friend class Entity;
    };
}
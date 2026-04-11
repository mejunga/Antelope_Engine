#pragma once

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

            void OnSimulationStart();
            void OnSimulationStop();
            void StepSimulation(float timeStep);
            inline bool IsSimulating() const { return m_IsSimulating; }

        #ifdef ANTELOPE_EDITOR_MODE
            void OnUpdateEditor(float timeStep, const EditorCamera& camera);
        #endif
            void OnUpdateRuntime(float timeStep);
            
            void MarkTransformDirty(Entity entity);
            void MarkHierarchyDirty() { m_HierarchyDirty = true; }
            
            inline entt::registry& GetRegistry() { return m_Registry; }
            inline PhysicsContext* GetPhysicsContext() { return m_PhysicsContext.get(); }

        private:
            entt::registry m_Registry;
            bool m_HierarchyDirty { true };

            std::unique_ptr<PhysicsContext> m_PhysicsContext;
            bool m_IsSimulating { false }; 

            friend class Entity;
    };
}
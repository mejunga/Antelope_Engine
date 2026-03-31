#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <string>


namespace Antelope
{
    class Entity;
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
        #ifdef ANTELOPE_EDITOR_MODE
            void OnUpdateEditor(float timeStep, const EditorCamera& camera);
        #endif
            void OnUpdateRuntime(float timeStep);
            void MarkTransformDirty(Entity entity);

            inline entt::registry& GetRegistry() { return m_Registry; }

        private:
            entt::registry m_Registry;
            
            friend class Entity;
    };
}
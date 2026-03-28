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
            void OnUpdateEditor(float deltaTime, EditorCamera& camera);
        #endif
            void OnUpdateRuntime(float deltaTime);

            entt::registry& GetRegistry() { return m_Registry; }

        private:
            void UpdateTransforms();
            void UpdateEntityTransform(entt::entity startEntity, const glm::mat4& parentMatrix, bool forceUpdate);

        private:
            entt::registry m_Registry;
            
            friend class Entity;
    };
}
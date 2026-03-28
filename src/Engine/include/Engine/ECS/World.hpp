#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <string>


namespace Antelope
{
    class Entity;
    class EditorCamera;

    class World
    {
        public:
            World();
            ~World();

            Entity CreateEntity(const std::string& name = std::string());
            void DestroyEntity(Entity entity);

            void OnUpdateRuntime(float deltaTime);
            void OnUpdateEditor(float deltaTime, EditorCamera& camera);

            entt::registry& GetRegistry() { return m_Registry; }

        private:
            void UpdateTransforms();
            void UpdateEntityTransform(entt::entity startEntity, const glm::mat4& parentMatrix, bool forceUpdate);

        private:
            entt::registry m_Registry;
            
            friend class Entity;
    };
}
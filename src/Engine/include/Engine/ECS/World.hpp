#pragma once

#include <entt/entt.hpp>
#include <string>


namespace Antelope {

    class Entity;
    class EditorCamera;

    class World {
        public:
            World();
            ~World();

            Entity CreateEntity(const std::string& name = std::string());
            void DestroyEntity(Entity entity);

            void OnUpdateRuntime(float deltaTime);
            void OnUpdateEditor(float deltaTime, EditorCamera& camera);

            entt::registry& GetRegistry() { return m_Registry; }

        private:
            entt::registry m_Registry;
            
            friend class Entity;
    };
}
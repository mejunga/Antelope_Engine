#pragma once

#include <entt/entt.hpp>
#include <string>


namespace Antelope {

    class Entity;

    class World {
    public:
        World();
        ~World();

        Entity CreateEntity(const std::string& name = std::string());
        void DestroyEntity(Entity entity);

        entt::registry& GetRegistry() { return m_Registry; }

    private:
        entt::registry m_Registry;
        
        friend class Entity;
    };
}
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>

namespace Antelope {

    World::World() {}

    World::~World() {}

    Entity World::CreateEntity(const std::string& name) 
    {
        entt::entity handle = m_Registry.create();
        Entity entity = { handle, this };

        entity.AddComponent<TransformComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;

        return entity;
    }

    void World::DestroyEntity(Entity entity) 
    {
        m_Registry.destroy(entity);
    }

}
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/ECS/System/RenderSystem.hpp>
#include <Engine/ECS/System/TransformSystem.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>

#include <vector>


namespace Antelope
{
    World::World()
    {
        m_Registry.on_construct<MeshComponent>().connect<[](entt::registry& reg, entt::entity e)
        {
            reg.emplace_or_replace<NormalMatrixComponent>(e);
        }>();
    }

    World::~World() {}

    Entity World::CreateEntity(const std::string& name)
    {
        entt::entity handle { m_Registry.create() };
        Entity entity { handle, this };

        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        auto& tag { entity.AddComponent<TagComponent>() };
        tag.Tag = name.empty() ? "Entity" : name;
        MarkTransformDirty(entity);
        MarkHierarchyDirty();
        return entity;
    }

    void World::DestroyEntity(Entity entity)
    {
        m_Registry.destroy(entity);
        MarkHierarchyDirty();
    }

#ifdef ANTELOPE_EDITOR_MODE
    void World::OnUpdateEditor(float timeStep, const EditorCamera& camera)
    {
        if (m_HierarchyDirty)
        {
            TransformSystem::SortHierarchy(*this);
            m_HierarchyDirty = false;
        }

        TransformSystem::OnUpdate(*this);
        auto renderer { Application::Get().GetRenderer() };
        RenderSystem::RenderEditor(*this, renderer, camera);
    }
#endif

    void World::OnUpdateRuntime(float timeStep)
    {        
        if (m_HierarchyDirty)
        {
            TransformSystem::SortHierarchy(*this);
            m_HierarchyDirty = false;
        }

        TransformSystem::OnUpdate(*this);
        auto renderer { Application::Get().GetRenderer() };
        RenderSystem::RenderRuntime(*this, renderer);
    }

    void World::MarkTransformDirty(Entity entity)
    {
        m_Registry.emplace_or_replace<DirtyTransform>(entity);
    }
}
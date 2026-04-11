#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/ECS/System/RenderSystem.hpp>
#include <Engine/ECS/System/TransformSystem.hpp>
#include <Engine/ECS/System/PhysicsSystem.hpp>
#include <Engine/Physics/PhysicsContext.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>

#include <vector>


namespace Antelope
{
    World::World()
    {
        m_PhysicsContext = std::make_unique<PhysicsContext>();

        m_Registry.on_construct<MeshComponent>().connect<[](entt::registry& reg, entt::entity e)
        {
            reg.emplace_or_replace<NormalMatrixComponent>(e);
        }>();
    }

    World::~World() 
    {
        if (m_IsSimulating)
        {
            OnSimulationStop();
        }
    }

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
        if (m_IsSimulating && m_Registry.all_of<RigidBodyComponent>(entity))
        {
            auto& rb { m_Registry.get<RigidBodyComponent>(entity) };
            
            if (rb.RuntimeBodyID != 0xFFFFFFFF)
            {
                auto& bodyInterface { m_PhysicsContext->GetBodyInterface() };
                JPH::BodyID bodyID(rb.RuntimeBodyID);
                
                bodyInterface.RemoveBody(bodyID);
                bodyInterface.DestroyBody(bodyID);
            }
        }

        m_Registry.destroy(entity);
        MarkHierarchyDirty();
    }

    void World::OnSimulationStart()
    {
        if (m_IsSimulating) { return; }

        PhysicsSystem::OnRuntimeStart(*this, *m_PhysicsContext);
        m_PhysicsContext->OptimizeBroadPhase();
        
        m_IsSimulating = true;
        AE_ENGINE_INFO("World Simulation Started.");
    }

    void World::OnSimulationStop()
    {
        if (!m_IsSimulating) { return; }

        PhysicsSystem::OnRuntimeStop(*this, *m_PhysicsContext);
        
        m_IsSimulating = false;
        AE_ENGINE_INFO("World Simulation Stopped.");
    }

    void World::StepSimulation(float timeStep)
    {
        if (m_IsSimulating)
        {
            PhysicsSystem::OnUpdate(*this, *m_PhysicsContext, timeStep);
        }
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
        if (!m_IsSimulating) { OnSimulationStart(); }

        StepSimulation(timeStep);

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
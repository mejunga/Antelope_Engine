#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Renderer/Graphics/Camera.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp> 

#include <vector>

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

    void World::OnUpdateRuntime(float deltaTime)
    {
        // 31
    }

    void World::OnUpdateEditor(float deltaTime, EditorCamera& camera)
    {
        auto renderer = Application::Get().GetRenderer();
        if (!renderer) return;

        UniformBufferObject cameraUBO {};
        cameraUBO.view = camera.GetViewMatrix();
        
        auto extent = Application::Get().GetSwapChain()->GetExtent();
        cameraUBO.proj = camera.GetProjectionMatrix(
            static_cast<float>(extent.width), 
            static_cast<float>(extent.height)
        );

        std::vector<RenderCommand> renderList;
        auto view = m_Registry.view<TransformComponent, MeshComponent>();

        for (auto [entityID, transform, meshComponent] : view.each()) 
        {
            renderList.push_back({ transform.GetTransform(), meshComponent.Handle });
        }

        renderer->DrawFrame(cameraUBO, renderList);
    }
}
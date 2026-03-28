#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Renderer/Graphics/Camera.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp> 

#include <vector>

namespace Antelope
{
    World::World() {}
    World::~World() {}

    Entity World::CreateEntity(const std::string& name) 
    {
        entt::entity handle { m_Registry.create() };
        Entity entity { handle, this };

        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        auto& tag { entity.AddComponent<TagComponent>() };
        tag.Tag = name.empty() ? "Entity" : name;

        return entity;
    }

    void World::DestroyEntity(Entity entity) 
    {
        m_Registry.destroy(entity);
    }

    void World::OnUpdateRuntime(float deltaTime)
    {

    }

    void World::OnUpdateEditor(float deltaTime, EditorCamera& camera)
    {
        auto renderer { Application::Get().GetRenderer() };
        if (!renderer) { return; }

        UpdateTransforms();

        UniformBufferObject cameraUBO {};
        cameraUBO.view = camera.GetViewMatrix();
        
        auto extent { Application::Get().GetSwapChain()->GetExtent() };
        cameraUBO.proj = camera.GetProjectionMatrix(
            static_cast<float>(extent.width), 
            static_cast<float>(extent.height)
        );

        std::vector<RenderCommand> renderList;
        auto view { m_Registry.view<TransformComponent, MeshComponent>() };
        renderList.reserve(view.size_hint());

        for (auto [entityID, transform, meshComponent] : view.each()) 
        {
            renderList.push_back({ transform.WorldMatrix, transform.NormalMatrix, meshComponent.Handle });
        }

        renderer->DrawFrame(cameraUBO, renderList);
    }

    void World::UpdateEntityTransform(entt::entity startEntity, const glm::mat4& parentMatrix, bool forceUpdate)
    {
        entt::entity current { startEntity };
        
        while (current != entt::null)
        {
            auto& transform { m_Registry.get<TransformComponent>(current) };
            auto& rel { m_Registry.get<RelationshipComponent>(current) };
            bool needsUpdate = forceUpdate || transform.IsDirty;

            if (needsUpdate)
            {
                transform.WorldMatrix = parentMatrix * transform.GetLocalTransform();
                transform.NormalMatrix = glm::transpose(glm::inverse(transform.WorldMatrix));
                transform.IsDirty = false;
            }

            if (rel.FirstChild != entt::null)
            {
                UpdateEntityTransform(rel.FirstChild, transform.WorldMatrix, needsUpdate);
            }

            current = rel.NextSibling;
        }
    }

    void World::UpdateTransforms()
    {
        auto view { m_Registry.view<TransformComponent, RelationshipComponent>() };

        for (auto [entity, transform, rel] : view.each())
        {
            if (rel.Parent == entt::null)
            {
                if (transform.IsDirty)
                {
                    transform.WorldMatrix = transform.GetLocalTransform();
                    transform.NormalMatrix = glm::transpose(glm::inverse(transform.WorldMatrix));
                    transform.IsDirty = false;

                    if (rel.FirstChild != entt::null)
                    {
                        UpdateEntityTransform(rel.FirstChild, transform.WorldMatrix, true);
                    }
                }
                else
                {
                    if (rel.FirstChild != entt::null)
                    {
                        UpdateEntityTransform(rel.FirstChild, transform.WorldMatrix, false);
                    }
                }
            }
        }
    }
}
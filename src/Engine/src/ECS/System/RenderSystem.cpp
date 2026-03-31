#include <Engine/ECS/System/RenderSystem.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp>


namespace Antelope
{
#ifdef ANTELOPE_EDITOR_MODE
    void RenderSystem::RenderEditor(World& world, std::shared_ptr<Renderer> renderer, const EditorCamera& camera)
    {
        if (!renderer) { return; }

        UniformBufferObject cameraUBO {};
        cameraUBO.view = camera.GetViewMatrix();
        
        auto renderExtent { renderer->GetRenderTexture()->GetExtent() };
        cameraUBO.proj = camera.GetProjectionMatrix(
            static_cast<float>(renderExtent.width), 
            static_cast<float>(renderExtent.height)
        );

        std::vector<RenderCommand> renderList;
        auto& registry { world.GetRegistry() };
        auto view { registry.view<TransformComponent, MeshComponent>() };
        renderList.reserve(view.size_hint());

        for (auto [entityID, transform, meshComponent] : view.each()) 
        {
            renderList.push_back({ 
                transform.WorldMatrix, 
                transform.NormalMatrix, 
                meshComponent.Handle,
                static_cast<uint32_t>(entityID)
            });
        }

        renderer->DrawFrame(cameraUBO, renderList);
    }
#endif

    void RenderSystem::RenderRuntime(World& world, std::shared_ptr<Renderer> renderer)
    {
        if (!renderer) { return; }

        glm::mat4 viewMatrix { 1.0f };
        glm::mat4 projMatrix { 1.0f };
        bool cameraFound { false };

        auto& registry { world.GetRegistry() };
        auto cameraView { registry.view<TransformComponent, CameraComponent>() };
        
        for (auto [entity, transform, camera] : cameraView.each())
        {
            if (camera.IsPrimary)
            {
                viewMatrix = glm::inverse(transform.WorldMatrix);
                auto extent { Application::Get().GetSwapChain()->GetExtent() };
                camera.CalculateProjection(static_cast<float>(extent.width) / static_cast<float>(extent.height));
                projMatrix = camera.Projection;

                cameraFound = true;
                break;
            }
        }

        if (!cameraFound) 
        {
            AE_ENGINE_WARN("No primary camera found in the scene!");
            return;
        }

        UniformBufferObject cameraUBO {};
        cameraUBO.view = viewMatrix;
        cameraUBO.proj = projMatrix;

        std::vector<RenderCommand> renderList;
        auto meshView { registry.view<TransformComponent, MeshComponent>() };
        renderList.reserve(meshView.size_hint());

        for (auto [entityID, transform, meshComponent] : meshView.each()) 
        {
            renderList.push_back({ 
                transform.WorldMatrix, 
                transform.NormalMatrix, 
                meshComponent.Handle
            });
        }

        renderer->DrawFrame(cameraUBO, renderList);
    }
}
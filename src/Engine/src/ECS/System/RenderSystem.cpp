#include <Engine/ECS/System/RenderSystem.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp>
#include <Engine/Renderer/Graphics/RenderCommand.hpp>
#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#endif


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

        static std::vector<RenderCommand> renderList;
        renderList.clear();

        auto& registry { world.GetRegistry() };
        auto view { registry.view<TransformComponent, MeshComponent>(entt::exclude<DisabledComponent>) };
        renderList.reserve(view.size_hint());

        for (auto [entityID, transform, meshComponent] : view.each()) 
        {
            RenderCommand cmd{};
            cmd.transform = transform.WorldMatrix;
            cmd.normalMatrix = transform.NormalMatrix;
            cmd.mesh = meshComponent.Handle;
            cmd.entityID = static_cast<uint32_t>(entityID);

            renderList.push_back(cmd);
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

        static std::vector<RenderCommand> renderList;
        renderList.clear();

        auto meshView { registry.view<TransformComponent, MeshComponent>(entt::exclude<DisabledComponent>) };
        renderList.reserve(meshView.size_hint());

        for (auto [entityID, transform, meshComponent] : meshView.each()) 
        {
            RenderCommand cmd{};
            cmd.transform = transform.WorldMatrix;
            cmd.normalMatrix = transform.NormalMatrix;
            cmd.mesh = meshComponent.Handle;

            renderList.push_back(cmd);
        }

        renderer->DrawFrame(cameraUBO, renderList);
    }
}
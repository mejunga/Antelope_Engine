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
    static std::vector<RenderCommand> s_RenderList;

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

        s_RenderList.clear();
        auto& registry { world.GetRegistry() };
        auto view { registry.view<WorldMatrixComponent, MeshComponent, NormalMatrixComponent>(entt::exclude<DisabledComponent>) };
        s_RenderList.reserve(view.size_hint());

        for (auto [entityID, worldMat, meshComponent, normalMat] : view.each())
        {
            RenderCommand cmd {};
            cmd.transform = worldMat.Matrix;
            cmd.normalMatrix = normalMat.Matrix;
            cmd.mesh = meshComponent.Handle;

            if (auto* mat { registry.try_get<MaterialComponent>(entityID) })
            {
                cmd.materialIndex = mat->MaterialIndex;
            }

            cmd.entityID = static_cast<uint32_t>(entityID);

            s_RenderList.push_back(cmd);
        }

        renderer->DrawFrame(cameraUBO, s_RenderList);
    }
#endif

    void RenderSystem::RenderRuntime(World& world, std::shared_ptr<Renderer> renderer)
    {
        if (!renderer) { return; }

        glm::mat4 viewMatrix { 1.0f };
        glm::mat4 projMatrix { 1.0f };

        auto& registry { world.GetRegistry() };
        entt::entity camEntity { world.GetPrimaryCamera() };
        if (camEntity == entt::null)
        {
            AE_ENGINE_WARN("No primary camera found in the scene!");
            return;
        }

        const auto& [camWorld, camera] { registry.get<WorldMatrixComponent, CameraComponent>(camEntity) };
        viewMatrix = glm::inverse(camWorld.Matrix);
        auto extent { Application::Get().GetSwapChain()->GetExtent() };
        camera.CalculateProjection(static_cast<float>(extent.width) / static_cast<float>(extent.height));
        projMatrix = camera.Projection;

        UniformBufferObject cameraUBO {};
        cameraUBO.view = viewMatrix;
        cameraUBO.proj = projMatrix;

        s_RenderList.clear();
        auto meshView { registry.view<WorldMatrixComponent, MeshComponent, NormalMatrixComponent>(entt::exclude<DisabledComponent>) };
        s_RenderList.reserve(meshView.size_hint());

        for (auto [entityID, worldMat, meshComponent, normalMat] : meshView.each())
        {
            RenderCommand cmd {};
            cmd.transform = worldMat.Matrix;
            cmd.normalMatrix = normalMat.Matrix;
            cmd.mesh = meshComponent.Handle;

            if (auto* mat { registry.try_get<MaterialComponent>(entityID) })
            {
                cmd.materialIndex = mat->MaterialIndex;
            }

            s_RenderList.push_back(cmd);
        }

        renderer->DrawFrame(cameraUBO, s_RenderList);
    }
}
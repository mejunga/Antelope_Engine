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

#include <chrono>


namespace Antelope
{
    static std::vector<RenderCommand> s_RenderList;

    static void GatherLights(entt::registry& registry, UniformBufferObject& ubo)
    {
        auto dirLightView { registry.view<WorldMatrixComponent, DirectionalLightComponent>(entt::exclude<DisabledComponent>) };
        auto it { dirLightView.begin() };
        bool sunFound { false };

        for (auto [entity, worldMat, lightComp] : dirLightView.each())
        {
            glm::vec3 forward { -glm::normalize(glm::vec3(worldMat.Matrix[2])) };
            ubo.sunDirection = glm::vec4(forward, 1.0f);
            ubo.sunColor = glm::vec4(lightComp.Color, lightComp.Intensity);
            sunFound = true;
            break;
        }

        if (!sunFound) { ubo.sunDirection.w = 0.0f; }

        auto pointView { registry.view<WorldMatrixComponent, PointLightComponent>(entt::exclude<DisabledComponent>) };
        ubo.pointLightCount = 0;

        for (auto [entity, worldMat, lightComp] : pointView.each())
        {
            if (ubo.pointLightCount >= MAX_POINT_LIGHTS) { break; }
            glm::vec3 pos { glm::vec3(worldMat.Matrix[3]) };
            ubo.pointLights[ubo.pointLightCount].positionAndRadius = glm::vec4(pos, lightComp.Radius);
            ubo.pointLights[ubo.pointLightCount].colorAndIntensity = glm::vec4(lightComp.Color, lightComp.Intensity);
            ubo.pointLightCount++;
        }

        auto spotView { registry.view<WorldMatrixComponent, SpotLightComponent>(entt::exclude<DisabledComponent>) };
        ubo.spotLightCount = 0;

        for (auto [entity, worldMat, lightComp] : spotView.each())
        {
            if (ubo.spotLightCount >= MAX_SPOT_LIGHTS) { break; }
            
            glm::vec3 pos { glm::vec3(worldMat.Matrix[3]) };
            glm::vec3 forward { -glm::normalize(glm::vec3(worldMat.Matrix[2])) };
            ubo.spotLights[ubo.spotLightCount].positionAndRadius = glm::vec4(pos, lightComp.Radius);
            ubo.spotLights[ubo.spotLightCount].directionAndCutOff = glm::vec4(forward, lightComp.InnerCutOff);
            ubo.spotLights[ubo.spotLightCount].colorAndIntensity = glm::vec4(lightComp.Color, lightComp.Intensity);
            ubo.spotLights[ubo.spotLightCount].outerCutOffAndPad.x = lightComp.OuterCutOff;
            ubo.spotLightCount++;
        }

        auto ambientView { registry.view<AmbientComponent>(entt::exclude<DisabledComponent>) };
        
        if (ambientView.begin() != ambientView.end())
        {
            auto entity { *ambientView.begin() };
            const auto& ambient { ambientView.get<AmbientComponent>(entity) };

            ubo.skyColorDayAndStar = glm::vec4(ambient.SkyColorDay, ambient.StarIntensity);
            ubo.horizonColorDay = glm::vec4(ambient.HorizonColorDay, 1.0f);
            ubo.skyColorNight = glm::vec4(ambient.SkyColorNight, 1.0f);
            ubo.horizonColorNight = glm::vec4(ambient.HorizonColorNight, 1.0f);
            ubo.groundColor = glm::vec4(ambient.GroundColor, 1.0f);
            ubo.ambientEnabled = 1.0f;
        }
        else
        {
            ubo.ambientEnabled = 0.0f;
        }
    }

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

        glm::mat4 invView { glm::inverse(cameraUBO.view) };
        cameraUBO.cameraPos = glm::vec4(invView[3][0], invView[3][1], invView[3][2], 1.0f);
        static auto s_StartTime { std::chrono::high_resolution_clock::now() };
        cameraUBO.time = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - s_StartTime).count();

        GatherLights(world.GetRegistry(), cameraUBO);

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

        glm::mat4 invView = glm::inverse(cameraUBO.view);
        cameraUBO.cameraPos = glm::vec4(invView[3][0], invView[3][1], invView[3][2], 1.0f);
        static auto s_StartTime { std::chrono::high_resolution_clock::now() };
        cameraUBO.time = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - s_StartTime).count();

        GatherLights(registry, cameraUBO);

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
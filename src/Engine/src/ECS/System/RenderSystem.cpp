#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <Engine/ECS/System/RenderSystem.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp>
#include <Engine/Renderer/Graphics/RenderCommand.hpp>
#include <Engine/Core/JobSystem.hpp>
#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#endif

#include <chrono>


namespace Antelope
{
    namespace
    {
        struct RenderEntry
        {
            entt::entity entity;
            const WorldMatrixComponent* worldMat;
            const MeshComponent* meshComp;
        };
    }

    static void GatherLights(entt::registry& registry, UniformBufferObject& ubo)
    {
        entt::entity sunEnt  { entt::null };
        entt::entity moonEnt { entt::null };
        {
            auto av { registry.view<AmbientComponent>(entt::exclude<DisabledComponent>) };
            
            for (auto [ent, amb] : av.each())
            {
                sunEnt = amb.SunEntity;
                moonEnt = amb.MoonEntity;
                break;
            }
        }

        bool sunFound { false };

        auto addSun { [&](const WorldMatrixComponent& worldMat, const DirectionalLightComponent& lightComp)
        {
            glm::vec3 forward { -glm::normalize(glm::vec3(worldMat.Matrix[2])) };
            ubo.sunDirection = glm::vec4(forward, 1.0f);
            ubo.sunColor = glm::vec4(lightComp.Color, lightComp.Intensity);
            sunFound = true;
        }};

        if (sunEnt != entt::null)
        {
            auto* wm { registry.try_get<WorldMatrixComponent>(sunEnt) };
            auto* lc { registry.try_get<DirectionalLightComponent>(sunEnt) };
            
            if (wm && lc) { addSun(*wm, *lc); }
        }

        if (!sunFound)
        {
            auto dv { registry.view<WorldMatrixComponent, DirectionalLightComponent>(entt::exclude<DisabledComponent>) };
            
            for (auto [ent, wm, lc] : dv.each())
            {
                if (ent == moonEnt) { continue; }

                addSun(wm, lc);
                break;
            }
        }

        if (!sunFound) { ubo.sunDirection.w = 0.0f; }
        
        bool moonFound { false };
        if (moonEnt != entt::null)
        {
            auto* wm { registry.try_get<WorldMatrixComponent>(moonEnt) };
            auto* lc { registry.try_get<DirectionalLightComponent>(moonEnt) };
                
            if (wm && lc)
            {
                glm::vec3 forward { -glm::normalize(glm::vec3(wm->Matrix[2])) };
                ubo.moonDirection = glm::vec4(forward, 1.0f);
                ubo.moonColor = glm::vec4(lc->Color, lc->Intensity);
                moonFound = true;
            }
        }
        
        if (!moonFound) { ubo.moonDirection.w = 0.0f; }
        

        glm::vec3 shadowLightDir(0.0f, -1.0f, 0.0f);

        if (sunFound && ubo.sunDirection.y >= -0.05f)
        {
            shadowLightDir = glm::vec3(ubo.sunDirection);
            ubo.shadowCaster = 1u;
        }
        else if (moonFound && ubo.moonDirection.y > 0.0f)
        {
            shadowLightDir = glm::vec3(ubo.moonDirection);
            ubo.shadowCaster = 2u;
        }
        else
        {
            ubo.shadowCaster = 0u;
        }

        if (ubo.shadowCaster > 0)
        {
            constexpr float shadowMapRes { 4096.0f };
            constexpr float cascadeSplit { 20.0f };
            constexpr float cascadeFarEnd { 80.0f };

            float fovY { glm::radians(60.0f) };
            float zNear { 0.1f };
            auto camQuery { registry.view<CameraComponent>() };
            for (auto [ent, cam] : camQuery.each())
            {
                if (cam.IsPrimary) { fovY = cam.PerspectiveFOV; zNear = cam.PerspectiveNear; break; }
            }

            float tanHalfFovY { tanf(fovY * 0.5f) };
            float tanHalfFovX { 1.0f / ubo.proj[0][0] };

            glm::mat4 invView { glm::inverse(ubo.view) };
            glm::vec3 cameraPos { glm::vec3(invView[3]) };
            glm::vec3 camRight { glm::normalize(glm::vec3(invView[0])) };
            glm::vec3 camUp { glm::normalize(glm::vec3(invView[1])) };
            glm::vec3 camFwd { -glm::normalize(glm::vec3(invView[2])) };

            glm::vec3 lightDir { -shadowLightDir };
            glm::vec3 worldUp { glm::abs(glm::dot(lightDir, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f
                ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f) };

            float zStarts[2] { zNear, cascadeSplit };
            float zEnds[2] { cascadeSplit, cascadeFarEnd };

            for (int i { 0 }; i < 2; i++)
            {
                glm::vec3 corners[8];
                
                for (int j { 0 }; j < 8; j++)
                {
                    float z { (j < 4) ? zStarts[i] : zEnds[i] };
                    float sx { (j & 1) ? 1.0f : -1.0f };
                    float sy { (j & 2) ? 1.0f : -1.0f };
                    corners[j] = cameraPos + camFwd * z + camRight * (sx * tanHalfFovX * z) + camUp * (sy * tanHalfFovY * z);
                }

                glm::vec3 sphereCenter { 0.0f };
                for (const auto& c : corners) { sphereCenter += c; }
                sphereCenter /= 8.0f;

                float sphereRadius { 0.0f };

                for (const auto& c : corners)
                {
                    float dist { glm::length(c - sphereCenter) };
                    if (dist > sphereRadius) { sphereRadius = dist; }
                }

                constexpr float shadowDepthRange { 400.0f };
                glm::vec3 eye { sphereCenter - lightDir * (shadowDepthRange * 0.5f) };
                glm::mat4 lightView { glm::lookAt(eye, sphereCenter, worldUp) };
                glm::mat4 lightProj { glm::ortho(-sphereRadius, sphereRadius, -sphereRadius, sphereRadius, 1.0f, shadowDepthRange) };
                lightProj[1][1] *= -1.0f;

                glm::vec4 shadowOrigin { (lightProj * lightView) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) };
                glm::vec2 originTexel { (glm::vec2(shadowOrigin) * 0.5f + 0.5f) * shadowMapRes };
                glm::vec2 snapOffset { (glm::round(originTexel) - originTexel) * (2.0f / shadowMapRes) };
                lightProj[3][0] += snapOffset.x;
                lightProj[3][1] += snapOffset.y;

                ubo.lightSpaceMatrices[i] = lightProj * lightView;
            }

            ubo.cascadeSplits = glm::vec4(cascadeSplit, cascadeFarEnd, 0.0f, 0.0f);
        }

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
        else { ubo.ambientEnabled = 0.0f; }
    }

#ifdef ANTELOPE_EDITOR_MODE
    void RenderSystem::RenderEditor(World& world, std::shared_ptr<Renderer> renderer, const EditorCamera& camera)
    {
        if (!renderer) { return; }

        UniformBufferObject cameraUBO {};
        cameraUBO.view = camera.GetViewMatrix();
        
        auto renderExtent { renderer->GetFinalLDRTexture()->GetExtent() };
        cameraUBO.proj = camera.GetProjectionMatrix(
            static_cast<float>(renderExtent.width), 
            static_cast<float>(renderExtent.height)
        );

        glm::mat4 invView { glm::inverse(cameraUBO.view) };
        cameraUBO.cameraPos = glm::vec4(invView[3][0], invView[3][1], invView[3][2], 1.0f);
        static auto s_StartTime { std::chrono::high_resolution_clock::now() };
        cameraUBO.time = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - s_StartTime).count();

        GatherLights(world.GetRegistry(), cameraUBO);

        auto& registry { world.GetRegistry() };
        auto& jobSystem { Application::Get().GetJobSystem() };
        auto& frameAlloc { Application::Get().GetFrameAllocator() };
        auto view { registry.view<WorldMatrixComponent, MeshComponent, NormalMatrixComponent>(entt::exclude<DisabledComponent>) };

        const size_t maxCount { view.size_hint() };
        auto renderEntries { frameAlloc.AllocateArray<RenderEntry>(maxCount) };
        auto renderList { frameAlloc.AllocateArray<RenderCommand>(maxCount) };
        uint32_t count { 0 };

        for (auto [entityID, worldMat, meshComp, normalMat] : view.each())
        {
            renderEntries[count++] = { entityID, &worldMat, &meshComp };
        }

        constexpr uint32_t k_BatchSize { 64 };

        if (count <= k_BatchSize)
        {
            for (uint32_t i { 0 }; i < count; ++i)
            {
                auto& entry { renderEntries[i] };
                glm::mat4 meshLocal { glm::scale(glm::mat4(1.0f), entry.meshComp->Scale) };
                glm::mat4 combined { entry.worldMat->Matrix * meshLocal };
                combined[3] += glm::vec4(entry.meshComp->Offset, 0.0f);

                auto& cmd { renderList[i] };
                cmd.transform = combined;
                cmd.normalMatrix = glm::mat3(glm::transpose(glm::inverse(combined)));
                cmd.mesh = entry.meshComp->Handle;
                cmd.entityID = static_cast<uint32_t>(entry.entity);
                cmd.materialIndex = 0;

                if (auto* mat { registry.try_get<MaterialComponent>(entry.entity) })
                {
                    cmd.materialIndex = mat->MaterialIndex;
                }
            }
        }
        else
        {
            const uint32_t numBatches { (count + k_BatchSize - 1) / k_BatchSize };
            std::vector<JobHandle> handles;
            handles.reserve(numBatches);

            for (uint32_t b { 0 }; b < numBatches; ++b)
            {
                uint32_t begin { b * k_BatchSize };
                uint32_t end { count < (begin + k_BatchSize) ? count : (begin + k_BatchSize) };

                handles.push_back(jobSystem.Submit("RenderListEditor", [begin, end, &registry, renderEntries, renderList]()
                {
                    for (uint32_t i { begin }; i < end; ++i)
                    {
                        auto& entry { renderEntries[i] };
                        glm::mat4 meshLocal { glm::scale(glm::mat4(1.0f), entry.meshComp->Scale) };
                        glm::mat4 combined  { entry.worldMat->Matrix * meshLocal };
                        combined[3] += glm::vec4(entry.meshComp->Offset, 0.0f);

                        auto& cmd { renderList[i] };
                        cmd.transform = combined;
                        cmd.normalMatrix = glm::mat3(glm::transpose(glm::inverse(combined)));
                        cmd.mesh = entry.meshComp->Handle;
                        cmd.entityID = static_cast<uint32_t>(entry.entity);
                        cmd.materialIndex = 0;

                        if (auto* mat { registry.try_get<MaterialComponent>(entry.entity) })
                        {
                            cmd.materialIndex = mat->MaterialIndex;
                        }
                    }
                }));
            }

            for (auto& h : handles) { h.wait(); }
        }

        renderer->DrawFrame(cameraUBO, renderList.first(count));
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

        auto& jobSystem { Application::Get().GetJobSystem() };
        auto& frameAlloc { Application::Get().GetFrameAllocator() };
        auto meshView { registry.view<WorldMatrixComponent, MeshComponent, NormalMatrixComponent>(entt::exclude<DisabledComponent>) };

        const size_t maxCount { meshView.size_hint() };
        auto renderEntries { frameAlloc.AllocateArray<RenderEntry>(maxCount) };
        auto renderList { frameAlloc.AllocateArray<RenderCommand>(maxCount) };
        uint32_t count { 0 };

        for (auto [entityID, worldMat, meshComp, normalMat] : meshView.each())
        {
            renderEntries[count++] = { entityID, &worldMat, &meshComp };
        }

        constexpr uint32_t k_BatchSize { 64 };

        if (count <= k_BatchSize)
        {
            for (uint32_t i { 0 }; i < count; ++i)
            {
                auto& entry { renderEntries[i] };
                glm::mat4 meshLocal { glm::translate(glm::mat4(1.0f), entry.meshComp->Offset) * glm::scale(glm::mat4(1.0f), entry.meshComp->Scale) };
                glm::mat4 combined { entry.worldMat->Matrix * meshLocal };

                auto& cmd { renderList[i] };
                cmd.transform = combined;
                cmd.normalMatrix = glm::mat3(glm::transpose(glm::inverse(combined)));
                cmd.mesh = entry.meshComp->Handle;
                cmd.materialIndex = 0;

                if (auto* mat { registry.try_get<MaterialComponent>(entry.entity) })
                {
                    cmd.materialIndex = mat->MaterialIndex;
                }
            }
        }
        else
        {
            const uint32_t numBatches { (count + k_BatchSize - 1) / k_BatchSize };
            std::vector<JobHandle> handles;
            handles.reserve(numBatches);

            for (uint32_t b { 0 }; b < numBatches; ++b)
            {
                uint32_t begin { b * k_BatchSize };
                uint32_t end { count < (begin + k_BatchSize) ? count : (begin + k_BatchSize) };

                handles.push_back(jobSystem.Submit("RenderListRuntime", [begin, end, &registry, renderEntries, renderList]()
                {
                    for (uint32_t i { begin }; i < end; ++i)
                    {
                        auto& entry { renderEntries[i] };
                        glm::mat4 meshLocal { glm::translate(glm::mat4(1.0f), entry.meshComp->Offset) * glm::scale(glm::mat4(1.0f), entry.meshComp->Scale) };
                        glm::mat4 combined  { entry.worldMat->Matrix * meshLocal };

                        auto& cmd { renderList[i] };
                        cmd.transform = combined;
                        cmd.normalMatrix = glm::mat3(glm::transpose(glm::inverse(combined)));
                        cmd.mesh = entry.meshComp->Handle;
                        cmd.materialIndex = 0;

                        if (auto* mat { registry.try_get<MaterialComponent>(entry.entity) })
                        {
                            cmd.materialIndex = mat->MaterialIndex;
                        }
                    }
                }));
            }

            for (auto& h : handles) { h.wait(); }
        }

        renderer->DrawFrame(cameraUBO, renderList.first(count));
    }
}
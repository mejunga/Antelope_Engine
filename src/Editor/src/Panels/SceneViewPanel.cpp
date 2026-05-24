#include <Editor/Panels/SceneViewPanel.hpp>
#include <Editor/Panels/GameViewPanel.hpp>

#include <Engine/Core/Application.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#include <Engine/Renderer/UI/UIContext.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Platform/Input.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/ECS/System/PhysicsSystem.hpp>
#include <Engine/Physics/PhysicsContext.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>


namespace Antelope::Editor
{
    SceneViewPanel::SceneViewPanel()
    {
        m_ScenePicker = std::make_unique<ScenePicker>(Application::Get().GetVulkanContext(), 1, 1);
    }

    void SceneViewPanel::OnUIRender(EditorCamera& camera, GameViewPanel& gamePanel)
    {
        ImGuiWindowFlags windowFlags { ImGuizmo::IsUsing() ? ImGuiWindowFlags_NoMove : 0 };
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 0.0f, 0.0f });
        ImGui::Begin("Scene", nullptr, windowFlags);

        m_ViewportFocused = ImGui::IsWindowFocused();
        m_ViewportHovered = ImGui::IsWindowHovered();
        bool isRightClicking { Input::IsMouseButtonClicked(GLFW_MOUSE_BUTTON_RIGHT) };

        if (m_ViewportHovered && isRightClicking)
        {
            m_IsCameraMoving = true;
        }
        else if (!isRightClicking)
        {
            m_IsCameraMoving = false;
        }

        camera.SetActive(m_ViewportHovered || m_IsCameraMoving);

        ImVec2 viewportSize { ImGui::GetContentRegionAvail() };

        if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
        {
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        uint32_t newWidth { static_cast<uint32_t>(viewportSize.x) };
        uint32_t newHeight { static_cast<uint32_t>(viewportSize.y) };

        if (newWidth > 0 && newHeight > 0 && (newWidth != m_LastPanelSize.x || newHeight != m_LastPanelSize.y))
        {
            m_LastPanelSize = viewportSize;
            m_PendingResizeFrames = 1;
        }

        if (m_PendingResizeFrames > 0)
        {
            m_PendingResizeFrames--;
            if (m_PendingResizeFrames == 0)
            {
                uint32_t w { static_cast<uint32_t>(m_LastPanelSize.x) };
                uint32_t h { static_cast<uint32_t>(m_LastPanelSize.y) };
                Application::Get().GetRenderer()->ResizeRenderTexture(w, h);
                Application::Get().GetUIContext()->UpdateSceneTextureID();
                m_ScenePicker->Resize(w, h);
            }
        }

        void* textureID { Application::Get().GetUIContext()->GetSceneTextureID() };
        
        if (textureID)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(textureID), viewportSize);

            if (ImGui::BeginDragDropTarget())
            {
                if (auto* payload { ImGui::AcceptDragDropPayload("ASSET_MESH") })
                {
                    if (m_OnMeshDropped)
                    {
                        auto viewportMinRegion { ImGui::GetWindowContentRegionMin() };
                        auto viewportMaxRegion { ImGui::GetWindowContentRegionMax() };
                        auto viewportOffset    { ImGui::GetWindowPos() };

                        float bMinX { viewportMinRegion.x + viewportOffset.x };
                        float bMinY { viewportMinRegion.y + viewportOffset.y };
                        float vpW { viewportMaxRegion.x - viewportMinRegion.x };
                        float vpH { viewportMaxRegion.y - viewportMinRegion.y };

                        auto mouse { ImGui::GetMousePos() };
                        float mx { (mouse.x - bMinX) / vpW };
                        float my { (mouse.y - bMinY) / vpH };

                        glm::mat4 cameraProj { camera.GetProjectionMatrix(vpW, vpH) };
                        cameraProj[1][1] *= -1.0f;
                        glm::mat4 invPV { glm::inverse(cameraProj * camera.GetViewMatrix()) };

                        float ndcX { 2.0f * mx - 1.0f };
                        float ndcY { 2.0f * my - 1.0f };

                        glm::vec4 n4 { invPV * glm::vec4(ndcX, ndcY, 0.0f, 1.0f) };
                        glm::vec4 f4 { invPV * glm::vec4(ndcX, ndcY, 1.0f, 1.0f) };
                        glm::vec3 nearPt { glm::vec3(n4) / n4.w };
                        glm::vec3 farPt { glm::vec3(f4) / f4.w };
                        glm::vec3 dir { glm::normalize(farPt - nearPt) };

                        glm::vec3 spawnPos { nearPt + 10.0f * dir };

                        if (glm::abs(dir.y) > 0.0001f)
                        {
                            float t { -nearPt.y / dir.y };

                            if (t > 0.0f && t < 500.0f) { spawnPos = nearPt + t * dir; }
                        }

                        UUID uuid { *reinterpret_cast<const UUID*>(payload->Data) };
                        Entity spawned { m_OnMeshDropped(uuid, spawnPos) };
                        
                        if (spawned) { m_SelectedEntity = spawned; }
                    }
                }

                ImGui::EndDragDropTarget();
            }
        }

        constexpr float k_BtnSize { 34.0f };
        constexpr float k_Gap { 9.0f };
        constexpr float k_Pad { 6.0f };
        constexpr float k_BarW { 3 * k_BtnSize + 2 * k_Gap };

        {
            ImVec2 wp { ImGui::GetWindowPos() };
            ImVec2 cMin { ImGui::GetWindowContentRegionMin() };
            ImVec2 cMax { ImGui::GetWindowContentRegionMax() };
            float bx { wp.x + cMin.x + (cMax.x - cMin.x - k_BarW) * 0.5f };
            float by { wp.y + cMax.y - k_BtnSize - 8.0f };
            ImVec2 mouse { ImGui::GetMousePos() };

            m_MouseOverPlaybar = mouse.x >= bx - k_Pad && mouse.x <= bx + k_BarW + k_Pad && mouse.y >= by - k_Pad && mouse.y <= by + k_BtnSize + k_Pad;
        }

        if (m_ViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !m_MouseOverPlaybar)
        {
            auto viewportMinRegion { ImGui::GetWindowContentRegionMin() };
            auto viewportMaxRegion { ImGui::GetWindowContentRegionMax() };
            auto viewportOffset { ImGui::GetWindowPos() };

            ImVec2 viewportBounds[2];
            viewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
            viewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

            auto mousePos { ImGui::GetMousePos() };
                
            float pickX { mousePos.x - viewportBounds[0].x };
            float pickY { mousePos.y - viewportBounds[0].y };

            if (pickX >= 0 && pickY >= 0 && pickX < newWidth && pickY < newHeight) 
            {
                auto& registry { Application::Get().GetWorld()->GetRegistry() };
                auto view { registry.view<WorldMatrixComponent, MeshComponent, NormalMatrixComponent>() };
                size_t maxCount { view.size_hint() };
                auto renderList { Application::Get().GetFrameAllocator().AllocateArray<RenderCommand>(maxCount) };
                uint32_t count { 0 };

                for (auto [entityID, worldMat, mesh, normalMat] : view.each())
                {
                    glm::mat4 meshLocal { glm::scale(glm::mat4(1.0f), mesh.Scale) };
                    glm::mat4 combined { worldMat.Matrix * meshLocal };
                    combined[3] += glm::vec4(mesh.Offset, 0.0f);

                    RenderCommand cmd {};
                    cmd.transform = combined;
                    cmd.normalMatrix = normalMat.Matrix;
                    cmd.mesh = mesh.Handle;
                    cmd.entityID = static_cast<uint32_t>(entityID);

                    AnimatorComponent* animator { nullptr };
                    entt::entity animRoot { entt::null };
                    entt::entity e { entityID };

                    while (e != entt::null && !animator)
                    {
                        animator = registry.try_get<AnimatorComponent>(e);
                        if (animator) { animRoot = e; }
                        auto* r { registry.try_get<RelationshipComponent>(e) };
                        e = r ? r->Parent : entt::null;
                    }

                    if (animator && !animator->FinalBoneMatrices.empty())
                    {
                        cmd.isAnimated = 1;
                        cmd.BoneMatrices = animator->FinalBoneMatrices.data();
                        cmd.BoneCount = static_cast<uint32_t>(animator->FinalBoneMatrices.size());
                        
                        if (auto* rootWorld { registry.try_get<WorldMatrixComponent>(animRoot) })
                        {
                            cmd.transform = rootWorld->Matrix;
                        }
                    }

                    renderList[count++] = cmd;
                }

                m_ScenePicker->SubmitPick(static_cast<uint32_t>(pickX), static_cast<uint32_t>(pickY), camera, renderList.first(count));
            }
        }

        if (auto result { m_ScenePicker->TryGetPickResult() })
        {
            uint32_t pickedID { *result };

            if (pickedID != static_cast<uint32_t>(entt::null))
            {
                Entity pickedEntity { Entity(static_cast<entt::entity>(pickedID), Application::Get().GetWorld().get()) };
                Entity rootEntity { pickedEntity };
                
                while (rootEntity.GetComponent<RelationshipComponent>().Parent != entt::null)
                {
                    rootEntity = Entity(rootEntity.GetComponent<RelationshipComponent>().Parent, Application::Get().GetWorld().get());
                }

                if (rootEntity != pickedEntity)
                {
                    if (m_SelectedEntity == rootEntity) 
                    {
                        m_SelectedEntity = pickedEntity;
                    } 
                    else 
                    {
                        m_SelectedEntity = rootEntity;
                    }
                }
                else
                {
                    m_SelectedEntity = pickedEntity;
                }
            } 
            else 
            {
                m_SelectedEntity = {};
            }
        }

        auto renderer { Application::Get().GetRenderer() };

        if (m_SelectedEntity)
        {
            bool isRoot { m_SelectedEntity.GetComponent<RelationshipComponent>().Parent == entt::null };
            glm::vec4 color { isRoot
                ? glm::vec4(0.3f, 0.6f, 1.0f, 1.0f)
                : glm::vec4(1.0f, 0.6f, 0.0f, 1.0f) };

            std::unordered_set<uint32_t> ids;
            CollectMeshDescendants(m_SelectedEntity, ids);
            renderer->SetSelectedEntityIDs(std::move(ids), color);
        }
        else
        {
            renderer->SetSelectedEntityIDs({});
        }

        if (m_ViewportFocused && !m_IsCameraMoving) 
        {
            if (Input::IsKeyPressed(GLFW_KEY_Q)) { m_GizmoType = -1; }
            if (Input::IsKeyPressed(GLFW_KEY_W)) { m_GizmoType = ImGuizmo::OPERATION::TRANSLATE; }
            if (Input::IsKeyPressed(GLFW_KEY_E)) { m_GizmoType = ImGuizmo::OPERATION::ROTATE; }
            if (Input::IsKeyPressed(GLFW_KEY_R)) { m_GizmoType = ImGuizmo::OPERATION::SCALE; }
        }

        float windowWidth { (float)ImGui::GetWindowWidth() };
        float windowHeight { (float)ImGui::GetWindowHeight() };
        glm::mat4 cameraView { camera.GetViewMatrix() };
        glm::mat4 cameraProjection { camera.GetProjectionMatrix(windowWidth, windowHeight) };
        cameraProjection[1][1] *= -1.0f;

        if (m_SelectedEntity && m_GizmoType != -1)
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();

            ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);

            auto& tc { m_SelectedEntity.GetComponent<TransformComponent>() };
            auto& worldMat { m_SelectedEntity.GetComponent<WorldMatrixComponent>() };
            glm::mat4 transform { worldMat.Matrix };
            ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform));

            if (ImGuizmo::IsUsing())
            {
                auto& rel { m_SelectedEntity.GetComponent<RelationshipComponent>() };

                glm::mat4 localTransform { transform };

                if (rel.Parent != entt::null)
                {
                    const auto& parentWorld { Application::Get().GetWorld()->GetRegistry().get<WorldMatrixComponent>(rel.Parent) };
                    localTransform = glm::inverse(parentWorld.Matrix) * transform;
                }

                glm::vec3 translation, rotation, scale;
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localTransform), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale));

                tc.Translation = translation;
                tc.Rotation = glm::radians(rotation);
                tc.Scale = scale;
                Application::Get().GetWorld()->MarkTransformDirty(m_SelectedEntity);

                if (Application::Get().GetWorld()->IsSimulating())
                {
                    PhysicsSystem::SetBodyTransform(
                        *Application::Get().GetWorld(), 
                        *Application::Get().GetWorld()->GetPhysicsContext(), 
                        m_SelectedEntity.GetHandle()
                    );
                }
            }
        }

        glm::mat4 vp { cameraProjection * cameraView };
        DrawColliderGizmos(vp, ImGui::GetWindowPos(), { (float)newWidth, (float)newHeight });

        {
            ImVec2 winPos { ImGui::GetWindowPos() };
            ImVec2 cMax { ImGui::GetWindowContentRegionMax() };
            char buf[24];
            std::snprintf(buf, sizeof(buf), "FPS: %d", static_cast<int>(ImGui::GetIO().Framerate));
            ImVec2 textSize { ImGui::CalcTextSize(buf) };
            constexpr float k_Pad { 6.0f };
            constexpr float k_Margin { 8.0f };
            float rx { winPos.x + cMax.x - textSize.x - k_Pad * 2.0f - k_Margin };
            float ry { winPos.y + cMax.y - textSize.y - k_Pad * 2.0f - k_Margin };
            ImDrawList* dl { ImGui::GetWindowDrawList() };
            dl->AddRectFilled(
                ImVec2(rx - k_Pad, ry - k_Pad),
                ImVec2(rx + textSize.x + k_Pad, ry + textSize.y + k_Pad),
                IM_COL32(20, 20, 20, 180), 4.0f
            );
            dl->AddText(ImVec2(rx, ry), IM_COL32(180, 230, 180, 255), buf);
        }

        gamePanel.DrawPlaybackControls(Application::Get().GetWorld().get());

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void SceneViewPanel::CollectMeshDescendants(Entity entity, std::unordered_set<uint32_t>& ids)
    {
        if (entity.HasComponent<MeshComponent>())
        {
            ids.insert(static_cast<uint32_t>(entity.GetHandle()));
        }

        auto& rel { entity.GetComponent<RelationshipComponent>() };
        auto child { rel.FirstChild };

        while (child != entt::null)
        {
            Entity childEntity { child, Application::Get().GetWorld().get() };
            CollectMeshDescendants(childEntity, ids);
            child = childEntity.GetComponent<RelationshipComponent>().NextSibling;
        }
    }

    void SceneViewPanel::DrawColliderGizmos(const glm::mat4& viewProj, ImVec2 windowPos, ImVec2 windowSize)
    {
        if (!m_ShowColliders) { return; }

        ImDrawList* dl { ImGui::GetWindowDrawList() };
        auto& registry { Application::Get().GetWorld()->GetRegistry() };
        auto view { registry.view<ColliderComponent, WorldMatrixComponent>() };

        for (auto [entityID, col, worldMat] : view.each())
        {
            bool isSelected { m_SelectedEntity && m_SelectedEntity.GetHandle() == entityID };
            ImU32 color { isSelected ? IM_COL32(0, 255, 80, 255) : IM_COL32(0, 200, 80, 120) };
            float thickness { isSelected ? 1.5f : 0.75f };

            auto drawLine { [&](glm::vec3 a, glm::vec3 b) {
                glm::vec4 ca { viewProj * glm::vec4(a, 1.0f) };
                glm::vec4 cb { viewProj * glm::vec4(b, 1.0f) };

                if (ca.w < 0.001f && cb.w < 0.001f) { return; }

                if (ca.w < 0.001f)
                {
                    float t { (0.001f - ca.w) / (cb.w - ca.w) };
                    ca = ca + t * (cb - ca);
                }
                else if (cb.w < 0.001f)
                {
                    float t { (0.001f - cb.w) / (ca.w - cb.w) };
                    cb = cb + t * (ca - cb);
                }

                ImVec2 sa { windowPos.x + (ca.x/ca.w * 0.5f + 0.5f) * windowSize.x, windowPos.y + (0.5f - ca.y/ca.w * 0.5f) * windowSize.y };
                ImVec2 sb { windowPos.x + (cb.x/cb.w * 0.5f + 0.5f) * windowSize.x, windowPos.y + (0.5f - cb.y/cb.w * 0.5f) * windowSize.y };
                dl->AddLine(sa, sb, color, thickness);
            }};

            glm::vec3 offset { col.Offset };
            glm::mat3 rot { worldMat.Matrix };
            rot[0] = glm::normalize(rot[0]);
            rot[1] = glm::normalize(rot[1]);
            rot[2] = glm::normalize(rot[2]);
            glm::mat4 colliderTransform { glm::mat4(rot) };
            colliderTransform[3] = worldMat.Matrix[3];

            if (col.Type == ColliderType::Box)
            {
                glm::vec3 half { col.Size * 0.5f };
                glm::vec3 c[8];
                int i { 0 };

                for (int x : { -1, 1 }) for (int y : { -1, 1 }) for (int z : { -1, 1 })
                {
                    c[i++] = glm::vec3(colliderTransform * glm::vec4(offset + glm::vec3(x, y, z) * half, 1.0f));
                }

                drawLine(c[0],c[1]); drawLine(c[0],c[4]); drawLine(c[1],c[5]); drawLine(c[4],c[5]);
                drawLine(c[2],c[3]); drawLine(c[2],c[6]); drawLine(c[3],c[7]); drawLine(c[6],c[7]);
                drawLine(c[0],c[2]); drawLine(c[1],c[3]); drawLine(c[4],c[6]); drawLine(c[5],c[7]);
            }
            else if (col.Type == ColliderType::Sphere)
            {
                glm::vec3 center { glm::vec3(colliderTransform * glm::vec4(offset, 1.0f)) };
                float radius { col.Size.x };
                constexpr int segments { 32 };

                for (int plane { 0 }; plane < 3; ++plane)
                {
                    for (int s { 0 }; s < segments; ++s)
                    {
                        float a0 { (s / (float)segments) * glm::two_pi<float>() };
                        float a1 { ((s + 1) / (float)segments) * glm::two_pi<float>() };
                        glm::vec3 p0 { center }, p1 { center };

                        if (plane == 0) { p0 += glm::vec3(cos(a0), sin(a0), 0) * radius; p1 += glm::vec3(cos(a1), sin(a1), 0) * radius; }
                        else if (plane == 1) { p0 += glm::vec3(cos(a0), 0, sin(a0)) * radius; p1 += glm::vec3(cos(a1), 0, sin(a1)) * radius; }
                        else { p0 += glm::vec3(0, cos(a0), sin(a0)) * radius; p1 += glm::vec3(0, cos(a1), sin(a1)) * radius; }

                        drawLine(p0, p1); 
                    }
                }
            }
        }
    }
}
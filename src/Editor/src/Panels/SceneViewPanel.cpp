#include <Editor/Panels/SceneViewPanel.hpp>

#include <Engine/Core/Application.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/UI/UIContext.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Platform/Input.hpp>
#include <Engine/Debug/Log.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>


namespace Antelope
{
    SceneViewPanel::SceneViewPanel()
    {
        m_ScenePicker = std::make_unique<ScenePicker>(Application::Get().GetVulkanContext(), 800, 600);
    }

    void SceneViewPanel::OnUIRender(EditorCamera& camera)
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
        }

        if (m_ViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
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
                std::vector<RenderCommand> renderList;
                auto& registry { Application::Get().GetWorld()->GetRegistry() };
                auto view { registry.view<TransformComponent, MeshComponent>() };
                
                for (auto [entityID, transform, mesh] : view.each()) {
                    RenderCommand cmd{};
                    cmd.transform = transform.WorldMatrix;
                    cmd.normalMatrix = transform.NormalMatrix;
                    cmd.mesh = mesh.Handle;
                    
                #ifdef ANTELOPE_EDITOR_MODE
                    cmd.entityID = static_cast<uint32_t>(entityID);
                #endif
                    renderList.push_back(cmd);
                }

                m_ScenePicker->SubmitPick(static_cast<uint32_t>(pickX), static_cast<uint32_t>(pickY), camera, renderList);
            }
        }

        if (auto result { m_ScenePicker->TryGetPickResult() })
        {
            uint32_t pickedID { *result };

            if (pickedID != static_cast<uint32_t>(entt::null))
            {
                Entity pickedEntity { Entity(static_cast<entt::entity>(pickedID), Application::Get().GetWorld().get()) };
                auto& rel { pickedEntity.GetComponent<RelationshipComponent>() };

                if (rel.Parent != entt::null)
                {
                    Entity parentEntity { Entity(rel.Parent, Application::Get().GetWorld().get()) };
                    
                    if (m_SelectedEntity == parentEntity) 
                    {
                        m_SelectedEntity = pickedEntity;
                    } 
                    else 
                    {
                        m_SelectedEntity = parentEntity;
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

        if (m_ViewportFocused && !m_IsCameraMoving) 
        {
            if (Input::IsKeyPressed(GLFW_KEY_Q)) { m_GizmoType = -1; }
            if (Input::IsKeyPressed(GLFW_KEY_W)) { m_GizmoType = ImGuizmo::OPERATION::TRANSLATE; }
            if (Input::IsKeyPressed(GLFW_KEY_E)) { m_GizmoType = ImGuizmo::OPERATION::ROTATE; }
            if (Input::IsKeyPressed(GLFW_KEY_R)) { m_GizmoType = ImGuizmo::OPERATION::SCALE; }
        }

        if (m_SelectedEntity && m_GizmoType != -1)
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();

            float windowWidth { (float)ImGui::GetWindowWidth() };
            float windowHeight { (float)ImGui::GetWindowHeight() };
            ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);

            glm::mat4 cameraView { camera.GetViewMatrix() };
            glm::mat4 cameraProjection { camera.GetProjectionMatrix(windowWidth, windowHeight) };
            cameraProjection[1][1] *= -1.0f;

            auto& tc { m_SelectedEntity.GetComponent<TransformComponent>() };
            glm::mat4 transform { tc.WorldMatrix };

            ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform));

            if (ImGuizmo::IsUsing())
            {
                auto& rel { m_SelectedEntity.GetComponent<RelationshipComponent>() };

                glm::mat4 localTransform { transform };

                if (rel.Parent != entt::null)
                {
                    auto& parentTc { Application::Get().GetWorld()->GetRegistry().get<TransformComponent>(rel.Parent) };
                    localTransform = glm::inverse(parentTc.WorldMatrix) * transform;
                }

                glm::vec3 translation, rotation, scale;
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localTransform), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale));

                tc.Translation = translation;
                tc.Rotation = glm::radians(rotation);
                tc.Scale = scale;
                Application::Get().GetWorld()->MarkTransformDirty(m_SelectedEntity);
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }
}
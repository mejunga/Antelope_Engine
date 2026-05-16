#pragma once

#include <Engine/ECS/Entity.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#include <Engine/Renderer/UI/ScenePicker.hpp>
#include <Engine/Core/UUID.hpp>

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>

#include <unordered_set>
#include <memory>
#include <functional>


namespace Antelope::Editor
{
    class GameViewPanel;

    class SceneViewPanel
    {
        public:
            SceneViewPanel();
            ~SceneViewPanel() = default;

            void OnUIRender(EditorCamera& camera, GameViewPanel& gamePanel);
            inline void SetOnMeshDropped(std::function<Entity(UUID, glm::vec3)> cb) { m_OnMeshDropped = std::move(cb); }
            inline void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }
            inline Entity& GetSelectedEntity() { return m_SelectedEntity; }

        private:
            void CollectMeshDescendants(Entity entity, std::unordered_set<uint32_t>& ids);
            void DrawColliderGizmos(const glm::mat4& viewProj, ImVec2 windowPos, ImVec2 windowSize);
            
        private:
            int m_PendingResizeFrames { 0 };
            ImVec2 m_LastPanelSize { 0.0f, 0.0f };

            bool m_ViewportFocused { false };
            bool m_ViewportHovered { false };
            bool m_IsCameraMoving { false };
            bool m_ShowColliders { true };
            bool m_MouseOverPlaybar { false };

            std::function<Entity(UUID, glm::vec3)> m_OnMeshDropped;

            int m_GizmoType { ImGuizmo::OPERATION::TRANSLATE };
            Entity m_SelectedEntity;
                
            std::unique_ptr<ScenePicker> m_ScenePicker;
    };
}
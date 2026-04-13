#pragma once

#include <Engine/ECS/Entity.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#include <Engine/Renderer/UI/ScenePicker.hpp>

#include <imgui.h>
#include <ImGuizmo.h>

#include <unordered_set>
#include <memory>


namespace Antelope::Editor
{
    class SceneViewPanel
    {
        public:
            SceneViewPanel();
            ~SceneViewPanel() = default;

            void OnUIRender(EditorCamera& camera);
            inline void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }
            inline Entity& GetSelectedEntity() { return m_SelectedEntity; }

        private:
            void CollectMeshDescendants(Entity entity, std::unordered_set<uint32_t>& ids);

        private:
            int m_PendingResizeFrames { 0 };
            ImVec2 m_LastPanelSize { 0.0f, 0.0f };

            bool m_ViewportFocused { false };
            bool m_ViewportHovered { false };
            bool m_IsCameraMoving { false };

            int m_GizmoType { ImGuizmo::OPERATION::TRANSLATE };
            Entity m_SelectedEntity;
                
            std::unique_ptr<ScenePicker> m_ScenePicker;
    };
}
#pragma once

#include <Engine/ECS/Entity.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#include <Engine/Renderer/UI/ScenePicker.hpp>

#include <imgui.h>
#include <ImGuizmo.h>

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
            inline Entity GetSelectedEntity() const { return m_SelectedEntity; }

        private:
            int m_PendingResizeFrames { 0 };
            ImVec2 m_LastPanelSize { 0.0f, 0.0f };

            bool m_ViewportFocused { false };
            bool m_ViewportHovered { false };
            bool m_IsCameraMoving { false };

            int m_GizmoType { -1 };
            Entity m_SelectedEntity;
                
            std::unique_ptr<ScenePicker> m_ScenePicker;
    };
}
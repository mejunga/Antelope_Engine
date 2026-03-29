#pragma once

#include <imgui.h>

namespace Antelope
{
    class SceneViewportPanel
    {
        public:
            SceneViewportPanel() = default;
            ~SceneViewportPanel() = default;

            void OnUIRender();

        private:
            bool m_PanelResizing { false };
            float m_PanelResizeTimer { 0.0f };
            ImVec2 m_LastPanelSize { 0.0f, 0.0f };
    };
}
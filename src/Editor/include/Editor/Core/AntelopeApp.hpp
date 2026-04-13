#pragma once

#include <Editor/Panels/SceneViewPanel.hpp>
#include <Editor/Panels/HierarchyPanel.hpp>
#include <Editor/Panels/PropertiesPanel.hpp>
#include <Editor/Panels/ConsolePanel.hpp>
#include <Editor/Panels/ProjectPanel.hpp>

#include <Engine/Core/Application.hpp>
#include <Engine/Renderer/Graphics/Model.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>

#include <vector>


namespace Antelope::Editor
{
    class AntelopeApp : public Application
    {
        public:
            AntelopeApp();
            ~AntelopeApp();

            void OnInit() override;
            void OnUpdate(float timeStep) override;
            void OnUIRender() override;
            void OnShutdown() override;

        private:
            void SetupMockData();

        private:
            ModelData m_BearMesh;
            uint32_t m_BearTexID { 0 };

            float m_DebounceTimer { 0.0f };
            const float DEBOUNCE_DELAY { 0.2f };

            Entity m_BearRoot;

            EditorCamera m_EditorCamera;
            SceneViewPanel m_ScenePanel;
            HierarchyPanel m_HierarchyPanel;
            PropertiesPanel m_PropertiesPanel;
            ConsolePanel m_ConsolePanel;
            ProjectPanel m_ProjectPanel;
    };
}
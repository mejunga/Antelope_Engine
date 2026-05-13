#pragma once

#include <Editor/Panels/SceneViewPanel.hpp>
#include <Editor/Panels/HierarchyPanel.hpp>
#include <Editor/Panels/PropertiesPanel.hpp>
#include <Editor/Panels/ConsolePanel.hpp>
#include <Editor/Panels/ProjectPanel.hpp>
#include <Editor/Panels/AnimatorPanel.hpp>
#include <Editor/Core/Project.hpp>

#include <Engine/Core/Application.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#include <Engine/Scene/SceneSerializer.hpp>
#include <Engine/Renderer/Graphics/Model.hpp>

#include <unordered_map>
#include <filesystem>
#include <string>
#include <vector>


namespace Antelope::Editor
{
    class AntelopeApp : public Application
    {
        public:
            explicit AntelopeApp(const std::string& projectRoot);
            ~AntelopeApp();

            void OnInit() override;
            void OnUpdate(float timeStep) override;
            void OnUIRender() override;
            void OnShutdown() override;

        private:
            void LoadScene(const std::string& virtualPath);
            void SaveScene(const std::string& virtualPath);
            void NewUnnamedScene();
            void FreeSceneMeshes();
            void RenderSaveAsPopup();
            void PopulateAssetRecords();
            Entity SpawnModel(UUID assetUUID, glm::vec3 spawnPos);

        private:
            std::filesystem::path m_ProjectRoot;
            std::filesystem::path m_ProjectFilePath;
            ProjectState m_ProjectState;

            std::string m_CurrentScenePath;
            bool m_SceneIsUntitled { false };
            bool m_OpenSaveAsPopup { false };
            char m_SaveAsNameBuf[128] { "Unnamed" };

            std::vector<AssetBinding> m_AssetBindings;
            std::unordered_map<uint64_t, ModelData> m_ModelCache;

            float m_DebounceTimer { 0.0f };
            static constexpr float DEBOUNCE_DELAY { 0.2f };

            EditorCamera m_EditorCamera;
            SceneViewPanel m_ScenePanel;
            HierarchyPanel m_HierarchyPanel;
            PropertiesPanel m_PropertiesPanel;
            ConsolePanel m_ConsolePanel;
            ProjectPanel m_ProjectPanel;
            AnimatorPanel m_AnimatorPanel;
    };
}
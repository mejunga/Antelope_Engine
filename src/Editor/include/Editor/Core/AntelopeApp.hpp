#pragma once

#include <Editor/Panels/SceneViewPanel.hpp>

#include <Engine/Core/Application.hpp>
#include <Engine/Renderer/Graphics/Model.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>

#include <imgui.h>

#include <vector>


class AntelopeApp : public Antelope::Application
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
        Antelope::ModelData m_BearMesh;
        uint32_t m_BearTexID { 0 };

        Antelope::ModelData m_GorillaMesh;
        uint32_t m_GorillaTexID { 0 };

        int m_RenderState { 0 };
        float m_DebounceTimer { 0.0f };
        const float DEBOUNCE_DELAY { 0.2f };

        Antelope::EditorCamera m_EditorCamera;
        std::vector<Antelope::Entity> m_ActiveEntities;

        Antelope::SceneViewPanel m_ScenePanel;
};
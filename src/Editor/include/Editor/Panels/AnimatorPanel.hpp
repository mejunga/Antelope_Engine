#pragma once

#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/ECS/AnimatorController.hpp>
#include <Engine/Renderer/Graphics/Model.hpp>
#include <Editor/Panels/ProjectPanel.hpp>

#include <imgui.h>
#include <imgui-node-editor/imgui_node_editor.h>

#include <unordered_map>
#include <cstdint>


namespace Antelope::Editor
{
    class AnimatorPanel
    {
        public:
            AnimatorPanel();
            ~AnimatorPanel();

            void SetModelCache(const std::unordered_map<uint64_t, ModelData>* cache);
            void OnUIRender(Entity selectedEntity);

        private:
            void DrawParametersSidebar(AnimatorComponent& anim);
            void DrawNodeCanvas(AnimatorComponent& anim);
            void DrawInspectorPanel(AnimatorComponent& anim);

        private:
            ax::NodeEditor::EditorContext* m_NodeEditorCtx { nullptr };
            const std::unordered_map<uint64_t, ModelData>* m_ModelCache { nullptr };

            float m_PreviewTime { 0.0f };
            uint32_t m_SelectedState { UINT32_MAX };
            uint32_t m_SelectedLink { UINT32_MAX };
            uint32_t m_ContextMenuState { UINT32_MAX };
            uint32_t m_ContextLinkIdx { UINT32_MAX };
            char m_NewParamName[64] { "Param" };
            int m_NewParamType { 0 };
            char m_NewStateName[64] { "New State" };
            uint32_t m_LastStateCount { 0 };
            uint64_t m_LastEntityID { UINT64_MAX };
            bool m_ShouldLayoutNodes { true };
            bool m_IsRenamingState { false };
            char m_RenameBuffer[64] { "" };
    };
}
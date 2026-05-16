#pragma once

#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/glm.hpp>

#include <string>


namespace Antelope
{
    class EditorCamera;
}

namespace Antelope::Editor
{
    class PropertiesPanel
    {
        public:
            PropertiesPanel() = default;

            void OnUIRender(Entity entity, const EditorCamera& camera);

        private:
            void DrawComponents(Entity entity, const EditorCamera& camera);
            bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f);

            template<typename T, typename UIFunction>
            void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction)
            {
                const ImGuiTreeNodeFlags treeNodeFlags {
                    ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
                    ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
                    ImGuiTreeNodeFlags_FramePadding };

                if (!entity.HasComponent<T>()) { return; }

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
                ImGui::Separator();

                float lineHeight { ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f };
                bool open { ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, "%s", name.c_str()) };
                ImGui::PopStyleVar();
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - lineHeight);

                if (ImGui::Button("x", ImVec2{ lineHeight, lineHeight }))
                {
                    entity.RemoveComponent<T>();
                    if (open) { ImGui::TreePop(); }
                    return;
                }

                if (open)
                {
                    auto& component { entity.GetComponent<T>() };
                    uiFunction(component);
                    ImGui::TreePop();
                }
            }
    };
}
#include <Editor/Panels/GameViewPanel.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Renderer/UI/UIContext.hpp>

#include <imgui.h>
#include <imgui_internal.h>


namespace Antelope::Editor
{
    void GameViewPanel::OnUIRender(World* world)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Game");

        ImGuiWindow* win { ImGui::GetCurrentWindow() };
        m_IsActive = !win->DockIsActive || win->DockTabIsVisible;

        ImVec2 available { ImGui::GetContentRegionAvail() };

        void* texID { Application::Get().GetUIContext()->GetSceneTextureID() };

        if (texID && available.x > 0.0f && available.y > 0.0f)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(texID), available);
        }

        DrawPlaybackControls(world);

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void GameViewPanel::DrawPlaybackControls(World* world)
    {
        bool isPlaying { world->IsSimulating() };

        constexpr float k_BtnSize { 34.0f };
        constexpr float k_Gap { 9.0f };
        constexpr int k_NumBtns { 3 };
        float barW { k_NumBtns * k_BtnSize + (k_NumBtns - 1) * k_Gap };

        ImVec2 winPos { ImGui::GetWindowPos() };
        ImVec2 cMin { ImGui::GetWindowContentRegionMin() };
        ImVec2 cMax { ImGui::GetWindowContentRegionMax() };
        float bx { winPos.x + cMin.x + (cMax.x - cMin.x - barW) * 0.5f };
        float by { winPos.y + cMax.y - k_BtnSize - 8.0f };

        constexpr float k_Pad { 6.0f };
        ImDrawList* dl { ImGui::GetWindowDrawList() };
        dl->AddRectFilled(
            ImVec2(bx - k_Pad, by - k_Pad),
            ImVec2(bx + barW + k_Pad, by + k_BtnSize + k_Pad),
            IM_COL32(65, 65, 65, 220), 6.0f
        );

        ImGui::SetCursorScreenPos(ImVec2(bx, by));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.30f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));

        if (ImGui::Button(isPlaying ? "\xe2\x96\xa0##stop" : ">##play", ImVec2(k_BtnSize, k_BtnSize)))
        {
            if (isPlaying)
            {
                world->OnSimulationStop();
                world->RestoreSnapshot();
                m_Paused = false;
                ImGui::SetWindowFocus("Scene");
            }
            else
            {
                world->TakeSnapshot();
                world->OnSimulationStart();
                m_Paused = false;
                ImGui::SetWindowFocus("Game");
            }
        }

        ImGui::SameLine(0.0f, k_Gap);

        ImGui::BeginDisabled(!isPlaying);
        if (ImGui::Button(m_Paused ? ">##resume" : "||##pause", ImVec2(k_BtnSize, k_BtnSize)))
        {
            m_Paused = !m_Paused;
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, k_Gap);

        ImGui::BeginDisabled(!isPlaying || !m_Paused);
        if (ImGui::Button(">|##step", ImVec2(k_BtnSize, k_BtnSize)))
        {
            world->StepSimulation(1.0f / 60.0f);
        }
        ImGui::EndDisabled();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
}
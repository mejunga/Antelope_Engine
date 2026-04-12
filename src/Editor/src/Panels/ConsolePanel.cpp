#include <Editor/Panels/ConsolePanel.hpp>
#include <Engine/Debug/EditorConsoleSink.hpp>

#include <imgui.h>


namespace Antelope::Editor
{
    void ConsolePanel::OnUIRender()
    {
        ImGui::Begin("Console");

        for (const auto& msg : EditorConsoleSink::s_Messages)
        {
            if (msg.LoggerName == "ENGINE") {
                ImGui::TextColored(ImVec4(0.88f, 0.71f, 0.22f, 1.0f), "[ENGINE]");
            } else {
                ImGui::TextColored(ImVec4(0.35f, 0.68f, 0.95f, 1.0f), "[CLIENT]");
            }

            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.38f, 0.37f, 0.50f, 1.0f), ":");
            ImGui::SameLine();

            ImVec4 color { 0.78f, 0.77f, 0.86f, 1.0f };
            if (msg.Level == spdlog::level::trace) { color = ImVec4(0.40f, 0.39f, 0.52f, 1.0f); }
            else if (msg.Level == spdlog::level::info) { color = ImVec4(0.35f, 0.78f, 0.45f, 1.0f); }
            else if (msg.Level == spdlog::level::warn) { color = ImVec4(0.88f, 0.71f, 0.22f, 1.0f); }
            else if (msg.Level == spdlog::level::err) { color = ImVec4(0.92f, 0.30f, 0.28f, 1.0f); }
            else if (msg.Level == spdlog::level::critical) { color = ImVec4(0.92f, 0.30f, 0.28f, 1.0f); }

            ImGui::TextColored(color, "%s", msg.Payload.c_str());
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) { ImGui::SetScrollHereY(1.0f); }

        ImGui::End();
    }
}
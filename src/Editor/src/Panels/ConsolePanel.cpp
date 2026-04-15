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
            ImGuiIO& io { ImGui::GetIO() };
            ImFont* boldFont { io.Fonts->Fonts[1] };

            ImGui::PushFont(boldFont);

            if (msg.LoggerName == "ENGINE") {
                ImGui::TextColored(ImVec4(0.792f, 0.584f, 0.000f, 1.0f), "[ENGINE]");
            } else {
                ImGui::TextColored(ImVec4(0.329f, 0.769f, 0.910f, 1.0f), "[CLIENT]");
            }
            
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.38f, 0.37f, 0.50f, 1.0f), ":");
            ImGui::SameLine();

            ImVec4 color { 0.78f, 0.77f, 0.86f, 1.0f };
            bool isCritical { false };

            if (msg.Level == spdlog::level::trace) { color = ImVec4(0.776f, 0.773f, 0.867f, 1.0f); }
            else if (msg.Level == spdlog::level::info) { color = ImVec4(0.271f, 0.663f, 0.514f, 1.0f); }
            else if (msg.Level == spdlog::level::warn) { color = ImVec4(0.792f, 0.584f, 0.000f, 1.0f); }
            else if (msg.Level == spdlog::level::err) { color = ImVec4(0.933f, 0.239f, 0.000f, 1.0f); }
            else if (msg.Level == spdlog::level::critical) 
            { 
                color = ImVec4(0.05f, 0.05f, 0.05f, 1.0f); 
                isCritical = true;
            }

            if (isCritical)
            {
                ImVec2 pos { ImGui::GetCursorScreenPos() };
                
                ImVec2 textSize { ImGui::CalcTextSize(msg.Payload.c_str()) };
                
                ImVec2 rectMin { ImVec2(pos.x - 2.0f, pos.y - 1.0f) };
                ImVec2 rectMax { ImVec2(pos.x + textSize.x + 2.0f, pos.y + textSize.y + 1.0f) };
                
                ImU32 bgColor { ImGui::ColorConvertFloat4ToU32(ImVec4(0.92f, 0.30f, 0.28f, 1.0f)) };
                
                ImGui::GetWindowDrawList()->AddRectFilled(rectMin, rectMax, bgColor);
            }

            ImGui::TextColored(color, "%s", msg.Payload.c_str());
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) { ImGui::SetScrollHereY(1.0f); }

        ImGui::End();
    }
}
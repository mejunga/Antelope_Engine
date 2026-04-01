#include <Editor/Panels/ConsolePanel.hpp>

#include <imgui.h>


namespace Antelope::Editor
{
    void ConsolePanel::OnUIRender()
    {
        ImGui::Begin("Console");
        ImGui::End();
    }
}

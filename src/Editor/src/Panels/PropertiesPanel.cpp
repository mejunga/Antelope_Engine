#include <Editor/Panels/PropertiesPanel.hpp>

#include <imgui.h>
#include <entt/entt.hpp>


namespace Antelope::Editor
{
    void PropertiesPanel::OnUIRender(Entity selectedEntity)
    {
        ImGui::Begin("Properties");
        
        if (selectedEntity)
        {
            uint32_t id { static_cast<uint32_t>(static_cast<entt::entity>(selectedEntity)) };
            ImGui::Text("Object ID: %d", id);
        }

        ImGui::End();
    }
}
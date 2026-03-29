#include <Editor/Panels/SceneViewPanel.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/UI/UIContext.hpp>
#include <Engine/Debug/Log.hpp>

namespace Antelope
{
    void SceneViewportPanel::OnUIRender()
    {
        ImGui::Begin("Scene");
        ImVec2 viewportSize { ImGui::GetContentRegionAvail() };
        
        uint32_t newWidth { static_cast<uint32_t>(viewportSize.x) };
        uint32_t newHeight { static_cast<uint32_t>(viewportSize.y) };
        uint32_t oldWidth { static_cast<uint32_t>(m_LastPanelSize.x) };
        uint32_t oldHeight { static_cast<uint32_t>(m_LastPanelSize.y) };

        if (newWidth > 0 && newHeight > 0 && (newWidth != oldWidth || newHeight != oldHeight))
        {
            Application::Get().GetRenderer()->ResizeRenderTexture(newWidth, newHeight);
            Application::Get().GetUIContext()->UpdateSceneTextureID(); 
            
            m_LastPanelSize = viewportSize;
            m_PanelResizeTimer = 0.2f;
            m_PanelResizing = true;
        }

        if (m_PanelResizing)
        {
            m_PanelResizeTimer -= ImGui::GetIO().DeltaTime;
            
            if (m_PanelResizeTimer <= 0.0f)
            {
                AE_CLIENT_TRACE("Scene Panel resized to: {0}x{1}", newWidth, newHeight);
                m_PanelResizing = false;
            }
        }

        void* textureID { Application::Get().GetUIContext()->GetSceneTextureID() };
        
        if (textureID)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(textureID), viewportSize);
        }
        
        ImGui::End();
    }
}
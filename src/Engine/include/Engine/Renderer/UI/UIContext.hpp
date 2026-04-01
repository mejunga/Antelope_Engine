#pragma once

#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/UI/EditorTheme.hpp>

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>


namespace Antelope
{
    class VulkanContext;
    class SwapChain;
    class Renderer;
    class Window;

    class UIContext
    {
        public:
            UIContext(std::shared_ptr<VulkanContext> context, std::shared_ptr<SwapChain> swapChain, std::shared_ptr<Renderer> renderer, Window& window);
            ~UIContext();

            void BeginFrame();
            void EndFrame();
            void RecordCommands(VkCommandBuffer cmdBuffer, uint32_t imageIndex);
            void RenderViewports();
            void UpdateSceneTextureID();
            void ApplyTheme(const EditorThemeProps& props);

            inline void* GetSceneTextureID() const { return m_SceneTexture; }

        private:
            void InitImGui();

        private:
            std::shared_ptr<VulkanContext> m_Context;
            std::shared_ptr<SwapChain> m_SwapChain;
            std::shared_ptr<Renderer> m_Renderer;
            void* m_SceneTexture { nullptr };

            VkDescriptorPool m_ImGuiPool { VK_NULL_HANDLE };
            VkDescriptorSet m_SceneTextureDescriptorSet { VK_NULL_HANDLE };
            
            Window& m_Window;
    };
}
#endif
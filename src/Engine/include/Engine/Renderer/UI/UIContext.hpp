#pragma once

#ifdef ANTELOPE_EDITOR_MODE
#include <vulkan/vulkan.h>

#include <memory>

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

            void* GetSceneTextureID() const { return m_SceneTexture; }

        private:
            void InitImGui();

        private:
            std::shared_ptr<VulkanContext> m_Context;
            std::shared_ptr<SwapChain> m_SwapChain;
            std::shared_ptr<Renderer> m_Renderer;
            Window& m_Window;

            VkDescriptorPool m_ImGuiPool { VK_NULL_HANDLE };
            void* m_SceneTexture { nullptr };
    };
}
#endif
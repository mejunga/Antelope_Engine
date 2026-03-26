#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <memory>


struct GLFWwindow;

namespace Antelope
{
    class VulkanContext;
    
    class SwapChain
    {
        public:
            SwapChain(std::shared_ptr<VulkanContext> context);
            ~SwapChain();

            void RecreateSwapchain();

            VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
            VkRenderPass GetRenderPass() const { return m_RenderPass; }
            VkExtent2D GetExtent() const { return m_SwapchainExtent; }
            VkFormat GetImageFormat() const { return m_SwapchainImageFormat; }
            std::vector<VkFramebuffer>& GetFramebuffers() { return m_SwapchainFramebuffers; }
            bool IsFramebufferResized() const { return m_FramebufferResized; }
            void SetFramebufferResized(bool resized) { m_FramebufferResized = resized; }

        private:
            VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
            VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
            VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* windowHandle);
            VkFormat FindDepthFormat();
            void CleanupSwapchain();
            void RecreateSwapChain();

            void CreateSwapchain();
            void CreateImageViews();
            void CreateDepthResources();
            void CreateRenderPass();
            void CreateFramebuffers();
            void CreateColorResources();

        private:
            std::shared_ptr<VulkanContext> m_Context;
            
            VkImage m_DepthImage { VK_NULL_HANDLE };
            VmaAllocation m_DepthImageAllocation { VK_NULL_HANDLE };
            VkImageView m_DepthImageView { VK_NULL_HANDLE };
            VkSwapchainKHR m_Swapchain { VK_NULL_HANDLE };
            VkRenderPass m_RenderPass { VK_NULL_HANDLE };
            VkImage m_ColorImage { VK_NULL_HANDLE };
            VmaAllocation m_ColorImageAllocation { VK_NULL_HANDLE };
            VkImageView m_ColorImageView { VK_NULL_HANDLE };

            bool m_FramebufferResized { false };
            VkFormat m_DepthFormat { VK_FORMAT_UNDEFINED };
            VkFormat m_SwapchainImageFormat { VK_FORMAT_UNDEFINED };
            VkExtent2D m_SwapchainExtent { 0, 0 };

            std::vector<VkImage> m_SwapchainImages;
            std::vector<VkImageView> m_SwapchainImageViews;
            std::vector<VkFramebuffer> m_SwapchainFramebuffers;
    };
}
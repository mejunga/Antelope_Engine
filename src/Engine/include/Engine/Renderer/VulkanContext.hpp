#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

struct GLFWwindow; 

namespace Antelope
{
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> GraphicsFamily;
        std::optional<uint32_t> PresentFamily;

        bool IsComplete() const { return GraphicsFamily.has_value() && PresentFamily.has_value(); }
    };

    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

    class VulkanContext
    {
        public:
            VulkanContext();
            ~VulkanContext();

            void Init(GLFWwindow *windowHandle);

        private:
            bool CheckValidationLayerSupport();
            static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData
            );
            void SetupDebugMessenger();

            bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
            QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
            SwapchainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
            bool IsDeviceSuitable(VkPhysicalDevice device);
            void PickPhysicalDevice();

            VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
            VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
            VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* windowHandle);
            void CreateSwapchain();

            void CreateLogicalDevice();

        private:
            VkInstance m_Instance { VK_NULL_HANDLE };
            VkDebugUtilsMessengerEXT m_DebugMessenger { VK_NULL_HANDLE };
            VkSurfaceKHR m_Surface { VK_NULL_HANDLE };
            VkPhysicalDevice m_PhysicalDevice { VK_NULL_HANDLE };
            VkDevice m_Device { VK_NULL_HANDLE };
            VkQueue m_GraphicsQueue { VK_NULL_HANDLE };
            VkQueue m_PresentQueue { VK_NULL_HANDLE };
            VkSwapchainKHR m_Swapchain { VK_NULL_HANDLE };

            VkFormat m_SwapchainImageFormat;
            VkExtent2D m_SwapchainExtent;
            GLFWwindow *m_WindowHandle { nullptr };

            const std::vector<const char*> m_ValidationLayers { "VK_LAYER_KHRONOS_validation" };
            const std::vector<const char*> m_DeviceExtensions { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
            std::vector<VkImage> m_SwapchainImages;

        #ifdef NDEBUG
            const bool m_EnableValidationLayers { false };
        #else
            const bool m_EnableValidationLayers { true };
        #endif
    };
}
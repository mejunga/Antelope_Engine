#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <optional>


struct GLFWwindow; 

namespace Antelope
{
    struct QueueFamilyIndices {
        std::optional<uint32_t> GraphicsFamily;
        std::optional<uint32_t> PresentFamily;
        bool IsComplete() const { return GraphicsFamily.has_value() && PresentFamily.has_value(); }
    };

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

    class VulkanContext
    {
        public:
            VulkanContext(GLFWwindow *windowHandle);
            ~VulkanContext();

            VkDevice GetDevice() const { return m_Device; }
            VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
            VkSurfaceKHR GetSurface() const { return m_Surface; }
            VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
            VkQueue GetPresentQueue() const { return m_PresentQueue; }
            VmaAllocator GetAllocator() const { return m_Allocator; }
            GLFWwindow* GetWindowHandle() const { return m_WindowHandle; }
            VkSampleCountFlagBits GetMsaaSamples() const { return m_MsaaSamples; }

            SwapchainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
            QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
            VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

        private:
            bool CheckValidationLayerSupport();
            bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
            bool IsDeviceSuitable(VkPhysicalDevice device);
            static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData);
            VkSampleCountFlagBits GetMaxUsableSampleCount();

            void CreateInstance();
            void SetupDebugMessenger();
            void CreateSurface();
            void PickPhysicalDevice();
            void CreateLogicalDevice();
            void CreateMemoryAllocator();

        private:
            GLFWwindow *m_WindowHandle { nullptr };
            VkInstance m_Instance { VK_NULL_HANDLE };
            VkDebugUtilsMessengerEXT m_DebugMessenger { VK_NULL_HANDLE };
            VkSurfaceKHR m_Surface { VK_NULL_HANDLE };
            VkPhysicalDevice m_PhysicalDevice { VK_NULL_HANDLE };
            VkDevice m_Device { VK_NULL_HANDLE };
            VkQueue m_GraphicsQueue { VK_NULL_HANDLE };
            VkQueue m_PresentQueue { VK_NULL_HANDLE };
            VmaAllocator m_Allocator { VK_NULL_HANDLE };

            VkSampleCountFlagBits m_MsaaSamples { VK_SAMPLE_COUNT_1_BIT };

        #ifdef NDEBUG
            bool m_EnableValidationLayers { false };
        #else
            bool m_EnableValidationLayers { true };
        #endif

            const std::vector<const char*> m_ValidationLayers { "VK_LAYER_KHRONOS_validation" };
            const std::vector<const char*> m_DeviceExtensions { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    };
}
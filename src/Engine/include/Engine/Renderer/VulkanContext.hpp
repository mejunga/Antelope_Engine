#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <set>

struct GLFWwindow; 

namespace Antelope
{
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> GraphicsFamily;
        std::optional<uint32_t> PresentFamily;

        bool IsComplete() const { return GraphicsFamily.has_value() && PresentFamily.has_value(); }
    };

    class VulkanContext
    {
        public:
            VulkanContext();
            ~VulkanContext();

            void Init(GLFWwindow *windowHandle);

        private:
            bool CheckValidationLayerSupport();
            void SetupDebugMessenger();
            void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
            void PickPhysicalDevice();
            void CreateLogicalDevice();
            bool IsDeviceSuitable(VkPhysicalDevice device);
            QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
            static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData
            );

            VkInstance m_Instance { VK_NULL_HANDLE };
            VkDebugUtilsMessengerEXT m_DebugMessenger { VK_NULL_HANDLE };
            VkSurfaceKHR m_Surface { VK_NULL_HANDLE };
            VkPhysicalDevice m_PhysicalDevice { VK_NULL_HANDLE };
            VkDevice m_Device { VK_NULL_HANDLE };
            VkQueue m_GraphicsQueue { VK_NULL_HANDLE };
            VkQueue m_PresentQueue { VK_NULL_HANDLE };

            const std::vector<const char*> m_ValidationLayers { "VK_LAYER_KHRONOS_validation" };
            const std::vector<const char*> m_DeviceExtensions { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        #ifdef NDEBUG
            const bool m_EnableValidationLayers { false };
        #else
            const bool m_EnableValidationLayers { true };
        #endif
    };
}
#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Antelope
{
    class VulkanContext
    {
        public:
            VulkanContext();
            ~VulkanContext();

            void Init();

        private:
            bool CheckValidationLayerSupport();
            void SetupDebugMessenger();
            void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
            static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData
            );

            VkInstance m_Instance { VK_NULL_HANDLE };
            VkDebugUtilsMessengerEXT m_DebugMessenger { VK_NULL_HANDLE };
            const std::vector<const char*> m_ValidationLayers { "VK_LAYER_KHRONOS_validation" };

        #ifdef NDEBUG
            const bool m_EnableValidationLayers { false };
        #else
            const bool m_EnableValidationLayers { true };
        #endif
    };
}
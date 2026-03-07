#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include <array>

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

    struct Vertex
    {
        glm::vec2 pos;
        glm::vec3 color;
    };

    class VulkanContext
    {
        public:
            VulkanContext();
            ~VulkanContext();

            void Init(GLFWwindow *windowHandle);
            void DrawFrame();

        private:
            bool CheckValidationLayerSupport();
            static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData
            );

            bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
            QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
            SwapchainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
            bool IsDeviceSuitable(VkPhysicalDevice device);

            VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
            VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
            VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* windowHandle);

            static std::vector<char> ReadFile(const std::string& fileName);
            VkShaderModule CreateShaderModule(const std::vector<char>& code);

            static VkVertexInputBindingDescription GetBindingDescription();
            static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
            uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
            
            void CreateInstance();
            void SetupDebugMessenger();
            void CreateSurface();
            void PickPhysicalDevice();
            void CreateLogicalDevice();
            void CreateSwapchain();
            void CreateImageViews();
            void CreateRenderPass();
            void CreateGraphicsPipeline();
            void CreateFramebuffers();
            void CreateCommandPool();
            void CreateCommandBuffer();
            void CreateSyncObjects();
            void CreateVertexBuffer();
            
        private:
            const int MAX_FRAMES_IN_FLIGHT = 3;
            uint32_t m_CurrentFrame = 0;

            VkInstance m_Instance { VK_NULL_HANDLE };
            VkDebugUtilsMessengerEXT m_DebugMessenger { VK_NULL_HANDLE };
            VkSurfaceKHR m_Surface { VK_NULL_HANDLE };
            VkPhysicalDevice m_PhysicalDevice { VK_NULL_HANDLE };
            VkDevice m_Device { VK_NULL_HANDLE };
            VkQueue m_GraphicsQueue { VK_NULL_HANDLE };
            VkQueue m_PresentQueue { VK_NULL_HANDLE };
            VkSwapchainKHR m_Swapchain { VK_NULL_HANDLE };
            VkRenderPass m_RenderPass { VK_NULL_HANDLE };
            VkPipelineLayout m_PipelineLayout { VK_NULL_HANDLE };
            VkPipeline m_GraphicsPipeline { VK_NULL_HANDLE };
            VkCommandPool m_CommandPool { VK_NULL_HANDLE };
            VkBuffer m_VertexBuffer { VK_NULL_HANDLE };
            VkDeviceMemory m_VertexBufferMemory { VK_NULL_HANDLE };
            
            VkFormat m_SwapchainImageFormat { VK_FORMAT_UNDEFINED };
            VkExtent2D m_SwapchainExtent { 0, 0 };
            GLFWwindow *m_WindowHandle { nullptr };

            const std::vector<const char*> m_ValidationLayers { "VK_LAYER_KHRONOS_validation" };
            const std::vector<const char*> m_DeviceExtensions { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
            std::vector<VkImage> m_SwapchainImages;
            std::vector<VkImageView> m_SwapchainImageViews;
            std::vector<VkFramebuffer> m_SwapchainFramebuffers;
            std::vector<VkSemaphore> m_ImageAvailableSemaphores;
            std::vector<VkSemaphore> m_RenderFinishedSemaphores;
            std::vector<VkFence> m_InFlightFences;
            std::vector<VkCommandBuffer> m_CommandBuffers;

        #ifdef NDEBUG
            const bool m_EnableValidationLayers { false };
        #else
            const bool m_EnableValidationLayers { true };
        #endif
    };
}
#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <vector>
#include <optional>
#include <chrono>


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
        glm::vec3 pos;
        glm::vec3 color;
    };

    struct UniformBufferObject
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
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

            void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize);
            void CreateStagingBuffer(const void* data, VkDeviceSize size, VkBuffer& outBuffer, VmaAllocation& outAllocation);
            void UpdateUniformBuffer(uint32_t currentImage);
            
            void CreateInstance();
            void SetupDebugMessenger();
            void CreateSurface();
            void PickPhysicalDevice();
            void CreateLogicalDevice();
            void CreateMemoryAllocator();
            void CreateSwapchain();
            void CreateImageViews();
            void CreateRenderPass();
            void CreateDescriptorSetLayout();
            void CreateGraphicsPipeline();
            void CreateFramebuffers();
            void CreateCommandPool();
            void CreateCommandBuffers();
            void CreateSyncObjects();
            void CreateVertexBuffer();
            void CreateIndexBuffer();
            void CreateUniformBuffers();
            void CreateDescriptorPool();
            void CreateDescriptorSets();
            
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
            VmaAllocator m_Allocator { VK_NULL_HANDLE };
            VkSwapchainKHR m_Swapchain { VK_NULL_HANDLE };
            VkRenderPass m_RenderPass { VK_NULL_HANDLE };
            VkDescriptorSetLayout m_DescriptorSetLayout { VK_NULL_HANDLE };
            VkPipelineLayout m_PipelineLayout { VK_NULL_HANDLE };
            VkPipeline m_GraphicsPipeline { VK_NULL_HANDLE };
            VkCommandPool m_CommandPool { VK_NULL_HANDLE };
            VkBuffer m_VertexBuffer { VK_NULL_HANDLE };
            VmaAllocation m_VertexBufferAllocation { VK_NULL_HANDLE };
            VkBuffer m_IndexBuffer { VK_NULL_HANDLE };
            VmaAllocation m_IndexBufferAllocation { VK_NULL_HANDLE };
            VkDescriptorPool m_DescriptorPool { VK_NULL_HANDLE };
            
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
            std::vector<VkBuffer> m_UniformBuffers;
            std::vector<VmaAllocation> m_UniformBuffersAllocations;
            std::vector<void*> m_UniformBuffersMapped;
            std::vector<VkDescriptorSet> m_DescriptorSets;

        #ifdef NDEBUG
            const bool m_EnableValidationLayers { false };
        #else
            const bool m_EnableValidationLayers { true };
        #endif
    };
}
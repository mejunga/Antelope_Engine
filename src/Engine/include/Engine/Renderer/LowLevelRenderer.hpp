#pragma once

#include "VulkanContext.hpp"
#include "SwapChain.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <array>
#include <memory>
#include <string>


namespace Antelope
{
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

    class LowLevelRenderer
    {
        public:
            LowLevelRenderer(std::shared_ptr<VulkanContext> context, std::shared_ptr<SwapChain> swapChain);
            ~LowLevelRenderer();

            void DrawFrame();

        private:
            static std::vector<char> ReadFile(const std::string& fileName);
            VkShaderModule CreateShaderModule(const std::vector<char>& code);
            static VkVertexInputBindingDescription GetBindingDescription();
            static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();

            void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize);
            void CreateStagingBuffer(const void* data, VkDeviceSize size, VkBuffer& outBuffer, VmaAllocation& outAllocation);
            void UpdateUniformBuffer(uint32_t currentImage);

            void CreateDescriptorSetLayout();
            void CreateGraphicsPipeline();
            void CreateCommandPool();
            void CreateCommandBuffers();
            void CreateSyncObjects();
            void CreateVertexBuffer();
            void CreateIndexBuffer();
            void CreateUniformBuffers();
            void CreateDescriptorPool();
            void CreateDescriptorSets();

        private:
            std::shared_ptr<VulkanContext> m_Context;
            std::shared_ptr<SwapChain> m_SwapChain;
            VkDescriptorSetLayout m_DescriptorSetLayout { VK_NULL_HANDLE };
            VkPipelineLayout m_PipelineLayout { VK_NULL_HANDLE };
            VkPipeline m_GraphicsPipeline { VK_NULL_HANDLE };
            VkCommandPool m_CommandPool { VK_NULL_HANDLE };
            VkDescriptorPool m_DescriptorPool { VK_NULL_HANDLE };
            VkBuffer m_VertexBuffer { VK_NULL_HANDLE };
            VmaAllocation m_VertexBufferAllocation { VK_NULL_HANDLE };
            VkBuffer m_IndexBuffer { VK_NULL_HANDLE };
            VmaAllocation m_IndexBufferAllocation { VK_NULL_HANDLE };

            const int MAX_FRAMES_IN_FLIGHT = 3;
            uint32_t m_CurrentFrame = 0;
            
            std::vector<VkSemaphore> m_ImageAvailableSemaphores;
            std::vector<VkSemaphore> m_RenderFinishedSemaphores;
            std::vector<VkFence> m_InFlightFences;
            std::vector<VkCommandBuffer> m_CommandBuffers;
            std::vector<VkBuffer> m_UniformBuffers;
            std::vector<VmaAllocation> m_UniformBuffersAllocations;
            std::vector<void*> m_UniformBuffersMapped;
            std::vector<VkDescriptorSet> m_DescriptorSets;
    };
}
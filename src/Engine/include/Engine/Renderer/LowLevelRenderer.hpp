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
    struct VertexPosition
    {
        alignas(16) glm::vec3 pos;
    };

    struct VertexColor
    {
        alignas(16) glm::vec3 color;
    };

    struct VertexNormal
    {
        alignas(16) glm::vec3 normal;
    };

    struct Face
    {
        uint32_t v0, v1, v2;
        uint32_t normalIndex;
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
            void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize);
            void CreateStagingBuffer(const void* data, VkDeviceSize size, VkBuffer& outBuffer, VmaAllocation& outAllocation);
            void UpdateUniformBuffer(uint32_t currentImage);

            void CreateDescriptorSetLayout();
            void CreateGraphicsPipeline();
            void CreateCommandPool();
            void CreateCommandBuffers();
            void CreateSyncObjects();
            void CreateStorageBuffers(); 
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
            VkBuffer m_PosBuffer { VK_NULL_HANDLE };
            VmaAllocation m_PosBufferAllocation { VK_NULL_HANDLE };
            VkBuffer m_ColorBuffer { VK_NULL_HANDLE };
            VmaAllocation m_ColorBufferAllocation { VK_NULL_HANDLE };
            VkBuffer m_NormalBuffer { VK_NULL_HANDLE };
            VmaAllocation m_NormalBufferAllocation { VK_NULL_HANDLE };
            VkBuffer m_FaceBuffer { VK_NULL_HANDLE };
            VmaAllocation m_FaceBufferAllocation { VK_NULL_HANDLE };

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
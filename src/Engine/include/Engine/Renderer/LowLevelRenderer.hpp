#pragma once

#include <Engine/Renderer/Mesh.hpp>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <unordered_set>
#include <vector>
#include <array>
#include <memory>
#include <string>

namespace Antelope
{
    class VulkanContext;
    class SwapChain;
    struct UniformBufferObject;

    struct PendingTransfer
    {
        VkFence fence;
        VkCommandBuffer commandBuffer;
        VkBuffer stagingBuffer;
        VmaAllocation stagingAllocation;
        uint32_t posOffset;
    };

    class LowLevelRenderer
    {
        public:
            LowLevelRenderer(std::shared_ptr<VulkanContext> context, std::shared_ptr<SwapChain> swapChain);
            ~LowLevelRenderer();

            void DrawFrame(const UniformBufferObject& cameraData, const std::vector<RenderCommand>& renderList);
            MeshHandle UploadMesh(const MeshData& meshData);

        private:
            static std::vector<char> ReadFile(const std::string& fileName);
            VkShaderModule CreateShaderModule(const std::vector<char>& code);
            void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize);
            void CreateStagingBuffer(const void* data, VkDeviceSize size, VkBuffer& outBuffer, VmaAllocation& outAllocation);
            void UpdateUniformBuffer(uint32_t currentImage, const UniformBufferObject& cameraData);
            void ProcessPendingTransfers();

            void CreateDescriptorSetLayout();
            void CreateGraphicsPipeline();
            void CreateCommandPool();
            void CreateTransferCommandPool();
            void CreateCommandBuffers();
            void CreateSyncObjects();
            void CreateStorageBuffers(); 
            void CreateUniformBuffers();
            void CreateObjectBuffers(); 
            void CreateIndirectBuffers();
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
            VkCommandPool m_TransferCommandPool { VK_NULL_HANDLE };
        
            const int MAX_FRAMES_IN_FLIGHT = 3;
            const uint32_t MAX_OBJECTS = 10000;
            uint32_t m_CurrentFrame = 0;
            uint32_t m_CurrentPosOffset = 0;
            uint32_t m_CurrentColorOffset = 0;
            uint32_t m_CurrentNormalOffset = 0;
            uint32_t m_CurrentFaceOffset = 0;

            std::vector<VkSemaphore> m_ImageAvailableSemaphores;
            std::vector<VkSemaphore> m_RenderFinishedSemaphores;
            std::vector<VkFence> m_InFlightFences;
            std::vector<VkCommandBuffer> m_CommandBuffers;
            std::vector<VkBuffer> m_UniformBuffers;
            std::vector<VmaAllocation> m_UniformBuffersAllocations;
            std::vector<void*> m_UniformBuffersMapped;
            std::vector<VkDescriptorSet> m_DescriptorSets;
            std::vector<VkBuffer> m_ObjectBuffers;
            std::vector<VmaAllocation> m_ObjectBuffersAllocations;
            std::vector<void*> m_ObjectBuffersMapped;
            std::vector<VkBuffer> m_IndirectBuffers;
            std::vector<VmaAllocation> m_IndirectBuffersAllocations;
            std::vector<void*> m_IndirectBuffersMapped;
            std::vector<PendingTransfer> m_PendingTransfers;
            std::unordered_set<uint32_t> m_PendingMeshOffsets;

    };
}
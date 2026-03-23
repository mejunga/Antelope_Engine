#pragma once

#include <Engine/Renderer/Graphics/Mesh.hpp>

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
    class VulkanBuffer;
    class VulkanPipeline;
    class DescriptorAllocator;;
    class SwapChain;
    
    struct UniformBufferObject;
    struct Texture;

    struct PendingTransfer
    {
        VkFence fence;
        VkCommandBuffer commandBuffer;
        VkBuffer stagingBuffer;
        VmaAllocation stagingAllocation;
        uint32_t posOffset;
    };

    class Renderer
    {
        public:
            Renderer(std::shared_ptr<VulkanContext> context, std::shared_ptr<SwapChain> swapChain);
            ~Renderer();

            void DrawFrame(const UniformBufferObject& cameraData, const std::vector<RenderCommand>& renderList);
            MeshHandle UploadMesh(const MeshData& meshData);
            void FreeMesh(const MeshHandle& handle);
            void UpdateTextureDescriptors(const std::vector<Texture>& textures);

        private:
            void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize);
            void CreateStagingBuffer(const void* data, VkDeviceSize size, VkBuffer& outBuffer, VmaAllocation& outAllocation);
            void UpdateUniformBuffer(uint32_t currentImage, const UniformBufferObject& cameraData);
            void ProcessPendingTransfers();
            VkCommandBuffer BeginFrame();
            void DrawObjects(VkCommandBuffer cmd, const UniformBufferObject& cameraData, const std::vector<RenderCommand>& renderList);
            void EndFrame(VkCommandBuffer cmd);

            void CreateDescriptorSetLayout();
            void CreatePipelineLayout();
            void CreateGraphicsPipeline();
            void CreateCommandPool();
            void CreateTransferCommandPool();
            void CreateCommandBuffers();
            void CreateSyncObjects();
            void CreateUniformBuffers();
            void CreateObjectBuffers(); 
            void CreateIndirectBuffers();
            void CreateDescriptorPool();
            void CreateDescriptorSets();

        private:
            std::shared_ptr<VulkanContext> m_Context;
            std::shared_ptr<SwapChain> m_SwapChain;
            std::unique_ptr<GpuMemoryAllocator> m_GpuAllocator;
            std::unique_ptr<VulkanPipeline> m_MainPipeline;
            std::unique_ptr<DescriptorAllocator> m_GlobalDescriptorAllocator;

            VkDescriptorSetLayout m_DescriptorSetLayout { VK_NULL_HANDLE };
            VkPipelineLayout m_PipelineLayout { VK_NULL_HANDLE };
            VkCommandPool m_CommandPool { VK_NULL_HANDLE };
            VkDescriptorPool m_DescriptorPool { VK_NULL_HANDLE };
            VkCommandPool m_TransferCommandPool { VK_NULL_HANDLE };
        
            const int MAX_FRAMES_IN_FLIGHT = 3;
            const uint32_t MAX_OBJECTS = 128000;
            uint32_t m_CurrentFrame = 0;
            uint32_t m_CurrentImageIndex = 0;

            std::vector<VkSemaphore> m_ImageAvailableSemaphores;
            std::vector<VkSemaphore> m_RenderFinishedSemaphores;
            std::vector<VkFence> m_InFlightFences;
            std::vector<VkCommandBuffer> m_CommandBuffers;
            std::vector<std::unique_ptr<VulkanBuffer>> m_UniformBuffers;
            std::vector<std::unique_ptr<VulkanBuffer>> m_ObjectBuffers;
            std::vector<std::unique_ptr<VulkanBuffer>> m_IndirectBuffers;
            std::vector<VkDescriptorSet> m_DescriptorSets;
            std::vector<PendingTransfer> m_PendingTransfers;
            std::unordered_set<uint32_t> m_PendingMeshOffsets;
    };
}
#pragma once

#include <Engine/Renderer/Graphics/RenderCommand.hpp>
#include <Engine/Renderer/Graphics/Material.hpp>

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
    class Buffer;
    class Pipeline;
    class DescriptorAllocator;
    class SwapChain;
    class GpuMemoryAllocator;
    class SkyboxPass;
    class ShadowPass;
    class PostCompositePass;
    class RenderTexture;
    class BloomPass;
#ifdef ANTELOPE_EDITOR_MODE
    class UIContext;
    class EditorGridPass;
    class OutlinePass;
#endif

    struct Texture;

    struct PendingTransfer
    {
        VkFence fence { VK_NULL_HANDLE };
        VkCommandBuffer commandBuffer { VK_NULL_HANDLE };
        VkBuffer stagingBuffer { VK_NULL_HANDLE };
        VmaAllocation stagingAllocation { VK_NULL_HANDLE };
        uint32_t meshID { 0 };
        uint32_t textureIndex { UINT32_MAX };
    };

    struct FrameSyncObjects
    {
        VkSemaphore imageAvailableSemaphore { VK_NULL_HANDLE };
    #ifndef ANTELOPE_EDITOR_MODE
        VkSemaphore renderFinishedSemaphore { VK_NULL_HANDLE };
    #endif
        VkFence inFlightFence { VK_NULL_HANDLE };
    };

#ifdef ANTELOPE_EDITOR_MODE
    struct SemaphoreSlot
    {
        VkSemaphore semaphore { VK_NULL_HANDLE };
        VkFence presentFence { VK_NULL_HANDLE };
        bool pendingPresent { false };
    };
#endif

    class Renderer
    {
        public:
            Renderer(std::shared_ptr<VulkanContext> context, std::shared_ptr<SwapChain> swapChain, uint32_t framesInFlight = 2);
            ~Renderer();

            void DrawFrame(const UniformBufferObject& cameraData, const std::vector<RenderCommand>& renderList);
            MeshHandle UploadMesh(const MeshData& meshData);
            void FreeMesh(const MeshHandle& handle);
            void UpdateTextureDescriptors(const std::vector<Texture>& textures);
            VkCommandBuffer BeginAsyncGraphicsCommand();
            void EndAndSubmitAsyncGraphicsCommand(VkCommandBuffer cmd, VkBuffer stagingBuffer, VmaAllocation stagingAllocation, uint32_t textureIndex = UINT32_MAX);
            void CreateStagingBuffer(const void* data, VkDeviceSize size, VkBuffer& outBuffer, VmaAllocation& outAllocation);
            uint32_t AddMaterial(const PBRMaterialData& material);
            void ClearMaterials();            
        #ifdef ANTELOPE_EDITOR_MODE
            void ResizeRenderTexture(uint32_t width, uint32_t height);
        #endif

        #ifdef ANTELOPE_EDITOR_MODE
            inline void SetUIContext(std::shared_ptr<UIContext> uiContext) { m_UIContext = uiContext; }
            inline uint32_t GetMaxFramesInFlight() const { return m_MaxFramesInFlight; }
            inline std::shared_ptr<RenderTexture> GetFinalLDRTexture() const { return m_FinalLDRTexture; }
        #endif
            inline void SetSelectedEntityIDs(std::unordered_set<uint32_t> entityIDs, glm::vec4 outlineColor = { 1.0f, 0.6f, 0.0f, 1.0f })
            {
                m_SelectedEntityIDs = std::move(entityIDs);
                m_OutlineColor = outlineColor;
            }

        private:
            void UpdateUniformBuffer(uint32_t currentImage, const UniformBufferObject& cameraData);
            void ProcessPendingTransfers();
            VkCommandBuffer BeginFrame();
            void DrawObjects(VkCommandBuffer cmd, const UniformBufferObject& cameraData, const std::vector<RenderCommand>& renderList);
            void EndFrame(VkCommandBuffer cmd);
            void DestroySyncObjects();
        #ifdef ANTELOPE_EDITOR_MODE
            VkSemaphore AcquireRenderFinishedSemaphore();
            void MarkSemaphorePending(VkSemaphore semaphore, VkFence presentFence);
        #endif

            void CreateDescriptorSetLayout();
            void CreatePipelineLayout();
            void CreateGraphicsPipeline();
            void CreateCommandPool();
            void CreateTransferCommandPool();
            void CreateCommandBuffers();
            void CreateSyncObjects();
            void CreateUniformBuffers();
            void CreateObjectBuffers();
            void CreateMaterialBuffers();
            void CreateIndirectBuffers();
            void CreateDescriptorPool();
            void CreateDescriptorSets();

        private:
            std::shared_ptr<VulkanContext> m_Context;
            std::shared_ptr<SwapChain> m_SwapChain;
            std::unique_ptr<GpuMemoryAllocator> m_GpuAllocator;
            std::unique_ptr<Pipeline> m_MainPipeline;
            std::unique_ptr<SkyboxPass> m_SkyBoxPass;
            std::shared_ptr<ShadowPass> m_ShadowPass;
            std::unique_ptr<DescriptorAllocator> m_GlobalDescriptorAllocator;
            std::shared_ptr<RenderTexture> m_SceneHDRTexture; 
            std::unique_ptr<PostCompositePass> m_PostCompositePass;
            std::unique_ptr<BloomPass> m_BloomPass;
        #ifdef ANTELOPE_EDITOR_MODE
            std::weak_ptr<UIContext> m_UIContext;
            std::unique_ptr<EditorGridPass> m_EditorGridPass;
            std::unique_ptr<OutlinePass> m_OutlinePass;
            std::shared_ptr<RenderTexture> m_FinalLDRTexture;
        #endif

            VkDescriptorSetLayout m_DescriptorSetLayout { VK_NULL_HANDLE };
            VkPipelineLayout m_PipelineLayout { VK_NULL_HANDLE };
            VkCommandPool m_CommandPool { VK_NULL_HANDLE };
            VkDescriptorPool m_DescriptorPool { VK_NULL_HANDLE };
            VkCommandPool m_TransferCommandPool { VK_NULL_HANDLE };

            static constexpr uint32_t MAX_OBJECTS { 128000 };
            uint32_t m_MaxFramesInFlight;
            uint32_t m_CurrentFrame { 0 };
            uint32_t m_CurrentImageIndex { 0 };
            uint32_t m_NextMeshID { 1 };
            glm::vec4 m_OutlineColor { 1.0f, 0.6f, 0.0f, 1.0f };
        #ifdef ANTELOPE_EDITOR_MODE
            bool m_Maintenance1Supported { false };
        #endif

            std::vector<VkCommandBuffer> m_CommandBuffers;
            std::vector<std::unique_ptr<Buffer>> m_UniformBuffers;
            std::vector<std::unique_ptr<Buffer>> m_ObjectBuffers;
            std::vector<std::unique_ptr<Buffer>> m_MaterialBuffers;
            std::vector<std::unique_ptr<Buffer>> m_IndirectBuffers;
            std::vector<VkDescriptorSet> m_DescriptorSets;
            std::vector<PendingTransfer> m_PendingTransfers;
            std::vector<VkDescriptorImageInfo> m_GlobalImageInfos;
            std::vector<PBRMaterialData> m_GlobalMaterials;
            std::vector<uint32_t> m_LastUpdatedTextureCount;
            std::unordered_set<uint32_t> m_PendingMeshIDs;
            std::unordered_set<uint32_t> m_PendingTextureIndices;
            std::vector<FrameSyncObjects> m_FrameSync;
            std::unordered_set<uint32_t> m_SelectedEntityIDs;
        #ifdef ANTELOPE_EDITOR_MODE
            std::vector<SemaphoreSlot> m_SemaphorePool;
            std::vector<VkSemaphore> m_RenderFinishedRing;
        #endif

        #ifdef ANTELOPE_EDITOR_MODE
            friend class ScenePicker;
            friend class EditorGridPass;
        #endif
            friend class SkyboxPass;
    };
}
#pragma once

#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/Graphics/RenderCommand.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <entt/entt.hpp>

#include <memory>
#include <vector>
#include <optional>


namespace Antelope
{
    class VulkanContext;
    class Pipeline;
    class EditorCamera;
    class Buffer;

    class ScenePicker
    {
        public:
            ScenePicker(std::shared_ptr<VulkanContext> context, uint32_t width, uint32_t height);
            ~ScenePicker();

            void Resize(uint32_t width, uint32_t height);
            
            void SubmitPick(uint32_t x, uint32_t y, const EditorCamera& camera, const std::vector<RenderCommand>& renderList);
            std::optional<uint32_t> TryGetPickResult();

        private:
            void DestroyResources();
        
            void CreateRenderPass();
            void CreateResources(uint32_t width, uint32_t height);
            void CreatePipeline();
            void CreateStagingBuffer();
            void CreateReadBackFence();
            void CreatePickingBuffers();

        private:
            std::shared_ptr<VulkanContext> m_Context;
            std::unique_ptr<Pipeline> m_Pipeline;
            std::unique_ptr<Buffer> m_PickingObjectBuffer;
            std::unique_ptr<Buffer> m_PickingIndirectBuffer;
            
            VkRenderPass m_RenderPass { VK_NULL_HANDLE };
            VkFramebuffer m_Framebuffer { VK_NULL_HANDLE };
            VkImage m_Image { VK_NULL_HANDLE };
            VmaAllocation m_ImageAlloc { VK_NULL_HANDLE };
            VkImageView m_ImageView { VK_NULL_HANDLE };
            VkImage m_DepthImage { VK_NULL_HANDLE };
            VmaAllocation m_DepthAlloc { VK_NULL_HANDLE };
            VkImageView m_DepthImageView { VK_NULL_HANDLE };
            VkBuffer m_StagingBuffer { VK_NULL_HANDLE };
            VmaAllocation m_StagingAlloc { VK_NULL_HANDLE };
            VkFence m_ReadbackFence { VK_NULL_HANDLE };
            VkCommandBuffer m_PendingCmd { VK_NULL_HANDLE };
            VkPipelineLayout m_PickingPipelineLayout { VK_NULL_HANDLE };
            VkDescriptorSet m_PickingDescriptorSet { VK_NULL_HANDLE };
            
            bool m_ReadbackPending { false };
            uint32_t m_Width;
            uint32_t m_Height;
    };
}
#endif
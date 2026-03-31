#pragma once

#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/Pipeline.hpp>
#include <Engine/Renderer/Graphics/Mesh.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <entt/entt.hpp>
#include <memory>
#include <vector>

namespace Antelope
{
    class ScenePicker
    {
        public:
            ScenePicker(std::shared_ptr<VulkanContext> context, uint32_t width, uint32_t height);
            ~ScenePicker();

            void Resize(uint32_t width, uint32_t height);
            
            uint32_t GetEntityIDAtPixel(uint32_t x, uint32_t y, const EditorCamera& camera, const std::vector<RenderCommand>& renderList);

        private:
            void CreateRenderPass();
            void CreateResources(uint32_t width, uint32_t height);
            void DestroyResources();
            void CreatePipeline();
            void CreateStagingBuffer();

        private:
            std::shared_ptr<VulkanContext> m_Context;
            uint32_t m_Width;
            uint32_t m_Height;

            VkRenderPass m_RenderPass { VK_NULL_HANDLE };
            VkFramebuffer m_Framebuffer { VK_NULL_HANDLE };
            std::unique_ptr<Pipeline> m_Pipeline;

            VkImage m_Image { VK_NULL_HANDLE };
            VmaAllocation m_ImageAlloc { VK_NULL_HANDLE };
            VkImageView m_ImageView { VK_NULL_HANDLE };

            VkImage m_DepthImage { VK_NULL_HANDLE };
            VmaAllocation m_DepthAlloc { VK_NULL_HANDLE };
            VkImageView m_DepthImageView { VK_NULL_HANDLE };

            VkBuffer m_StagingBuffer { VK_NULL_HANDLE };
            VmaAllocation m_StagingAlloc { VK_NULL_HANDLE };
    };
}
#endif
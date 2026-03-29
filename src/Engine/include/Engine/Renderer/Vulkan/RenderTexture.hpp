#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <memory>


namespace Antelope
{
    class VulkanContext;

    class RenderTexture
    {
        public:
            RenderTexture(std::shared_ptr<VulkanContext> context, uint32_t width, uint32_t height, VkFormat colorFormat);
            ~RenderTexture();

            void Resize(uint32_t width, uint32_t height);

            VkRenderPass GetRenderPass() const { return m_RenderPass; }
            VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
            VkImageView GetResolveImageView() const { return m_ResolveImageView; }
            VkSampler GetSampler() const { return m_Sampler; }
            VkExtent2D GetExtent() const { return { m_Width, m_Height }; }

        private:
            void CreateResources();
            void DestroyResources();
            void CreateRenderPass();
            void CreateFramebuffer();
            void CreateSampler();
            VkFormat FindDepthFormat();

        private:
            std::shared_ptr<VulkanContext> m_Context;

            VkImage m_ColorImage { VK_NULL_HANDLE };
            VmaAllocation m_ColorAllocation { VK_NULL_HANDLE };
            VkImageView m_ColorImageView { VK_NULL_HANDLE };
            VkImage m_DepthImage { VK_NULL_HANDLE };
            VmaAllocation m_DepthAllocation { VK_NULL_HANDLE };
            VkImageView m_DepthImageView { VK_NULL_HANDLE };
            VkImage m_ResolveImage { VK_NULL_HANDLE };
            VmaAllocation m_ResolveAllocation { VK_NULL_HANDLE };
            VkImageView m_ResolveImageView { VK_NULL_HANDLE };
            VkSampler m_Sampler { VK_NULL_HANDLE };
            VkRenderPass m_RenderPass { VK_NULL_HANDLE };
            VkFramebuffer m_Framebuffer { VK_NULL_HANDLE };

            uint32_t m_Width;
            uint32_t m_Height;
            VkFormat m_ColorFormat;
            VkFormat m_DepthFormat;
    };
}
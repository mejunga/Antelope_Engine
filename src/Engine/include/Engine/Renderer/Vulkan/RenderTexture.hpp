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

            inline VkRenderPass GetRenderPass() const { return m_RenderPass; }
            inline VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
            inline VkImageView GetResolveImageView() const { return m_ResolveImageView; }
            inline VkSampler GetSampler() const { return m_Sampler; }
            inline VkExtent2D GetExtent() const { return { m_Width, m_Height }; }
            inline VkImageView GetMaskImageView() const { return m_MaskImageView; }
            inline VkFormat GetColorFormat() const { return m_ColorFormat; }


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
            VkImage m_MaskImage { VK_NULL_HANDLE };
            VmaAllocation m_MaskAllocation { VK_NULL_HANDLE };
            VkImageView m_MaskImageView { VK_NULL_HANDLE };


            uint32_t m_Width;
            uint32_t m_Height;
            VkFormat m_ColorFormat;
            VkFormat m_DepthFormat;
    };
}
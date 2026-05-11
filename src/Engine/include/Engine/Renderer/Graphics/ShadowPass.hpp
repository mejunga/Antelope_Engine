#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <memory>


namespace Antelope
{
    class VulkanContext;
    class Pipeline;

    class ShadowPass
    {
    public:
        ShadowPass(std::shared_ptr<VulkanContext> context, VkPipelineLayout pipelineLayout, uint32_t width, uint32_t height);
        ~ShadowPass();

        void Draw(VkCommandBuffer cmd, VkDescriptorSet descriptorSet, uint32_t objectCount, VkBuffer indirectBuffer);

        inline VkRenderPass GetRenderPass() const { return m_RenderPass; }
        inline VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
        inline VkImageView GetDepthImageView() const { return m_DepthImageView; }
        inline VkSampler GetSampler() const { return m_Sampler; }
        inline VkExtent2D GetExtent() const { return { m_Width, m_Height }; }
        inline VkImage GetDepthImage() const { return m_DepthImage; }

    private:
        VkFormat FindDepthFormat();

        void CreateResources();
        void CreateRenderPass();
        void CreateFramebuffer();
        void CreateSampler();
        void CreatePipeline();

        std::shared_ptr<VulkanContext> m_Context;
        std::unique_ptr<Pipeline> m_Pipeline;

        VkImage m_DepthImage { VK_NULL_HANDLE };
        VmaAllocation m_DepthAllocation { VK_NULL_HANDLE };
        VkImageView m_DepthImageView { VK_NULL_HANDLE };
        VkRenderPass m_RenderPass { VK_NULL_HANDLE };
        VkFramebuffer m_Framebuffer { VK_NULL_HANDLE };
        VkSampler m_Sampler { VK_NULL_HANDLE };
        VkPipelineLayout m_PipelineLayout { VK_NULL_HANDLE };

        uint32_t m_Width;
        uint32_t m_Height;
        VkFormat m_DepthFormat;
    };
}
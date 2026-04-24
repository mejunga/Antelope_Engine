#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <memory>


namespace Antelope
{
    class VulkanContext;
    class Pipeline;
    class RenderTexture;

    class PostCompositePass
    {
    public:
        PostCompositePass(std::shared_ptr<VulkanContext> context, std::shared_ptr<RenderTexture> sceneTexture, std::shared_ptr<RenderTexture> bloomTexture, std::shared_ptr<RenderTexture> flareTexture, VkRenderPass renderPass);
        ~PostCompositePass();

        void Draw(VkCommandBuffer cmd, float time, float exposure = 1.0f);
        void UpdateDescriptorSet(std::shared_ptr<RenderTexture> sceneTexture, std::shared_ptr<RenderTexture> bloomTexture, std::shared_ptr<RenderTexture> flareTexture);
        void SetLUT(VkImageView view, VkSampler sampler);

    private:
        void CreateDescriptorLayout();
        void CreatePipeline(VkRenderPass renderPass);
        void CreateIdentityLUT();

    private:
        std::shared_ptr<VulkanContext> m_Context;
        std::unique_ptr<Pipeline> m_Pipeline;
        
        VkDescriptorSetLayout m_DescriptorSetLayout { VK_NULL_HANDLE };
        VkDescriptorPool m_DescriptorPool { VK_NULL_HANDLE };
        VkDescriptorSet m_DescriptorSet { VK_NULL_HANDLE };
        VkPipelineLayout m_PipelineLayout { VK_NULL_HANDLE };
        VkImage m_LUTImage { VK_NULL_HANDLE };
        VmaAllocation m_LUTAllocation { VK_NULL_HANDLE };
        VkImageView m_LUTImageView { VK_NULL_HANDLE };
        VkSampler m_LUTSampler { VK_NULL_HANDLE };
    };
}
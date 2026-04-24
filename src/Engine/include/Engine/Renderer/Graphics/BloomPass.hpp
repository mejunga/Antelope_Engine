#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>


namespace Antelope
{
    class VulkanContext;
    class Pipeline;
    class RenderTexture;

    class BloomPass
    {
    public:
        BloomPass(std::shared_ptr<VulkanContext> context, std::shared_ptr<RenderTexture> sceneTexture, uint32_t width, uint32_t height);
        ~BloomPass();

        void Resize(uint32_t width, uint32_t height, std::shared_ptr<RenderTexture> sceneTexture);
        void Draw(VkCommandBuffer cmd, std::shared_ptr<RenderTexture> sceneTexture, float threshold = 1.0f, float knee = 0.1f);

        std::shared_ptr<RenderTexture> GetBloomTexture() const { return m_UpChain[0]; }
        std::shared_ptr<RenderTexture> GetFlareTexture() const { return m_FlareTexture; }

    private:
        void CreateMipChain(uint32_t width, uint32_t height);
        void DestroyMipChain();
        void CreateDescriptorLayout();
        void CreatePipelines();
        void UpdateDescriptorSets(std::shared_ptr<RenderTexture> sceneTexture);

    private:
        std::shared_ptr<VulkanContext> m_Context;
        std::unique_ptr<Pipeline> m_DownsamplePipeline;
        std::unique_ptr<Pipeline> m_UpsamplePipeline;
        std::shared_ptr<RenderTexture> m_FlareTexture;
        std::unique_ptr<Pipeline> m_FlarePipeline;

        VkDescriptorSetLayout m_DownDescriptorSetLayout { VK_NULL_HANDLE };
        VkDescriptorSetLayout m_UpDescriptorSetLayout { VK_NULL_HANDLE };
        VkDescriptorSetLayout m_FlareDescriptorSetLayout { VK_NULL_HANDLE };
        VkPipelineLayout m_DownPipelineLayout { VK_NULL_HANDLE };
        VkPipelineLayout m_UpPipelineLayout { VK_NULL_HANDLE };
        VkPipelineLayout m_FlarePipelineLayout { VK_NULL_HANDLE };
        VkDescriptorSet m_SceneDescriptorSet { VK_NULL_HANDLE };
        VkDescriptorSet m_FlareDescriptorSet { VK_NULL_HANDLE };
        VkDescriptorPool m_DescriptorPool { VK_NULL_HANDLE };

        const uint32_t m_MipCount = 6;

        std::vector<std::shared_ptr<RenderTexture>> m_DownChain;
        std::vector<std::shared_ptr<RenderTexture>> m_UpChain;
        std::vector<VkDescriptorSet> m_DownDescriptorSets;
        std::vector<VkDescriptorSet> m_UpDescriptorSets;

    };
}

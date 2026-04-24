#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <vector>
#include <memory>


namespace Antelope
{
    class VulkanContext;
    class Pipeline;
    class RenderTexture;

    class OutlinePass
    {
        public:
            OutlinePass(std::shared_ptr<VulkanContext> context, VkPipelineLayout maskPipelineLayout, std::shared_ptr<RenderTexture> renderTexture);
            ~OutlinePass();

            void DrawMask(VkCommandBuffer cmd, const std::vector<uint32_t>& indirectIndices, VkBuffer indirectBuffer, VkDescriptorSet descriptorSet, glm::vec4 color);
            void DrawComposite(VkCommandBuffer cmd);
            void RebuildResources(std::shared_ptr<RenderTexture> renderTexture);

        private:
            void CreateMaskRenderPass();
            void CreateMaskPipeline();
            void CreateCompositeRenderPass(VkFormat colorFormat);
            void CreateCompositePipeline();
            void RebuildFramebuffersAndDescriptors(std::shared_ptr<RenderTexture> renderTexture);

        private:
            std::shared_ptr<VulkanContext> m_Context;
            std::unique_ptr<Pipeline> m_MaskPipeline;
            std::unique_ptr<Pipeline> m_CompositePipeline;

            VkPipelineLayout m_MaskPipelineLayout { VK_NULL_HANDLE };
            VkPipelineLayout m_CompositePipelineLayout { VK_NULL_HANDLE };
            VkDescriptorSetLayout m_CompositeSetLayout { VK_NULL_HANDLE };
            VkDescriptorPool m_CompositePool { VK_NULL_HANDLE };
            VkDescriptorSet m_CompositeDescriptorSet { VK_NULL_HANDLE };

            VkRenderPass m_MaskRenderPass { VK_NULL_HANDLE };
            VkFramebuffer m_MaskFramebuffer { VK_NULL_HANDLE };
            VkRenderPass m_CompositeRenderPass { VK_NULL_HANDLE };
            VkFramebuffer m_CompositeFramebuffer { VK_NULL_HANDLE };

            VkExtent2D m_Extent { 0, 0 };
            glm::vec4 m_ActiveOutlineColor { 1.0f, 0.6f, 0.0f, 1.0f };
    };
}

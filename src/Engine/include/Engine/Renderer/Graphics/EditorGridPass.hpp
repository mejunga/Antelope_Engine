#pragma once

#ifdef ANTELOPE_EDITOR_MODE
#include <vulkan/vulkan.h>

#include <memory>


namespace Antelope
{
    class VulkanContext;
    class Pipeline;

    class EditorGridPass
    {
        public:
            EditorGridPass(std::shared_ptr<VulkanContext> context,
                         VkPipelineLayout pipelineLayout,
                         VkRenderPass renderPass);
            ~EditorGridPass() = default;

            void Draw(VkCommandBuffer cmd, VkDescriptorSet descriptorSet);

        private:
            void CreatePipeline(VkRenderPass renderPass);

        private:
            std::shared_ptr<VulkanContext> m_Context;
            std::unique_ptr<Pipeline> m_Pipeline;
            
            VkPipelineLayout m_PipelineLayout { VK_NULL_HANDLE };
    };
}
#endif

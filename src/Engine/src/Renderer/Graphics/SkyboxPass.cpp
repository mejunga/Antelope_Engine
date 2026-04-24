#include <Engine/Renderer/Graphics/SkyboxPass.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/Pipeline.hpp>


namespace Antelope
{
    SkyboxPass::SkyboxPass(std::shared_ptr<VulkanContext> context,
                             VkPipelineLayout pipelineLayout,
                             VkRenderPass renderPass)
        : m_Context(context), m_PipelineLayout(pipelineLayout)
    {
        CreatePipeline(renderPass);
    }

    void SkyboxPass::Draw(VkCommandBuffer cmd, VkDescriptorSet descriptorSet)
    {
        m_Pipeline->Bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_PipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(cmd, 6, 1, 0, 0);
    }

    void SkyboxPass::CreatePipeline(VkRenderPass renderPass)
    {
        PipelineConfigInfo config {};
        Pipeline::DefaultPipelineConfigInfo(config, m_Context);
        config.renderPass = renderPass;
        config.pipelineLayout = m_PipelineLayout;

        config.depthStencilInfo.depthWriteEnable = VK_FALSE;
        config.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        config.colorBlendAttachment.blendEnable = VK_FALSE;

        m_Pipeline = std::make_unique<Pipeline>(m_Context,
            "Assets/Shaders/sky.vert.spv",
            "Assets/Shaders/sky.frag.spv",
            config);
    }
}
#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/Graphics/EditorGridPass.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/Pipeline.hpp>
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Core/Application.hpp>


namespace Antelope
{
    EditorGridPass::EditorGridPass(std::shared_ptr<VulkanContext> context,
                               VkPipelineLayout pipelineLayout,
                               VkRenderPass renderPass)
        : m_Context(context), m_PipelineLayout(pipelineLayout)
    {
        CreatePipeline(renderPass);
    }

    void EditorGridPass::Draw(VkCommandBuffer cmd, VkDescriptorSet descriptorSet)
    {
        m_Pipeline->Bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_PipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(cmd, 6, 1, 0, 0);
    }

    void EditorGridPass::CreatePipeline(VkRenderPass renderPass)
    {
        PipelineConfigInfo config {};
        Pipeline::DefaultPipelineConfigInfo(config, m_Context);
        config.renderPass = renderPass;
        config.pipelineLayout = m_PipelineLayout;

        config.colorBlendAttachment.blendEnable = VK_TRUE;
        config.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        config.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        config.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        config.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        config.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        config.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        config.colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT 
                                                   | VK_COLOR_COMPONENT_G_BIT 
                                                   | VK_COLOR_COMPONENT_B_BIT;
                                                     
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;

        m_Pipeline = std::make_unique<Pipeline>(m_Context,
            "Assets/Shaders/grid.vert.spv",
            "Assets/Shaders/grid.frag.spv",
            config);
    }
}
#endif

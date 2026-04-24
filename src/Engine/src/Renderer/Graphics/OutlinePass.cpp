#include <Engine/Renderer/Graphics/OutlinePass.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/Pipeline.hpp>
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Debug/Log.hpp>

#include <stdexcept>
#include <array>


namespace Antelope
{
    OutlinePass::OutlinePass(std::shared_ptr<VulkanContext> context, VkPipelineLayout maskPipelineLayout, std::shared_ptr<RenderTexture> renderTexture)
        : m_Context(context), m_MaskPipelineLayout(maskPipelineLayout)
    {
        CreateMaskRenderPass();
        CreateMaskPipeline();
        CreateCompositeRenderPass(renderTexture->GetColorFormat());
        CreateCompositePipeline();
        RebuildFramebuffersAndDescriptors(renderTexture);
    }

    OutlinePass::~OutlinePass()
    {
        auto device { m_Context->GetDevice() };

        if (m_MaskFramebuffer) { vkDestroyFramebuffer(device, m_MaskFramebuffer, nullptr); }
        if (m_CompositeFramebuffer) { vkDestroyFramebuffer(device, m_CompositeFramebuffer, nullptr); }
        if (m_CompositePool) { vkDestroyDescriptorPool(device, m_CompositePool, nullptr); }
        if (m_CompositeSetLayout) { vkDestroyDescriptorSetLayout(device, m_CompositeSetLayout, nullptr); }
        if (m_CompositePipelineLayout) { vkDestroyPipelineLayout(device, m_CompositePipelineLayout, nullptr); }
        if (m_MaskRenderPass) { vkDestroyRenderPass(device, m_MaskRenderPass, nullptr); }
        if (m_CompositeRenderPass) { vkDestroyRenderPass(device, m_CompositeRenderPass, nullptr); }
    }

    void OutlinePass::DrawMask(VkCommandBuffer cmd, const std::vector<uint32_t>& indirectIndices, VkBuffer indirectBuffer, VkDescriptorSet descriptorSet, glm::vec4 color)
    {
        m_ActiveOutlineColor = color;

        VkClearValue clearValue {};
        clearValue.color = {{ 0.0f, 0.0f, 0.0f, 0.0f }};

        VkRenderPassBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = m_MaskRenderPass;
        beginInfo.framebuffer = m_MaskFramebuffer;
        beginInfo.renderArea.offset = { 0, 0 };
        beginInfo.renderArea.extent = m_Extent;
        beginInfo.clearValueCount = 1;
        beginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_Extent.width);
        viewport.height = static_cast<float>(m_Extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset = { 0, 0 };
        scissor.extent = m_Extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        m_MaskPipeline->Bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MaskPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        for (uint32_t indirectIndex : indirectIndices)
        {
            VkDeviceSize offset { indirectIndex * sizeof(VkDrawIndirectCommand) };
            vkCmdDrawIndirect(cmd, indirectBuffer, offset, 1, sizeof(VkDrawIndirectCommand));
        }

        vkCmdEndRenderPass(cmd);
    }

    void OutlinePass::DrawComposite(VkCommandBuffer cmd)
    {
        VkRenderPassBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = m_CompositeRenderPass;
        beginInfo.framebuffer = m_CompositeFramebuffer;
        beginInfo.renderArea.offset = { 0, 0 };
        beginInfo.renderArea.extent = m_Extent;
        beginInfo.clearValueCount = 0;
        beginInfo.pClearValues = nullptr;

        vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_Extent.width);
        viewport.height = static_cast<float>(m_Extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset = { 0, 0 };
        scissor.extent = m_Extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        m_CompositePipeline->Bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CompositePipelineLayout, 0, 1, &m_CompositeDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, m_CompositePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &m_ActiveOutlineColor);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);
    }

    void OutlinePass::RebuildResources(std::shared_ptr<RenderTexture> renderTexture)
    {
        RebuildFramebuffersAndDescriptors(renderTexture);
    }

    void OutlinePass::CreateMaskRenderPass()
    {
        VkAttachmentDescription attachment {};
        attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference ref {};
        ref.attachment = 0;
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &ref;

        VkSubpassDependency dep {};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &attachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dep;

        if (vkCreateRenderPass(m_Context->GetDevice(), &renderPassInfo, nullptr, &m_MaskRenderPass) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create mask render pass!");
            throw std::runtime_error("Failed to create mask render pass");
        }
    }

    void OutlinePass::CreateMaskPipeline()
    {
        PipelineConfigInfo config {};
        Pipeline::DefaultPipelineConfigInfo(config, m_Context);
        config.renderPass = m_MaskRenderPass;
        config.pipelineLayout = m_MaskPipelineLayout;
        config.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        config.depthStencilInfo.depthTestEnable = VK_FALSE;
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;
        config.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT;

        m_MaskPipeline = std::make_unique<Pipeline>(m_Context, "Assets/Shaders/solid_mask.vert.spv", "Assets/Shaders/solid_mask.frag.spv", config);
    }

    void OutlinePass::CreateCompositeRenderPass(VkFormat colorFormat)
    {
        VkAttachmentDescription attachment {};
        attachment.format = colorFormat;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference ref {};
        ref.attachment = 0;
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &ref;

        VkSubpassDependency dependencyIn {};
        dependencyIn.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencyIn.dstSubpass = 0;
        dependencyIn.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencyIn.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; 
        dependencyIn.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencyIn.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;

        VkSubpassDependency dependencyOut {};
        dependencyOut.srcSubpass = 0;
        dependencyOut.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencyOut.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencyOut.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencyOut.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencyOut.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        std::array<VkSubpassDependency, 2> dependencies { dependencyIn, dependencyOut };

        VkRenderPassCreateInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &attachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(m_Context->GetDevice(), &renderPassInfo, nullptr, &m_CompositeRenderPass) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create composite render pass!");
            throw std::runtime_error("Failed to create composite render pass");
        }
    }

    void OutlinePass::CreateCompositePipeline()
    {
        auto device { m_Context->GetDevice() };

        VkDescriptorSetLayoutBinding samplerBinding {};
        samplerBinding.binding = 0;
        samplerBinding.descriptorCount = 1;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerBinding;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_CompositeSetLayout) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create composite descriptor set layout!");
            throw std::runtime_error("Failed to create composite descriptor set layout");
        }

        VkPushConstantRange pushRange {};
        pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(glm::vec4);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_CompositeSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_CompositePipelineLayout) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create composite pipeline layout!");
            throw std::runtime_error("Failed to create composite pipeline layout");
        }

        PipelineConfigInfo config {};
        Pipeline::DefaultPipelineConfigInfo(config, m_Context);
        config.renderPass = m_CompositeRenderPass;
        config.pipelineLayout = m_CompositePipelineLayout;
        config.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        config.depthStencilInfo.depthTestEnable = VK_FALSE;
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;

        config.colorBlendAttachment.blendEnable = VK_TRUE;
        config.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        config.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        config.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        config.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        config.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        config.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        config.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT;

        m_CompositePipeline = std::make_unique<Pipeline>(m_Context, "Assets/Shaders/outline_composite.vert.spv", "Assets/Shaders/outline_composite.frag.spv", config);
    }

    void OutlinePass::RebuildFramebuffersAndDescriptors(std::shared_ptr<RenderTexture> renderTexture)
    {
        auto device { m_Context->GetDevice() };
        m_Extent = renderTexture->GetExtent();

        if (m_MaskFramebuffer) { vkDestroyFramebuffer(device, m_MaskFramebuffer, nullptr); }
        if (m_CompositeFramebuffer) { vkDestroyFramebuffer(device, m_CompositeFramebuffer, nullptr); }
        if (m_CompositePool) { vkDestroyDescriptorPool(device, m_CompositePool, nullptr); }

        VkImageView maskView { renderTexture->GetMaskImageView() };

        VkFramebufferCreateInfo maskFbInfo {};
        maskFbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        maskFbInfo.renderPass = m_MaskRenderPass;
        maskFbInfo.attachmentCount = 1;
        maskFbInfo.pAttachments = &maskView;
        maskFbInfo.width = m_Extent.width;
        maskFbInfo.height = m_Extent.height;
        maskFbInfo.layers = 1;

        if (vkCreateFramebuffer(device, &maskFbInfo, nullptr, &m_MaskFramebuffer) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create mask framebuffer!");
            throw std::runtime_error("Failed to create mask framebuffer");
        }

        VkDescriptorPoolSize poolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_CompositePool) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create composite descriptor pool!");
            throw std::runtime_error("Failed to create composite descriptor pool");
        }

        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_CompositePool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_CompositeSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &m_CompositeDescriptorSet) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to allocate composite descriptor set!");
            throw std::runtime_error("Failed to allocate composite descriptor set");
        }

        VkDescriptorImageInfo imageInfo {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = renderTexture->GetMaskImageView();
        imageInfo.sampler = renderTexture->GetSampler();

        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_CompositeDescriptorSet;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        VkImageView resolveView { renderTexture->GetResolveImageView() };

        VkFramebufferCreateInfo compositeFbInfo {};
        compositeFbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        compositeFbInfo.renderPass = m_CompositeRenderPass;
        compositeFbInfo.attachmentCount = 1;
        compositeFbInfo.pAttachments = &resolveView;
        compositeFbInfo.width = m_Extent.width;
        compositeFbInfo.height = m_Extent.height;
        compositeFbInfo.layers = 1;

        if (vkCreateFramebuffer(device, &compositeFbInfo, nullptr, &m_CompositeFramebuffer) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create composite framebuffer!");
            throw std::runtime_error("Failed to create composite framebuffer");
        }
    }
}

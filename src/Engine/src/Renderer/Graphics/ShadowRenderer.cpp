#include <Engine/Renderer/Graphics/ShadowRenderer.hpp>
#include <Engine/Renderer/Vulkan/Pipeline.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Debug/Log.hpp>

#include <stdexcept>
#include <array>


namespace Antelope
{
    ShadowRenderer::ShadowRenderer(std::shared_ptr<VulkanContext> context, 
                                   VkPipelineLayout pipelineLayout,
                                   uint32_t width, uint32_t height)
        : m_Context(context), m_PipelineLayout(pipelineLayout), m_Width(width), m_Height(height)
    {
        m_DepthFormat = FindDepthFormat();
        CreateRenderPass();
        CreateResources();
        CreateFramebuffer();
        CreateSampler();
        CreatePipeline();
        AE_ENGINE_TRACE("ShadowRenderer created: {0}x{1}", width, height);
    }

    ShadowRenderer::~ShadowRenderer()
    {
        auto allocator { m_Context->GetAllocator() };
        auto device { m_Context->GetDevice() };

        vkDestroySampler(device, m_Sampler, nullptr);
        vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
        vkDestroyRenderPass(device, m_RenderPass, nullptr);
        vkDestroyImageView(device, m_DepthImageView, nullptr);
        vmaDestroyImage(allocator, m_DepthImage, m_DepthAllocation);
    }

    void ShadowRenderer::Draw(VkCommandBuffer cmd, VkDescriptorSet descriptorSet, uint32_t objectCount, VkBuffer indirectBuffer)
    {
        VkRenderPassBeginInfo rpInfo {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = m_RenderPass;
        rpInfo.framebuffer = m_Framebuffer;
        rpInfo.renderArea.offset = { 0, 0 };
        rpInfo.renderArea.extent = { m_Width, m_Height };

        VkClearValue depthClear {};
        depthClear.depthStencil = { 1.0f, 0 };
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &depthClear;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport {};
        viewport.x = 0.0f; viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_Width);
        viewport.height = static_cast<float>(m_Height);
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset = { 0, 0 };
        scissor.extent = { m_Width, m_Height };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdSetDepthBias(cmd, 1.25f, 0.0f, 1.75f);

        m_Pipeline->Bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_PipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        if (objectCount > 0)
        {
            vkCmdDrawIndirect(cmd, indirectBuffer, 0, objectCount, sizeof(VkDrawIndirectCommand));
        }

        vkCmdEndRenderPass(cmd);
    }

    VkFormat ShadowRenderer::FindDepthFormat()
    {
        std::vector<VkFormat> candidates { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
        return m_Context->FindSupportedFormat(candidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    void ShadowRenderer::CreateResources()
    {
        auto allocator { m_Context->GetAllocator() };

        VkImageCreateInfo depthInfo {};
        depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthInfo.imageType = VK_IMAGE_TYPE_2D;
        depthInfo.extent.width = m_Width;
        depthInfo.extent.height = m_Height;
        depthInfo.extent.depth = 1;
        depthInfo.mipLevels = 1;
        depthInfo.arrayLayers = 1;
        depthInfo.format = m_DepthFormat;
        depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        vmaCreateImage(allocator, &depthInfo, &allocationInfo, &m_DepthImage, &m_DepthAllocation, nullptr);

        VkImageViewCreateInfo depthViewInfo {};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = m_DepthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = m_DepthFormat;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;
        
        vkCreateImageView(m_Context->GetDevice(), &depthViewInfo, nullptr, &m_DepthImageView);
    }

    void ShadowRenderer::CreateRenderPass()
    {
        VkAttachmentDescription depthAttachment {};
        depthAttachment.format = m_DepthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; 

        VkAttachmentReference depthAttachmentRef {};
        depthAttachmentRef.attachment = 0;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 0;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        std::array<VkSubpassDependency, 2> dependencies;
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &depthAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(m_Context->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
        {
            AE_CLIENT_CRITICAL("Failed to create shadow render pass!");
            throw std::runtime_error("Failed to create shadow render pass");
        }
    }

    void ShadowRenderer::CreateFramebuffer()
    {
        VkFramebufferCreateInfo framebufferInfo {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_DepthImageView;
        framebufferInfo.width = m_Width;
        framebufferInfo.height = m_Height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_Context->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffer) != VK_SUCCESS)
        {
            AE_CLIENT_CRITICAL("Failed to create shadow framebuffer!");
            throw std::runtime_error("Failed to create shadow framebuffer");
        }
    }

    void ShadowRenderer::CreateSampler()
    {
        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.compareEnable = VK_TRUE;
        samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
        {
            AE_CLIENT_CRITICAL("Failed to create shadow sampler!");
            throw std::runtime_error("Failed to create shadow sampler");
        }
    }

    void ShadowRenderer::CreatePipeline()
    {
        PipelineConfigInfo config {};
        Pipeline::DefaultPipelineConfigInfo(config, m_Context);
        config.pipelineLayout = m_PipelineLayout;
        config.renderPass = m_RenderPass;

        config.colorBlendInfo.attachmentCount = 0;
        config.colorBlendInfo.pAttachments = nullptr;

        config.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        config.rasterizationInfo.depthBiasEnable = VK_TRUE;

        config.dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
        config.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(config.dynamicStateEnables.size());
        config.dynamicStateInfo.pDynamicStates = config.dynamicStateEnables.data();

        m_Pipeline = std::make_unique<Pipeline>(
            m_Context,
            "Assets/Shaders/shadow.vert.spv",
            "",
            config
        );
    }
}
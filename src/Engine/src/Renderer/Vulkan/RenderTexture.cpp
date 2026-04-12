#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Debug/Log.hpp>

#include <stdexcept>
#include <array>


namespace Antelope
{
    RenderTexture::RenderTexture(std::shared_ptr<VulkanContext> context, uint32_t width, uint32_t height, VkFormat colorFormat)
        : m_Context(context), m_Width(width), m_Height(height), m_ColorFormat(colorFormat)
    {
        m_DepthFormat = FindDepthFormat();

        CreateRenderPass();
        CreateResources();
        CreateFramebuffer();
        CreateSampler();

        AE_ENGINE_TRACE("RenderTexture (Offscreen MSAA) created: {0}x{1}", width, height);
    }

    RenderTexture::~RenderTexture()
    {
        DestroyResources();

        if (m_Sampler != VK_NULL_HANDLE)
            vkDestroySampler(m_Context->GetDevice(), m_Sampler, nullptr);

        if (m_RenderPass != VK_NULL_HANDLE)
            vkDestroyRenderPass(m_Context->GetDevice(), m_RenderPass, nullptr);
    }

    void RenderTexture::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0 || (m_Width == width && m_Height == height)) return;

        m_Width = width;
        m_Height = height;

        DestroyResources();
        CreateResources();
        CreateFramebuffer();
    }

    void RenderTexture::DestroyResources()
    {
        if (m_Framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_Context->GetDevice(), m_Framebuffer, nullptr);
            m_Framebuffer = VK_NULL_HANDLE;
        }

        auto allocator { m_Context->GetAllocator() };
        auto device { m_Context->GetDevice() };

        if (m_ColorImageView) { vkDestroyImageView(device, m_ColorImageView, nullptr); }
        if (m_ColorImage) { vmaDestroyImage(allocator, m_ColorImage, m_ColorAllocation); }

        if (m_DepthImageView) { vkDestroyImageView(device, m_DepthImageView, nullptr); }
        if (m_DepthImage) { vmaDestroyImage(allocator, m_DepthImage, m_DepthAllocation); }

        if (m_ResolveImageView) { vkDestroyImageView(device, m_ResolveImageView, nullptr); }
        if (m_ResolveImage) { vmaDestroyImage(allocator, m_ResolveImage, m_ResolveAllocation); }

        if (m_MaskImageView) { vkDestroyImageView(device, m_MaskImageView, nullptr); }
        if (m_MaskImage) { vmaDestroyImage(allocator, m_MaskImage, m_MaskAllocation); }
        
        m_ColorImageView = m_DepthImageView = m_ResolveImageView = m_MaskImageView = VK_NULL_HANDLE;
        m_ColorImage = m_DepthImage = m_ResolveImage = m_MaskImage = VK_NULL_HANDLE;

    }

    void RenderTexture::CreateResources()
    {
        auto allocator { m_Context->GetAllocator() };
        VkSampleCountFlagBits msaaSamples { m_Context->GetMsaaSamples() };

        VkImageCreateInfo colorInfo {};
        colorInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        colorInfo.imageType = VK_IMAGE_TYPE_2D;
        colorInfo.extent.width = m_Width;
        colorInfo.extent.height = m_Height;
        colorInfo.extent.depth = 1;
        colorInfo.mipLevels = 1;
        colorInfo.arrayLayers = 1;
        colorInfo.format = m_ColorFormat;
        colorInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        colorInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        colorInfo.samples = msaaSamples;
        colorInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        vmaCreateImage(allocator, &colorInfo, &allocationInfo, &m_ColorImage, &m_ColorAllocation, nullptr);

        VkImageViewCreateInfo colorViewInfo {};
        colorViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorViewInfo.image = m_ColorImage;
        colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorViewInfo.format = m_ColorFormat;
        colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorViewInfo.subresourceRange.baseMipLevel = 0;
        colorViewInfo.subresourceRange.levelCount = 1;
        colorViewInfo.subresourceRange.baseArrayLayer = 0;
        colorViewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(m_Context->GetDevice(), &colorViewInfo, nullptr, &m_ColorImageView);

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
        depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthInfo.samples = msaaSamples;
        depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
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

        VkImageCreateInfo resolveInfo {};
        resolveInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        resolveInfo.imageType = VK_IMAGE_TYPE_2D;
        resolveInfo.extent.width = m_Width;
        resolveInfo.extent.height = m_Height;
        resolveInfo.extent.depth = 1;
        resolveInfo.mipLevels = 1;
        resolveInfo.arrayLayers = 1;
        resolveInfo.format = m_ColorFormat;
        resolveInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        resolveInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        resolveInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vmaCreateImage(allocator, &resolveInfo, &allocationInfo, &m_ResolveImage, &m_ResolveAllocation, nullptr);

        VkImageViewCreateInfo resolveViewInfo {};
        resolveViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        resolveViewInfo.image = m_ResolveImage;
        resolveViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        resolveViewInfo.format = m_ColorFormat;
        resolveViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        resolveViewInfo.subresourceRange.baseMipLevel = 0;
        resolveViewInfo.subresourceRange.levelCount = 1;
        resolveViewInfo.subresourceRange.baseArrayLayer = 0;
        resolveViewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(m_Context->GetDevice(), &resolveViewInfo, nullptr, &m_ResolveImageView);

        VkImageCreateInfo maskInfo {};
        maskInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        maskInfo.imageType = VK_IMAGE_TYPE_2D;
        maskInfo.extent.width = m_Width;
        maskInfo.extent.height = m_Height;
        maskInfo.extent.depth = 1;
        maskInfo.mipLevels = 1;
        maskInfo.arrayLayers = 1;
        maskInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        maskInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        maskInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        maskInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        maskInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        maskInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vmaCreateImage(allocator, &maskInfo, &allocationInfo, &m_MaskImage, &m_MaskAllocation, nullptr);

        VkImageViewCreateInfo maskViewInfo {};
        maskViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        maskViewInfo.image = m_MaskImage;
        maskViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        maskViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        maskViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        maskViewInfo.subresourceRange.baseMipLevel = 0;
        maskViewInfo.subresourceRange.levelCount = 1;
        maskViewInfo.subresourceRange.baseArrayLayer = 0;
        maskViewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(m_Context->GetDevice(), &maskViewInfo, nullptr, &m_MaskImageView);

        m_Context->ImmediateSubmit([this](VkCommandBuffer cmd)
        {
            VkImageMemoryBarrier toClear {};
            toClear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toClear.image = m_ResolveImage;
            toClear.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            toClear.subresourceRange.baseMipLevel = 0;
            toClear.subresourceRange.levelCount = 1;
            toClear.subresourceRange.baseArrayLayer = 0;
            toClear.subresourceRange.layerCount = 1;
            toClear.srcAccessMask = 0;
            toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &toClear);

            VkClearColorValue clearColor {};
            clearColor.float32[0] = 0.1f;
            clearColor.float32[1] = 0.1f;
            clearColor.float32[2] = 0.17f;
            clearColor.float32[3] = 1.0f;

            VkImageSubresourceRange range {};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;

            vkCmdClearColorImage(cmd, m_ResolveImage,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

            VkImageMemoryBarrier toReadable {};
            toReadable.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toReadable.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toReadable.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toReadable.image = m_ResolveImage;
            toReadable.subresourceRange = range;
            toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &toReadable);
        });
    }

    void RenderTexture::CreateRenderPass()
    {
        VkSampleCountFlagBits msaaSamples { m_Context->GetMsaaSamples() };

        VkAttachmentDescription colorAttachment {};
        colorAttachment.format = m_ColorFormat;
        colorAttachment.samples = msaaSamples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment {};
        depthAttachment.format = m_DepthFormat;
        depthAttachment.samples = msaaSamples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription resolveAttachment {};
        resolveAttachment.format = m_ColorFormat;
        resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference colorAttachmentRef {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef {};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference resolveAttachmentRef {};
        resolveAttachmentRef.attachment = 2;
        resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.pResolveAttachments = &resolveAttachmentRef;

        VkSubpassDependency dependencyIn {};
        dependencyIn.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencyIn.dstSubpass = 0;
        dependencyIn.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencyIn.srcAccessMask = 0;
        dependencyIn.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencyIn.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkSubpassDependency dependencyOut {};
        dependencyOut.srcSubpass = 0;
        dependencyOut.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencyOut.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencyOut.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencyOut.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencyOut.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        std::array<VkAttachmentDescription, 3> attachments { colorAttachment, depthAttachment, resolveAttachment };
        std::array<VkSubpassDependency, 2> dependencies { dependencyIn, dependencyOut };
        
        VkRenderPassCreateInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(m_Context->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) 
        {
            AE_CLIENT_CRITICAL("Failed to create offscreen render pass!");
            throw std::runtime_error("Failed to create offscreen render pass");
        }
    }
    void RenderTexture::CreateFramebuffer()
    {
        std::array<VkImageView, 3> attachments { m_ColorImageView, m_DepthImageView, m_ResolveImageView };

        VkFramebufferCreateInfo framebufferInfo {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_Width;
        framebufferInfo.height = m_Height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_Context->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffer) != VK_SUCCESS) 
        {
            AE_CLIENT_CRITICAL("Failed to create offscreen framebuffer!");
            throw std::runtime_error("Failed to create offscreen framebuffer");
        }
    }

    void RenderTexture::CreateSampler()
    {
        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) 
        {
            AE_CLIENT_CRITICAL("Failed to create offscreen texture sampler!");
            throw std::runtime_error("Failed to create offscreen texture sampler");
        }
    }

    VkFormat RenderTexture::FindDepthFormat()
    {
        std::vector<VkFormat> candidates { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
        return m_Context->FindSupportedFormat(candidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }
}
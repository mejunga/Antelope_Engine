#include <Engine/Renderer/Vulkan/SwapChain.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Debug/Log.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>
#include <array>


namespace Antelope
{
    SwapChain::SwapChain(std::shared_ptr<VulkanContext> context) : m_Context(context)
    {
        CreateSwapchain();
        AE_ENGINE_INFO("Swapchain created.");
        CreateImageViews();
        AE_ENGINE_INFO("Swapchain Image Views created.");
        CreateColorResources();
        AE_ENGINE_INFO("Color Resources (MSAA) created.");
        CreateDepthResources();
        AE_ENGINE_INFO("Depth Resources created.");
        CreateRenderPass();
        AE_ENGINE_INFO("Render Pass created.");
        CreateFramebuffers();
        AE_ENGINE_INFO("Framebuffers created.");
    }

    SwapChain::~SwapChain()
    {
        if (m_Swapchain != VK_NULL_HANDLE)
        {
            CleanupSwapchain();
            AE_ENGINE_TRACE("Depth resources destroyed.");
            AE_ENGINE_TRACE("Framebuffers destroyed.");
            AE_ENGINE_TRACE("Swapchain Image Views destroyed.");
            AE_ENGINE_TRACE("Swapchain destroyed.");
        }

        if (m_RenderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_Context->GetDevice(), m_RenderPass, nullptr);
            AE_ENGINE_TRACE("Render Pass destroyed.");
        }
    }

    void SwapChain::RecreateSwapchain()
    {
        int width { 0 }, height { 0 };
        glfwGetFramebufferSize(m_Context->GetWindowHandle(), &width, &height);
        
        while (width == 0 || height == 0) 
        {
            glfwGetFramebufferSize(m_Context->GetWindowHandle(), &width, &height);
            glfwWaitEvents();
        }

        if (m_Context && m_Context->GetDevice() != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(m_Context->GetDevice());
        }

        CleanupSwapchain();
        CreateSwapchain();
        CreateImageViews();
        CreateColorResources();
        CreateDepthResources();
        CreateFramebuffers();
    }

    VkSurfaceFormatKHR SwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR SwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* windowHandle)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
        { 
            return capabilities.currentExtent; 
        }

        int width, height;
        glfwGetFramebufferSize(windowHandle, &width, &height);

        VkExtent2D actualExtent { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        
        return actualExtent;
    }

    VkFormat SwapChain::FindDepthFormat()
    {
        return m_Context->FindSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    void SwapChain::CleanupSwapchain()
    {
        if (m_ColorImageView != VK_NULL_HANDLE)
        { 
            vkDestroyImageView(m_Context->GetDevice(), m_ColorImageView, nullptr); 
            m_ColorImageView = VK_NULL_HANDLE; 
        }

        if (m_ColorImage != VK_NULL_HANDLE)
        {
            vmaDestroyImage(m_Context->GetAllocator(), m_ColorImage, m_ColorImageAllocation);
            m_ColorImage = VK_NULL_HANDLE;
        }

        if (m_DepthImageView != VK_NULL_HANDLE) 
        {
            vkDestroyImageView(m_Context->GetDevice(), m_DepthImageView, nullptr);
            m_DepthImageView = VK_NULL_HANDLE;
        }

        if (m_DepthImage != VK_NULL_HANDLE) 
        {
            vmaDestroyImage(m_Context->GetAllocator(), m_DepthImage, m_DepthImageAllocation);
            m_DepthImage = VK_NULL_HANDLE;
        }

        if (!m_SwapchainFramebuffers.empty()) 
        {
            for (auto framebuffer : m_SwapchainFramebuffers) 
            {
                vkDestroyFramebuffer(m_Context->GetDevice(), framebuffer, nullptr);
            }
            m_SwapchainFramebuffers.clear();
        }

        if (!m_SwapchainImageViews.empty()) 
        {
            for (auto imageView : m_SwapchainImageViews) 
            {
                vkDestroyImageView(m_Context->GetDevice(), imageView, nullptr);
            }
            m_SwapchainImageViews.clear(); 
        }

        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Context->GetDevice(), m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
    }

    void SwapChain::CreateSwapchain()
    {
        SwapchainSupportDetails swapchainSupport { m_Context->QuerySwapChainSupport(m_Context->GetPhysicalDevice()) };
        VkSurfaceFormatKHR surfaceFormat { ChooseSwapSurfaceFormat(swapchainSupport.Formats) };
        VkPresentModeKHR presentMode { ChooseSwapPresentMode(swapchainSupport.PresentModes) };
        VkExtent2D extent { ChooseSwapExtent(swapchainSupport.Capabilities, m_Context->GetWindowHandle()) };
        uint32_t imageCount { swapchainSupport.Capabilities.minImageCount + 1 };

        if (swapchainSupport.Capabilities.maxImageCount > 0 && imageCount > swapchainSupport.Capabilities.maxImageCount)
        {
            imageCount = swapchainSupport.Capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR swapchainInfo {};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = m_Context->GetSurface();
        swapchainInfo.minImageCount = imageCount;
        swapchainInfo.imageFormat = surfaceFormat.format;
        swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainInfo.imageExtent = extent;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices { m_Context->FindQueueFamilies(m_Context->GetPhysicalDevice()) };
        uint32_t queueFamilyIndices[] = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

        if (indices.GraphicsFamily != indices.PresentFamily)
        {
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchainInfo.queueFamilyIndexCount = 2;
            swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchainInfo.queueFamilyIndexCount = 0;
            swapchainInfo.pQueueFamilyIndices = nullptr;
        }

        swapchainInfo.preTransform = swapchainSupport.Capabilities.currentTransform;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = presentMode;
        swapchainInfo.clipped = VK_TRUE;
        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_Context->GetDevice(), &swapchainInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Swapchain!");
            throw std::runtime_error("Failed to create Swapchain");
        }

        vkGetSwapchainImagesKHR(m_Context->GetDevice(), m_Swapchain, &imageCount, nullptr);
        m_SwapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_Context->GetDevice(), m_Swapchain, &imageCount, m_SwapchainImages.data());
        
        m_SwapchainImageFormat = surfaceFormat.format;
        m_SwapchainExtent = extent;
    }

    void SwapChain::CreateImageViews()
    {
        uint32_t imageViewsSize { static_cast<uint32_t>(m_SwapchainImages.size()) };
        m_SwapchainImageViews.resize(imageViewsSize);

        for (size_t i { 0 }; i < imageViewsSize; ++i)
        {
            VkImageViewCreateInfo ImageViewInfo {};
            ImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ImageViewInfo.image = m_SwapchainImages[i];
            ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ImageViewInfo.format = m_SwapchainImageFormat;
            ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ImageViewInfo.subresourceRange.baseMipLevel = 0;
            ImageViewInfo.subresourceRange.levelCount = 1;
            ImageViewInfo.subresourceRange.baseArrayLayer = 0;
            ImageViewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_Context->GetDevice(), &ImageViewInfo, nullptr, &m_SwapchainImageViews[i]) != VK_SUCCESS)
            {
                AE_ENGINE_CRITICAL("Failed to create Swapchain Image Views!");
                throw std::runtime_error("Failed to create Swapchain Image Views");
            }
        }
    }

    void SwapChain::CreateDepthResources() 
    {
        m_DepthFormat = FindDepthFormat();

        VkImageCreateInfo imageInfo {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_SwapchainExtent.width;
        imageInfo.extent.height = m_SwapchainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_DepthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = m_Context->GetMsaaSamples();
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        if (vmaCreateImage(m_Context->GetAllocator(), &imageInfo, &allocationInfo, &m_DepthImage, &m_DepthImageAllocation, nullptr) != VK_SUCCESS)
        {
            AE_CLIENT_CRITICAL("Failed to create depth image!");
            throw std::runtime_error("Failed to create depth image");
        }

        VkImageViewCreateInfo viewInfo {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_DepthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_DepthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS)
        {
            AE_CLIENT_CRITICAL("Failed to create depth image view!");
            throw std::runtime_error("Failed to create depth image view");
        }
    }

    void SwapChain::CreateRenderPass()
    {
        VkAttachmentDescription colorAttachment {};
        colorAttachment.format = m_SwapchainImageFormat;
        colorAttachment.samples = m_Context->GetMsaaSamples();
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment {};
        depthAttachment.format = FindDepthFormat();
        depthAttachment.samples = m_Context->GetMsaaSamples();
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorAttachmentResolve {};
        colorAttachmentResolve.format = m_SwapchainImageFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef {};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentResolveRef {};
        colorAttachmentResolveRef.attachment = 2;
        colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.pResolveAttachments = &colorAttachmentResolveRef;

        VkSubpassDependency dependency {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 3> attachments {colorAttachment, depthAttachment, colorAttachmentResolve};

        VkRenderPassCreateInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_Context->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Render Pass!");
            throw std::runtime_error("Failed to create Render Pass");
        }
    }

    void SwapChain::CreateFramebuffers()
    {
        m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());

        for (size_t i { 0 }; i < m_SwapchainImageViews.size(); i++) 
        {
            std::array<VkImageView, 3> attachments
            {
                m_ColorImageView,
                m_DepthImageView,
                m_SwapchainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_SwapchainExtent.width;
            framebufferInfo.height = m_SwapchainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_Context->GetDevice(), &framebufferInfo, nullptr, &m_SwapchainFramebuffers[i]) != VK_SUCCESS) 
            {
                AE_ENGINE_CRITICAL("Failed to create Framebuffer!");
                throw std::runtime_error("Failed to create Framebuffer");
            }
        }
    }

    void SwapChain::CreateColorResources() 
    {
        VkImageCreateInfo imageInfo {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_SwapchainExtent.width;
        imageInfo.extent.height = m_SwapchainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_SwapchainImageFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.samples = m_Context->GetMsaaSamples();
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        if (vmaCreateImage(m_Context->GetAllocator(), &imageInfo, &allocationInfo, &m_ColorImage, &m_ColorImageAllocation, nullptr) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create MSAA color image!");
            throw std::runtime_error("Failed to create MSAA color image");
        }

        VkImageViewCreateInfo viewInfo {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_ColorImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_SwapchainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_ColorImageView) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create MSAA color image view!");
            throw std::runtime_error("Failed to create MSAA color image view");
        }
    }
}
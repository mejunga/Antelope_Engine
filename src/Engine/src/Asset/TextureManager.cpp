#define STB_IMAGE_IMPLEMENTATION
#include <Engine/Asset/TextureManager.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Debug/Log.hpp>

#include <stb_image.h>
#include <stdexcept>


namespace Antelope
{
    TextureManager::TextureManager(std::shared_ptr<VulkanContext> context, std::shared_ptr<Renderer> renderer)
        : m_Context(context), m_Renderer(renderer)
    {
        AE_ENGINE_INFO("TextureManager initialized.");
    }

    TextureManager::~TextureManager()
    {
        for (auto& texture : m_Textures)
        {
            if (texture.Sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(m_Context->GetDevice(), texture.Sampler, nullptr);
            }
            
            if (texture.ImageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_Context->GetDevice(), texture.ImageView, nullptr);
            }    
            
            if (texture.Image != VK_NULL_HANDLE)
            {
                vmaDestroyImage(m_Context->GetAllocator(), texture.Image, texture.Allocation);
            }    
        }

        m_Textures.clear();
        AE_ENGINE_TRACE("TextureManager destroyed and all textures freed.");
    }

    uint32_t TextureManager::LoadTexture(const std::string& filepath, bool isSRGB)
    {
        auto it { m_PathToIndex.find(filepath) };
        if (it != m_PathToIndex.end()) { return it->second; }

        int texWidth { 0 }, texHeight { 0 }, texChannels { 0 };
        stbi_uc* pixels { stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha) };

        if (!pixels)
        {
            AE_ENGINE_ERROR("Failed to load texture image: {0}", filepath);
            return 0;
        }

        VkDeviceSize imageSize { static_cast<VkDeviceSize>(texWidth * texHeight * 4) };

        VkBuffer stagingBuffer { VK_NULL_HANDLE };
        VmaAllocation stagingAllocation { VK_NULL_HANDLE };
        VkDeviceSize stagingOffset { 0 };
        m_Renderer->CreateStagingBuffer(pixels, imageSize, stagingBuffer, stagingAllocation, stagingOffset);

        stbi_image_free(pixels);

        Texture newTexture {};

        newTexture.GlobalIndex = static_cast<uint32_t>(m_Textures.size());

        VkFormat format { isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM };

        CreateImage(texWidth, texHeight, format, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VMA_MEMORY_USAGE_AUTO, newTexture.Image, newTexture.Allocation);

        VkCommandBuffer cmd { m_Renderer->BeginAsyncGraphicsCommand() };

        TransitionImageLayout(cmd, newTexture.Image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(cmd, stagingBuffer, newTexture.Image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), stagingOffset);
        TransitionImageLayout(cmd, newTexture.Image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        m_Renderer->EndAndSubmitAsyncGraphicsCommand(cmd, stagingBuffer, stagingAllocation, newTexture.GlobalIndex);

        newTexture.ImageView = CreateImageView(newTexture.Image, format);
        newTexture.Sampler = CreateTextureSampler();

        m_Textures.push_back(newTexture);
        m_PathToIndex[filepath] = newTexture.GlobalIndex;
        m_Renderer->UpdateTextureDescriptors(m_Textures);

        AE_ENGINE_TRACE("Texture transfer dispatched for: {0}", filepath);
        return newTexture.GlobalIndex;
    }

    void TextureManager::Clear()
    {
        vkDeviceWaitIdle(m_Context->GetDevice());

        for (auto& texture : m_Textures)
        {
            if (texture.Sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(m_Context->GetDevice(), texture.Sampler, nullptr);
            }

            if (texture.ImageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_Context->GetDevice(), texture.ImageView, nullptr);
            }

            if (texture.Image != VK_NULL_HANDLE)
            {
                vmaDestroyImage(m_Context->GetAllocator(), texture.Image, texture.Allocation);
            }
        }

        m_Textures.clear();
        m_PathToIndex.clear();
        AE_ENGINE_TRACE("TextureManager: cache cleared.");
    }

    void TextureManager::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memUsage, VkImage& image, VmaAllocation& allocation)
    {
        VkImageCreateInfo imageInfo {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = memUsage;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        if (vmaCreateImage(m_Context->GetAllocator(), &imageInfo, &allocationInfo, &image, &allocation, nullptr) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create Vulkan image!");
            throw std::runtime_error("Failed to create Vulkan image");
        }
    }

    void TextureManager::TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkImageMemoryBarrier barrier {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            AE_ENGINE_CRITICAL("Unsupported layout transition!");
            throw std::invalid_argument("Unsupported layout transition");
        }

        vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void TextureManager::CopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkDeviceSize bufferOffset)
    {
        VkBufferImageCopy region {};
        region.bufferOffset = bufferOffset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    VkImageView TextureManager::CreateImageView(VkImage image, VkFormat format)
    {
        VkImageViewCreateInfo viewInfo {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView { VK_NULL_HANDLE };

        if (vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create texture image view!");
            throw std::runtime_error("Failed to create texture image view");
        }

        return imageView;
    }

    VkSampler TextureManager::CreateTextureSampler()
    {
        VkPhysicalDeviceProperties properties {};
        vkGetPhysicalDeviceProperties(m_Context->GetPhysicalDevice(), &properties);

        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        VkSampler sampler;

        if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create texture sampler!");
            throw std::runtime_error("Failed to create texture sampler");
        }
        return sampler;
    }
}
#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <string>
#include <vector>
#include <memory>


namespace Antelope
{
    class VulkanContext;
    class Renderer;

    struct Texture
    {
        VkImage Image { VK_NULL_HANDLE };
        VmaAllocation Allocation { VK_NULL_HANDLE };
        VkImageView ImageView { VK_NULL_HANDLE };
        VkSampler Sampler { VK_NULL_HANDLE };
        uint32_t GlobalIndex { 0 };
    };

    class TextureManager
    {
        public:
            TextureManager(std::shared_ptr<VulkanContext> context, std::shared_ptr<Renderer> renderer);
            ~TextureManager();

            uint32_t LoadTexture(const std::string& filepath);

            const std::vector<Texture>& GetGlobalTextures() const { return m_Textures; }

        private:
            void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memUsage, VkImage& image, VmaAllocation& allocation);
            void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
            void CopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
            VkImageView CreateImageView(VkImage image, VkFormat format);
            VkSampler CreateTextureSampler();

        private:
            std::shared_ptr<VulkanContext> m_Context;
            std::shared_ptr<Renderer> m_Renderer;
            
            std::vector<Texture> m_Textures;
    };
}
#include <Engine/Renderer/Graphics/PostCompositePass.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/Pipeline.hpp>
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Debug/Log.hpp>
#include <stdexcept>

namespace Antelope
{
    struct PostCompositePushConstants
    {
        float exposure;
        float time;
    };

    PostCompositePass::PostCompositePass(std::shared_ptr<VulkanContext> context, std::shared_ptr<RenderTexture> sceneTexture, std::shared_ptr<RenderTexture> bloomTexture, std::shared_ptr<RenderTexture> flareTexture, VkRenderPass renderPass)
        : m_Context(context)
    {
        CreateDescriptorLayout();
        CreatePipeline(renderPass);
        CreateIdentityLUT();
        UpdateDescriptorSet(sceneTexture, bloomTexture, flareTexture);
        AE_ENGINE_TRACE("PostCompositePass initialized.");
    }

    PostCompositePass::~PostCompositePass()
    {
        auto device { m_Context->GetDevice() };
        vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);

        if (m_LUTImage != VK_NULL_HANDLE)
        {
            vkDestroySampler(device, m_LUTSampler, nullptr);
            vkDestroyImageView(device, m_LUTImageView, nullptr);
            vmaDestroyImage(m_Context->GetAllocator(), m_LUTImage, m_LUTAllocation);
        }
    }

    void PostCompositePass::Draw(VkCommandBuffer cmd, float time, float exposure)
    {
        m_Pipeline->Bind(cmd);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);

        PostCompositePushConstants push {};
        push.exposure = exposure;
        push.time = time;
        vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostCompositePushConstants), &push);

        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    void PostCompositePass::UpdateDescriptorSet(std::shared_ptr<RenderTexture> sceneTexture, std::shared_ptr<RenderTexture> bloomTexture, std::shared_ptr<RenderTexture> flareTexture)
    {
        VkDescriptorImageInfo sceneInfo {};
        sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        sceneInfo.imageView = sceneTexture->GetResolveImageView(); 
        sceneInfo.sampler = sceneTexture->GetSampler();

        VkDescriptorImageInfo bloomInfo {};
        bloomInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bloomInfo.imageView = bloomTexture->GetResolveImageView(); 
        bloomInfo.sampler = bloomTexture->GetSampler();

        VkDescriptorImageInfo lutInfo {};
        lutInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        lutInfo.imageView = m_LUTImageView;
        lutInfo.sampler = m_LUTSampler;

        VkDescriptorImageInfo flareInfo {};
        flareInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        flareInfo.imageView = flareTexture->GetResolveImageView(); 
        flareInfo.sampler = flareTexture->GetSampler();

        std::vector<VkWriteDescriptorSet> writes(4);
        
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_DescriptorSet;
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &sceneInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_DescriptorSet;
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &bloomInfo;

        writes[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        writes[2].dstSet = m_DescriptorSet;
        writes[2].dstBinding = 2;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo = &lutInfo;

        writes[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        writes[3].dstSet = m_DescriptorSet;
        writes[3].dstBinding = 3;
        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[3].descriptorCount = 1;
        writes[3].pImageInfo = &flareInfo;

        vkUpdateDescriptorSets(m_Context->GetDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void PostCompositePass::SetLUT(VkImageView view, VkSampler sampler)
    {
        VkDescriptorImageInfo lutInfo {};
        lutInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        lutInfo.imageView = view;
        lutInfo.sampler = sampler;

        VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet = m_DescriptorSet;
        write.dstBinding = 2;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &lutInfo;

        vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &write, 0, nullptr);
    }

    void PostCompositePass::CreateDescriptorLayout()
    {
        auto device { m_Context->GetDevice() };
        VkDescriptorSetLayoutBinding textureBinding {};
        textureBinding.binding = 0;
        textureBinding.descriptorCount = 1;
        textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding bloomBinding {};
        bloomBinding.binding = 1;
        bloomBinding.descriptorCount = 1;
        bloomBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bloomBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding lutBinding {};
        lutBinding.binding = 2;
        lutBinding.descriptorCount = 1;
        lutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding flareBinding {};
        flareBinding.binding = 3;
        flareBinding.descriptorCount = 1;
        flareBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        flareBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::vector<VkDescriptorSetLayoutBinding> bindings { textureBinding, bloomBinding, lutBinding, flareBinding };

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create PostCompositePass descriptor set layout!");
            throw std::runtime_error("Failed to create PostCompositePass descriptor set layout");
        }

        VkPushConstantRange pushConstantRange {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PostCompositePushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create PostCompositePass pipeline layout!");
            throw std::runtime_error("Failed to create PostCompositePass pipeline layout");
        }

        VkDescriptorPoolSize poolSize {};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 4;

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to allocate post PostCompositePass descriptor set!");
            throw std::runtime_error("Failed to allocate post PostCompositePass descriptor set");
        }

        VkDescriptorSetAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_DescriptorPool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &m_DescriptorSetLayout;

        if (vkAllocateDescriptorSets(device, &allocateInfo, &m_DescriptorSet) != VK_SUCCESS)
        {
            AE_CLIENT_CRITICAL("Failed to create descriptor pool!");
            throw std::runtime_error("Failed to create descriptor pool");
        }
    }

    void PostCompositePass::CreatePipeline(VkRenderPass renderPass)
    {
        PipelineConfigInfo config {};
        Pipeline::DefaultPipelineConfigInfo(config, m_Context);
        
        config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        config.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        config.depthStencilInfo.depthTestEnable = VK_FALSE;
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;

        config.pipelineLayout = m_PipelineLayout;
        config.renderPass = renderPass;

        m_Pipeline = std::make_unique<Pipeline>(
            m_Context,
            "Assets/Shaders/postprocess.vert.spv",
            "Assets/Shaders/post_composite.frag.spv",
            config
        );
    }

    void PostCompositePass::CreateIdentityLUT()
    {
        constexpr uint32_t N { 32 };
        constexpr uint32_t texelCount { N * N * N };

        std::vector<uint8_t> pixels(texelCount * 4);
        for (uint32_t b { 0 }; b < N; b++)
        for (uint32_t g { 0 }; g < N; g++)
        for (uint32_t r { 0 }; r < N; r++)
        {
            uint32_t idx { (b * N * N + g * N + r) * 4 };
            pixels[idx + 0] = static_cast<uint8_t>(r * 255 / (N - 1));
            pixels[idx + 1] = static_cast<uint8_t>(g * 255 / (N - 1));
            pixels[idx + 2] = static_cast<uint8_t>(b * 255 / (N - 1));
            pixels[idx + 3] = 255;
        }

        VkDeviceSize bufferSize { texelCount * 4 };
        VkBuffer stagingBuffer { VK_NULL_HANDLE };
        VmaAllocation stagingAllocation { VK_NULL_HANDLE };

        VkBufferCreateInfo stagingInfo {};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size = bufferSize;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAllocInfo {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT 
                               | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo stagingResult {};
        vmaCreateBuffer(m_Context->GetAllocator(), &stagingInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, &stagingResult);
        memcpy(stagingResult.pMappedData, pixels.data(), bufferSize);

        VkImageCreateInfo imageInfo {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.extent = { N, N, N };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        VmaAllocationCreateInfo imageAllocInfo {};
        imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(m_Context->GetAllocator(), &imageInfo, &imageAllocInfo, &m_LUTImage, &m_LUTAllocation, nullptr);

        m_Context->ImmediateSubmit([&](VkCommandBuffer cmd)
        {
            VkImageMemoryBarrier toTransfer {};
            toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransfer.image = m_LUTImage;
            toTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            toTransfer.srcAccessMask = 0;
            toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

            VkBufferImageCopy region {};
            region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            region.imageExtent = { N, N, N };
            vkCmdCopyBufferToImage(cmd, stagingBuffer, m_LUTImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            VkImageMemoryBarrier toReadable {};
            toReadable.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toReadable.image = m_LUTImage;
            toReadable.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toReadable);
        });

        vmaDestroyBuffer(m_Context->GetAllocator(), stagingBuffer, stagingAllocation);

        VkImageViewCreateInfo viewInfo {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_LUTImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_LUTImageView);

        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &m_LUTSampler);

        AE_ENGINE_TRACE("PostCompositePass: identity LUT created (32^3).");
    }
}
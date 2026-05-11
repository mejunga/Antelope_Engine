#include <Engine/Renderer/Graphics/BloomPass.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/Pipeline.hpp>
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Debug/Log.hpp>

#include <stdexcept>
#include <algorithm>
#include <array>


namespace Antelope
{
    struct DownPushConstants 
    {
        float texelWidth;
        float texelHeight;
        float threshold;
        float knee;
    };

    struct UpPushConstants 
    {
        float texelWidth;
        float texelHeight;
        float filterRadius;
    };

    struct FlarePushConstants
    {
        glm::vec2 sunUV;
        float sunIntensity;
        float pad0;
        glm::vec2 moonUV;
        float moonIntensity;
        float pad1;
    };

    BloomPass::BloomPass(std::shared_ptr<VulkanContext> context, std::shared_ptr<RenderTexture> sceneTexture, uint32_t width, uint32_t height)
        : m_Context(context)
    {
        CreateDescriptorLayout();
        CreateMipChain(width, height);
        CreatePipelines();
        UpdateDescriptorSets(sceneTexture);
    }

    BloomPass::~BloomPass()
    {
        DestroyMipChain();
        auto device { m_Context->GetDevice() };
        
        vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, m_DownDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, m_UpDescriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(device, m_DownPipelineLayout, nullptr);
        vkDestroyPipelineLayout(device, m_UpPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, m_FlareDescriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(device, m_FlarePipelineLayout, nullptr);
    }

    void BloomPass::Resize(uint32_t width, uint32_t height, std::shared_ptr<RenderTexture> sceneTexture)
    {
        if (width == 0 || height == 0) { return; }

        DestroyMipChain();
        CreateMipChain(width, height);

        vkResetDescriptorPool(m_Context->GetDevice(), m_DescriptorPool, 0);
        m_DownDescriptorSets.clear();
        m_UpDescriptorSets.clear();
        m_SceneDescriptorSet = VK_NULL_HANDLE;

        UpdateDescriptorSets(sceneTexture);
    }

    void BloomPass::CreateMipChain(uint32_t width, uint32_t height)
    {
        for (uint32_t i { 0 }; i < m_MipCount; i++)
        {
            uint32_t mipWidth { std::max(1u, width >> (i + 1)) };
            uint32_t mipHeight { std::max(1u, height >> (i + 1)) };

            m_DownChain.push_back(std::make_shared<RenderTexture>(m_Context, mipWidth, mipHeight, VK_FORMAT_R16G16B16A16_SFLOAT, false));
            m_UpChain.push_back(std::make_shared<RenderTexture>(m_Context, mipWidth, mipHeight, VK_FORMAT_R16G16B16A16_SFLOAT, false));
        }

        m_FlareTexture = std::make_shared<RenderTexture>(m_Context, width / 2, height / 2, VK_FORMAT_R16G16B16A16_SFLOAT, false);
    }

    void BloomPass::DestroyMipChain()
    {
        m_DownChain.clear();
        m_UpChain.clear();
        m_FlareTexture.reset();
    }
    void BloomPass::CreateDescriptorLayout()
    {
        auto device { m_Context->GetDevice() };

        VkDescriptorSetLayoutBinding downBinding {};
        downBinding.binding = 0;
        downBinding.descriptorCount = 1;
        downBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        downBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo downLayoutInfo {};
        downLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        downLayoutInfo.bindingCount = 1;
        downLayoutInfo.pBindings = &downBinding;

        if (vkCreateDescriptorSetLayout(device, &downLayoutInfo, nullptr, &m_DownDescriptorSetLayout) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create BloomPass Downsample descriptor set layout!");
            throw std::runtime_error("Failed to create BloomPass Downsample descriptor set layout");
        }

        VkDescriptorSetLayoutBinding upCurrentBinding {};
        upCurrentBinding.binding = 1;
        upCurrentBinding.descriptorCount = 1;
        upCurrentBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        upCurrentBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::vector<VkDescriptorSetLayoutBinding> upBindings { downBinding, upCurrentBinding };

        VkDescriptorSetLayoutCreateInfo upLayoutInfo {};
        upLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        upLayoutInfo.bindingCount = static_cast<uint32_t>(upBindings.size());
        upLayoutInfo.pBindings = upBindings.data();

        if (vkCreateDescriptorSetLayout(device, &upLayoutInfo, nullptr, &m_UpDescriptorSetLayout) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create BloomPass Upsample descriptor set layout!");
            throw std::runtime_error("Failed to create BloomPass Upsample descriptor set layout");
        }

        VkPushConstantRange downPushRange {};
        downPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        downPushRange.offset = 0;
        downPushRange.size = sizeof(DownPushConstants);

        VkPipelineLayoutCreateInfo downPipelineLayoutInfo {};
        downPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        downPipelineLayoutInfo.setLayoutCount = 1;
        downPipelineLayoutInfo.pSetLayouts = &m_DownDescriptorSetLayout;
        downPipelineLayoutInfo.pushConstantRangeCount = 1;
        downPipelineLayoutInfo.pPushConstantRanges = &downPushRange;

        if (vkCreatePipelineLayout(device, &downPipelineLayoutInfo, nullptr, &m_DownPipelineLayout) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create BloomPass Downsample pipeline layout!");
            throw std::runtime_error("Failed to create BloomPass pipeline layout");
        }

        VkPushConstantRange upPushRange {};
        upPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        upPushRange.offset = 0;
        upPushRange.size = sizeof(UpPushConstants);

        VkPipelineLayoutCreateInfo upPipelineLayoutInfo {};
        upPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        upPipelineLayoutInfo.setLayoutCount = 1;
        upPipelineLayoutInfo.pSetLayouts = &m_UpDescriptorSetLayout;
        upPipelineLayoutInfo.pushConstantRangeCount = 1;
        upPipelineLayoutInfo.pPushConstantRanges = &upPushRange;

        if (vkCreatePipelineLayout(device, &upPipelineLayoutInfo, nullptr, &m_UpPipelineLayout) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create BloomPass Upsample pipeline layout!");
            throw std::runtime_error("Failed to create BloomPass pipeline layout");
        }

        VkDescriptorPoolSize poolSize {};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = static_cast<uint32_t>((m_MipCount * 3) + 2);

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>((m_MipCount * 2) + 2);

        VkDescriptorSetLayoutCreateInfo flareLayoutInfo {};
        flareLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        flareLayoutInfo.bindingCount = 1;
        flareLayoutInfo.pBindings = &downBinding;
        vkCreateDescriptorSetLayout(device, &flareLayoutInfo, nullptr, &m_FlareDescriptorSetLayout);

        VkPushConstantRange flarePushRange {};
        flarePushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        flarePushRange.offset = 0;
        flarePushRange.size = sizeof(FlarePushConstants);

        VkPipelineLayoutCreateInfo flarePipelineLayoutInfo {};
        flarePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        flarePipelineLayoutInfo.setLayoutCount = 1;
        flarePipelineLayoutInfo.pSetLayouts = &m_FlareDescriptorSetLayout;
        flarePipelineLayoutInfo.pushConstantRangeCount = 1;
        flarePipelineLayoutInfo.pPushConstantRanges = &flarePushRange;
        vkCreatePipelineLayout(device, &flarePipelineLayoutInfo, nullptr, &m_FlarePipelineLayout);

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create BloomPass descriptor pool!");
            throw std::runtime_error("Failed to create BloomPass descriptor pool");
        }
    }

    void BloomPass::UpdateDescriptorSets(std::shared_ptr<RenderTexture> sceneTexture)
    {
        auto device { m_Context->GetDevice() };

        VkDescriptorSetAllocateInfo sceneAllocInfo {};
        sceneAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        sceneAllocInfo.descriptorPool = m_DescriptorPool;
        sceneAllocInfo.descriptorSetCount = 1;
        sceneAllocInfo.pSetLayouts = &m_DownDescriptorSetLayout;
        vkAllocateDescriptorSets(device, &sceneAllocInfo, &m_SceneDescriptorSet);

        m_DownDescriptorSets.resize(m_MipCount);
        m_UpDescriptorSets.resize(m_MipCount);

        for (uint32_t i { 0 }; i < m_MipCount; i++)
        {
            VkDescriptorSetAllocateInfo downAllocInfo {};
            downAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            downAllocInfo.descriptorPool = m_DescriptorPool;
            downAllocInfo.descriptorSetCount = 1;
            downAllocInfo.pSetLayouts = &m_DownDescriptorSetLayout;
            vkAllocateDescriptorSets(device, &downAllocInfo, &m_DownDescriptorSets[i]);

            VkDescriptorSetAllocateInfo upAllocInfo {};
            upAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            upAllocInfo.descriptorPool = m_DescriptorPool;
            upAllocInfo.descriptorSetCount = 1;
            upAllocInfo.pSetLayouts = &m_UpDescriptorSetLayout;
            vkAllocateDescriptorSets(device, &upAllocInfo, &m_UpDescriptorSets[i]);
        }

        VkDescriptorImageInfo sceneInfo {};
        sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        sceneInfo.imageView = sceneTexture->GetResolveImageView();
        sceneInfo.sampler = sceneTexture->GetSampler();

        VkWriteDescriptorSet sceneWrite {};
        sceneWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sceneWrite.dstSet = m_SceneDescriptorSet;
        sceneWrite.dstBinding = 0;
        sceneWrite.dstArrayElement = 0;
        sceneWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sceneWrite.descriptorCount = 1;
        sceneWrite.pImageInfo = &sceneInfo;
        vkUpdateDescriptorSets(device, 1, &sceneWrite, 0, nullptr);

        for (uint32_t i { 0 }; i < m_MipCount; i++)
        {
            VkDescriptorImageInfo downInfo {};
            downInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            downInfo.imageView = m_DownChain[i]->GetResolveImageView();
            downInfo.sampler = m_DownChain[i]->GetSampler();

            VkWriteDescriptorSet downWrite {};
            downWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            downWrite.dstSet = m_DownDescriptorSets[i];
            downWrite.dstBinding = 0;
            downWrite.dstArrayElement = 0;
            downWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            downWrite.descriptorCount = 1;
            downWrite.pImageInfo = &downInfo;
            vkUpdateDescriptorSets(device, 1, &downWrite, 0, nullptr);

            std::vector<VkWriteDescriptorSet> upWrites;
            
            VkDescriptorImageInfo srcInfo {};
            srcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            if (i == m_MipCount - 1)
            {
                srcInfo.imageView = m_DownChain[i]->GetResolveImageView();
                srcInfo.sampler = m_DownChain[i]->GetSampler();
            }
            else
            {
                srcInfo.imageView = m_UpChain[i + 1]->GetResolveImageView();
                srcInfo.sampler = m_UpChain[i + 1]->GetSampler();
            }

            VkWriteDescriptorSet upWriteSrc {};
            upWriteSrc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            upWriteSrc.dstSet = m_UpDescriptorSets[i];
            upWriteSrc.dstBinding = 0;
            upWriteSrc.dstArrayElement = 0;
            upWriteSrc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            upWriteSrc.descriptorCount = 1;
            upWriteSrc.pImageInfo = &srcInfo;
            upWrites.push_back(upWriteSrc);

            VkWriteDescriptorSet upWriteCurrent {};
            upWriteCurrent.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            upWriteCurrent.dstSet = m_UpDescriptorSets[i];
            upWriteCurrent.dstBinding = 1;
            upWriteCurrent.dstArrayElement = 0;
            upWriteCurrent.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            upWriteCurrent.descriptorCount = 1;
            upWriteCurrent.pImageInfo = &downInfo;
            upWrites.push_back(upWriteCurrent);

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(upWrites.size()), upWrites.data(), 0, nullptr);
        }

        VkDescriptorSetAllocateInfo flareAllocInfo {};
        flareAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        flareAllocInfo.descriptorPool = m_DescriptorPool;
        flareAllocInfo.descriptorSetCount = 1;
        flareAllocInfo.pSetLayouts = &m_FlareDescriptorSetLayout;
        vkAllocateDescriptorSets(device, &flareAllocInfo, &m_FlareDescriptorSet);
        
        VkDescriptorImageInfo flareSrcInfo {};
        flareSrcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        flareSrcInfo.imageView = m_DownChain[1]->GetResolveImageView();
        flareSrcInfo.sampler = m_DownChain[1]->GetSampler();

        VkWriteDescriptorSet flareWrite {};
        flareWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        flareWrite.dstSet = m_FlareDescriptorSet;
        flareWrite.dstBinding = 0;
        flareWrite.dstArrayElement = 0;
        flareWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        flareWrite.descriptorCount = 1;
        flareWrite.pImageInfo = &flareSrcInfo;
        vkUpdateDescriptorSets(device, 1, &flareWrite, 0, nullptr);
    }

    void BloomPass::CreatePipelines()
    {
        PipelineConfigInfo downConfig {};
        Pipeline::DefaultPipelineConfigInfo(downConfig, m_Context);
        downConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        downConfig.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        downConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
        downConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        downConfig.pipelineLayout = m_DownPipelineLayout;
        downConfig.renderPass = m_DownChain[0]->GetRenderPass();

        m_DownsamplePipeline = std::make_unique<Pipeline>(
            m_Context,
            "Assets/Shaders/postprocess.vert.spv",
            "Assets/Shaders/bloom_downsample.frag.spv",
            downConfig
        );

        PipelineConfigInfo upConfig {};
        Pipeline::DefaultPipelineConfigInfo(upConfig, m_Context);
        upConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        upConfig.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        upConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
        upConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        upConfig.pipelineLayout = m_UpPipelineLayout;
        upConfig.renderPass = m_UpChain[0]->GetRenderPass();

        m_UpsamplePipeline = std::make_unique<Pipeline>(
            m_Context,
            "Assets/Shaders/postprocess.vert.spv",
            "Assets/Shaders/bloom_upsample.frag.spv",
            upConfig
        );

        PipelineConfigInfo flareConfig {};
        Pipeline::DefaultPipelineConfigInfo(flareConfig, m_Context);
        flareConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        flareConfig.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        flareConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
        flareConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        flareConfig.pipelineLayout = m_FlarePipelineLayout;
        flareConfig.renderPass = m_FlareTexture->GetRenderPass();
        m_FlarePipeline = std::make_unique<Pipeline>(
            m_Context,
            "Assets/Shaders/postprocess.vert.spv",
            "Assets/Shaders/lens_flare.frag.spv",
            flareConfig
        );
    }

    void BloomPass::Draw(VkCommandBuffer cmd, std::shared_ptr<RenderTexture> sceneTexture, float threshold, float knee, glm::vec2 sunUV, float sunIntensity, glm::vec2 moonUV, float moonIntensity)
    {
        m_DownsamplePipeline->Bind(cmd);
        
        for (uint32_t i { 0 }; i < m_MipCount; i++)
        {
            uint32_t mipWidth { m_DownChain[i]->GetWidth() };
            uint32_t mipHeight { m_DownChain[i]->GetHeight() };

            std::array<VkClearValue, 2> clearValues {};
            clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
            clearValues[1].depthStencil = { 1.0f, 0 };

            VkRenderPassBeginInfo renderPassInfo {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_DownChain[i]->GetRenderPass();
            renderPassInfo.framebuffer = m_DownChain[i]->GetFramebuffer();
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = { mipWidth, mipHeight };
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport {};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(mipWidth);
            viewport.height = static_cast<float>(mipHeight);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor {};
            scissor.offset = { 0, 0 };
            scissor.extent = { mipWidth, mipHeight };
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            VkDescriptorSet set { (i == 0) ? m_SceneDescriptorSet : m_DownDescriptorSets[i - 1] };
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DownPipelineLayout, 0, 1, &set, 0, nullptr);

            DownPushConstants push {};
            push.texelWidth = 1.0f / static_cast<float>((i == 0) ? sceneTexture->GetWidth() : m_DownChain[i - 1]->GetWidth());
            push.texelHeight = 1.0f / static_cast<float>((i == 0) ? sceneTexture->GetHeight() : m_DownChain[i - 1]->GetHeight());
            push.threshold = (i == 0) ? threshold : 0.0f; 
            push.knee = knee;

            vkCmdPushConstants(cmd, m_DownPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DownPushConstants), &push);
            vkCmdDraw(cmd, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmd);
        }

        m_UpsamplePipeline->Bind(cmd);

        for (int32_t i { static_cast<int32_t>(m_MipCount - 1) }; i >= 0; i--)
        {
            uint32_t mipWidth { m_UpChain[i]->GetWidth() };
            uint32_t mipHeight { m_UpChain[i]->GetHeight() };

            std::array<VkClearValue, 2> clearValues {};
            clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
            clearValues[1].depthStencil = { 1.0f, 0 };

            VkRenderPassBeginInfo renderPassInfo {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_UpChain[i]->GetRenderPass();
            renderPassInfo.framebuffer = m_UpChain[i]->GetFramebuffer();
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = { mipWidth, mipHeight };
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport {};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(mipWidth);
            viewport.height = static_cast<float>(mipHeight);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor {};
            scissor.offset = { 0, 0 };
            scissor.extent = { mipWidth, mipHeight };
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_UpPipelineLayout, 0, 1, &m_UpDescriptorSets[i], 0, nullptr);

            UpPushConstants push {};
            push.texelWidth  = 1.0f / static_cast<float>(mipWidth);
            push.texelHeight = 1.0f / static_cast<float>(mipHeight);
            push.filterRadius = push.texelWidth;

            vkCmdPushConstants(cmd, m_UpPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UpPushConstants), &push);
            vkCmdDraw(cmd, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmd);
        }

        m_FlarePipeline->Bind(cmd);
       
        std::array<VkClearValue, 2> clearValues {};
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };
        
        VkRenderPassBeginInfo flarePassInfo {};
        flarePassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        flarePassInfo.renderPass = m_FlareTexture->GetRenderPass();
        flarePassInfo.framebuffer = m_FlareTexture->GetFramebuffer();
        flarePassInfo.renderArea.offset = { 0, 0 };
        flarePassInfo.renderArea.extent = { m_FlareTexture->GetWidth(), m_FlareTexture->GetHeight() };
        flarePassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        flarePassInfo.pClearValues = clearValues.data();
        vkCmdBeginRenderPass(cmd, &flarePassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        VkViewport flareViewport {};
        flareViewport.width = static_cast<float>(m_FlareTexture->GetWidth());
        flareViewport.height = static_cast<float>(m_FlareTexture->GetHeight());
        flareViewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &flareViewport);
        
        VkRect2D flareScissor {};
        flareScissor.extent = { m_FlareTexture->GetWidth(), m_FlareTexture->GetHeight() };
        vkCmdSetScissor(cmd, 0, 1, &flareScissor);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_FlarePipelineLayout, 0, 1, &m_FlareDescriptorSet, 0, nullptr);
        
        FlarePushConstants flarePush {};
        flarePush.sunUV = sunUV;
        flarePush.sunIntensity = sunIntensity;
        flarePush.moonUV = moonUV;
        flarePush.moonIntensity = moonIntensity;
        
        vkCmdPushConstants(cmd, m_FlarePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(FlarePushConstants), &flarePush);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
}
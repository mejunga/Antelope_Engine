#include <Engine/Renderer/LowLevelRenderer.hpp>
#include <Engine/Debug/Log.hpp>

#include <stdexcept>
#include <fstream>
#include <chrono>

namespace Antelope
{
    LowLevelRenderer::LowLevelRenderer(std::shared_ptr<VulkanContext> context, std::shared_ptr<SwapChain> swapChain)
        : m_Context(context), m_SwapChain(swapChain)
    {
        CreateDescriptorSetLayout();
        AE_ENGINE_TRACE("Descriptor Set Layout created.");
        CreateGraphicsPipeline();
        AE_ENGINE_INFO("Graphics Pipeline created.");
        CreateCommandPool();
        AE_ENGINE_TRACE("Command Pool created.");
        CreateCommandBuffers();
        AE_ENGINE_TRACE("Command Buffers allocated for {0} frames.", MAX_FRAMES_IN_FLIGHT);
        CreateSyncObjects();
        AE_ENGINE_TRACE("Synchronization Objects created for {0} frames.", MAX_FRAMES_IN_FLIGHT);
        CreateVertexBuffer();
        AE_ENGINE_TRACE("Device local vertex buffer created and uploaded to GPU.");
        CreateIndexBuffer();
        AE_ENGINE_TRACE("Device local index buffer created and uploaded to GPU.");
        CreateUniformBuffers();
        AE_ENGINE_TRACE("Uniform buffers created and mapped for {0} frames.", MAX_FRAMES_IN_FLIGHT);
        CreateDescriptorPool();
        AE_ENGINE_TRACE("Descriptor pool created.");
        CreateDescriptorSets();
        AE_ENGINE_TRACE("Descriptor sets allocated and bound.");
    }

    LowLevelRenderer::~LowLevelRenderer()
    {
        if(m_Context && m_Context->GetDevice() != VK_NULL_HANDLE) 
        {
            vkDeviceWaitIdle(m_Context->GetDevice());
        }

        if(m_DescriptorPool != VK_NULL_HANDLE) 
        {
            vkDestroyDescriptorPool(m_Context->GetDevice(), m_DescriptorPool, nullptr);
            AE_ENGINE_TRACE("Vulkan Descriptor Pool destroyed.");
        }

        if(!m_UniformBuffers.empty()) 
        {
            for(size_t i = 0; i < m_UniformBuffers.size(); i++) 
            {
                if(m_UniformBuffers[i] != VK_NULL_HANDLE) 
                {
                    vmaDestroyBuffer(m_Context->GetAllocator(), m_UniformBuffers[i], m_UniformBuffersAllocations[i]);
                }
            }
            m_UniformBuffers.clear();
            m_UniformBuffersAllocations.clear();
            m_UniformBuffersMapped.clear();
            AE_ENGINE_TRACE("Vulkan Uniform Buffers destroyed.");
        }

        if(m_IndexBuffer != VK_NULL_HANDLE) 
        {
            vmaDestroyBuffer(m_Context->GetAllocator(), m_IndexBuffer, m_IndexBufferAllocation);
            AE_ENGINE_TRACE("Vulkan Index Buffer destroyed.");
        }

        if(m_VertexBuffer != VK_NULL_HANDLE) 
        {
            vmaDestroyBuffer(m_Context->GetAllocator(), m_VertexBuffer, m_VertexBufferAllocation);
            AE_ENGINE_TRACE("Vulkan Vertex Buffer destroyed.");
        }

        if(!m_InFlightFences.empty()) 
        {
            for(size_t i = 0; i < m_InFlightFences.size(); i++) 
            {
                vkDestroyFence(m_Context->GetDevice(), m_InFlightFences[i], nullptr);
            }
            m_InFlightFences.clear();
            AE_ENGINE_TRACE("Vulkan In-Flight Fences destroyed.");
        }

        if(!m_RenderFinishedSemaphores.empty()) 
        {
            for(size_t i = 0; i < m_RenderFinishedSemaphores.size(); i++) 
            {
                vkDestroySemaphore(m_Context->GetDevice(), m_RenderFinishedSemaphores[i], nullptr);
            }
            m_RenderFinishedSemaphores.clear();
            AE_ENGINE_TRACE("Vulkan Render Finished Semaphores destroyed.");
        }

        if(!m_ImageAvailableSemaphores.empty()) 
        {
            for(size_t i = 0; i < m_ImageAvailableSemaphores.size(); i++) 
            {
                vkDestroySemaphore(m_Context->GetDevice(), m_ImageAvailableSemaphores[i], nullptr);
            }
            m_ImageAvailableSemaphores.clear();
            AE_ENGINE_TRACE("Vulkan Image Available Semaphores destroyed.");
        }

        if(m_CommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_Context->GetDevice(), m_CommandPool, nullptr);
            AE_ENGINE_TRACE("Vulkan Command Pool destroyed.");
        }

        if(m_GraphicsPipeline != VK_NULL_HANDLE) 
        {
            vkDestroyPipeline(m_Context->GetDevice(), m_GraphicsPipeline, nullptr);
            AE_ENGINE_TRACE("Vulkan Graphics Pipeline destroyed.");
        }
        
        if(m_PipelineLayout != VK_NULL_HANDLE) 
        {
            vkDestroyPipelineLayout(m_Context->GetDevice(), m_PipelineLayout, nullptr);
            AE_ENGINE_TRACE("Vulkan Pipeline Layout destroyed.");
        }

        if(m_DescriptorSetLayout != VK_NULL_HANDLE) 
        {
            vkDestroyDescriptorSetLayout(m_Context->GetDevice(), m_DescriptorSetLayout, nullptr);
            AE_ENGINE_TRACE("Vulkan Descriptor Set Layout destroyed.");
        }
    }

    void LowLevelRenderer::DrawFrame()
    {
        vkWaitForFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_Context->GetDevice(), m_SwapChain->GetSwapchain(), UINT64_MAX, 
                                                m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

        if(result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_SwapChain->RecreateSwapchain();
            return; 
        }
        else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            AE_ENGINE_ERROR("Failed to acquire swapchain image!");
            return;
        }

        vkResetFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);
        UpdateUniformBuffer(m_CurrentFrame);
        vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

        VkCommandBufferBeginInfo commandBufferInfo {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        if(vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &commandBufferInfo) != VK_SUCCESS) 
        {
            AE_ENGINE_ERROR("Failed to begin recording command buffer!");
            return;
        }

        VkRenderPassBeginInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_SwapChain->GetRenderPass();
        renderPassInfo.framebuffer = m_SwapChain->GetFramebuffers()[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_SwapChain->GetExtent();

        std::array<VkClearValue, 2> clearValues {};
        clearValues[0].color = {{0.01f, 0.01f, 0.03f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(m_CommandBuffers[m_CurrentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_SwapChain->GetExtent().width);
        viewport.height = static_cast<float>(m_SwapChain->GetExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(m_CommandBuffers[m_CurrentFrame], 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset = {0, 0};
        scissor.extent = m_SwapChain->GetExtent();
        vkCmdSetScissor(m_CommandBuffers[m_CurrentFrame], 0, 1, &scissor);

        VkBuffer vertexBuffers[] = { m_VertexBuffer };
        VkDeviceSize offsets[] = { 0 };

        vkCmdBindVertexBuffers(m_CommandBuffers[m_CurrentFrame], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(m_CommandBuffers[m_CurrentFrame], m_IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[m_CurrentFrame], 0, nullptr);
        vkCmdDrawIndexed(m_CommandBuffers[m_CurrentFrame], 36, 1, 0, 0, 0);
        vkCmdEndRenderPass(m_CommandBuffers[m_CurrentFrame]);

        if(vkEndCommandBuffer(m_CommandBuffers[m_CurrentFrame]) != VK_SUCCESS) 
        {
            AE_ENGINE_ERROR("Failed to record command buffer!");
            return;
        }

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

        VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if(vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) 
        {
            AE_ENGINE_ERROR("Failed to submit draw command buffer!");
            return;
        }

        VkPresentInfoKHR presentInfo {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores; 

        VkSwapchainKHR swapchains[] = { m_SwapChain->GetSwapchain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(m_Context->GetPresentQueue(), &presentInfo);

        if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_SwapChain->IsFramebufferResized())
        {
            m_SwapChain->SetFramebufferResized(false);
            m_SwapChain->RecreateSwapchain();
        }
        else if(result != VK_SUCCESS)
        {
            AE_ENGINE_ERROR("Failed to present swapchain image!");
        }
        
        vkQueueWaitIdle(m_Context->GetPresentQueue());
        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    std::vector<char> LowLevelRenderer::ReadFile(const std::string& fileName)
    {
        std::ifstream file(fileName, std::ios::ate | std::ios::binary);
        
        if(!file.is_open())
        {
            AE_ENGINE_CRITICAL("Failed to open file: {0}", fileName);
            throw std::runtime_error("Failed to open file");
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    VkShaderModule LowLevelRenderer::CreateShaderModule(const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo shaderModuleInfo {};
        shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleInfo.codeSize = code.size();
        shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        
        if(vkCreateShaderModule(m_Context->GetDevice(), &shaderModuleInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Shader Module!");
        }
        return shaderModule;
    }

    VkVertexInputBindingDescription LowLevelRenderer::GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    std::array<VkVertexInputAttributeDescription, 2> LowLevelRenderer::GetAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        return attributeDescriptions;
    }

    void LowLevelRenderer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
    {
        VkCommandBufferAllocateInfo allocationInfo {};
        allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocationInfo.commandPool = m_CommandPool;
        allocationInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_Context->GetDevice(), &allocationInfo, &commandBuffer);

        VkCommandBufferBeginInfo commandBufferBeginInfo {};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

        VkBufferCopy copyRegion {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = bufferSize;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo,VK_NULL_HANDLE);
        vkQueueWaitIdle(m_Context->GetGraphicsQueue());
        vkFreeCommandBuffers(m_Context->GetDevice(), m_CommandPool, 1, &commandBuffer); 
    }

    void LowLevelRenderer::CreateStagingBuffer(const void* data, VkDeviceSize bufferSize, VkBuffer& outBuffer, VmaAllocation& outAllocation)
    {
        VkBufferCreateInfo stagingBufferInfo {};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; 
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if(vmaCreateBuffer(m_Context->GetAllocator(), &stagingBufferInfo, &allocationInfo, &outBuffer, &outAllocation, nullptr) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create staging buffer! Size: {0} bytes.", bufferSize);
            throw std::runtime_error("Failed to create staging buffer");
        }

        vmaCopyMemoryToAllocation(m_Context->GetAllocator(), data, outAllocation, 0, bufferSize);
    }

    void LowLevelRenderer::UpdateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo {};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), m_SwapChain->GetExtent().width / (float)m_SwapChain->GetExtent().height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void LowLevelRenderer::CreateDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding {};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1; 
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; 
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        if(vkCreateDescriptorSetLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Descriptor Set Layout!");
            throw std::runtime_error("Failed to create Descriptor Set Layout");
        }
    }

    void LowLevelRenderer::CreateGraphicsPipeline()
    {
        auto vertShaderCode = ReadFile("Shaders/base.vert.spv");
        auto fragShaderCode = ReadFile("Shaders/base.frag.spv");

        VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode); 
        VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        auto bindingDescription = GetBindingDescription();
        auto attributeDescriptions = GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputStateInfo {};
        vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputStateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputStateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo {};
        inputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

        std::vector<VkDynamicState> dynamicStates {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicStateInfo {};
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateInfo.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportStateInfo {};
        viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateInfo.viewportCount = 1;
        viewportStateInfo.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizationStateInfo {};
        rasterizationStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateInfo.depthClampEnable = VK_FALSE;
        rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateInfo.lineWidth = 1.0f;
        rasterizationStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationStateInfo.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampleStateInfo {};
        multisampleStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleStateInfo.sampleShadingEnable = VK_FALSE;
        multisampleStateInfo.rasterizationSamples = m_Context->GetMsaaSamples();

        VkPipelineColorBlendAttachmentState colorBlendAttachmentStateInfo {};
        colorBlendAttachmentStateInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT 
                                                     | VK_COLOR_COMPONENT_G_BIT 
                                                     | VK_COLOR_COMPONENT_B_BIT 
                                                     | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachmentStateInfo.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachmentStateInfo;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if(vkCreatePipelineLayout(m_Context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create Pipeline Layout!");
            throw std::runtime_error("Failed to create Pipeline Layout");
        }

        VkPipelineDepthStencilStateCreateInfo depthStencilState {};
        depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilState.depthTestEnable = VK_TRUE;
        depthStencilState.depthWriteEnable = VK_TRUE;
        depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencilState.depthBoundsTestEnable = VK_FALSE;
        depthStencilState.stencilTestEnable = VK_FALSE;

        VkGraphicsPipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputStateInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyStateInfo;
        pipelineInfo.pViewportState = &viewportStateInfo;
        pipelineInfo.pRasterizationState = &rasterizationStateInfo;
        pipelineInfo.pMultisampleState = &multisampleStateInfo;
        pipelineInfo.pDepthStencilState = &depthStencilState;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicStateInfo;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = m_SwapChain->GetRenderPass();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if(vkCreateGraphicsPipelines(m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create Graphics Pipeline!");
            throw std::runtime_error("Failed to create Graphics Pipeline");
        }

        vkDestroyShaderModule(m_Context->GetDevice(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_Context->GetDevice(), vertShaderModule, nullptr);
    }

    void LowLevelRenderer::CreateCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = m_Context->FindQueueFamilies(m_Context->GetPhysicalDevice());

        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = queueFamilyIndices.GraphicsFamily.value();

        if(vkCreateCommandPool(m_Context->GetDevice(), &commandPoolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Command Pool!");
            throw std::runtime_error("Failed to create Command Pool");
        }
    }

    void LowLevelRenderer::CreateCommandBuffers()
    {
        m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocationInfo {};
        allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocationInfo.commandPool = m_CommandPool;
        allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocationInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if(vkAllocateCommandBuffers(m_Context->GetDevice(), &allocationInfo, m_CommandBuffers.data()) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to allocate Command Buffers!");
            throw std::runtime_error("Failed to allocate Command Buffers");
        }
    }

    void LowLevelRenderer::CreateSyncObjects() 
    {
        m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            if(vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
               vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
               vkCreateFence(m_Context->GetDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) 
            {
                AE_ENGINE_ERROR("Failed to create synchronization objects for frame {0}!", i);
                throw std::runtime_error("Failed to create synchronization objects");
                
            }
        }
    }

    void LowLevelRenderer::CreateVertexBuffer()
    {
        const std::array<Vertex, 8> vertices = 
        {{
            {{-0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
            
            {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}}
        }};

        VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        CreateStagingBuffer(vertices.data(), bufferSize, stagingBuffer, stagingBufferAllocation);

        VkBufferCreateInfo vertexBufferInfo {};
        vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertexBufferInfo.size = bufferSize;
        vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vertexAllocInfo {};
        vertexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if(vmaCreateBuffer(m_Context->GetAllocator(), &vertexBufferInfo, &vertexAllocInfo, &m_VertexBuffer, &m_VertexBufferAllocation, nullptr) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create device local vertex buffer!");
            throw std::runtime_error("Failed to create device local vertex buffer");
        }

        CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);
        vmaDestroyBuffer(m_Context->GetAllocator(), stagingBuffer, stagingBufferAllocation);
    }

    void LowLevelRenderer::CreateIndexBuffer()
    {
        const std::array<uint16_t, 36> indices = 
        {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4,
            0, 1, 5, 5, 4, 0,
            2, 3, 7, 7, 6, 2,
            1, 2, 6, 6, 5, 1,
            3, 0, 4, 4, 7, 3
        };

        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        CreateStagingBuffer(indices.data(), bufferSize, stagingBuffer, stagingBufferAllocation);

        VkBufferCreateInfo indexBufferInfo {};
        indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        indexBufferInfo.size = bufferSize;
        indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo indexAllocInfo {};
        indexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if(vmaCreateBuffer(m_Context->GetAllocator(), &indexBufferInfo, &indexAllocInfo, &m_IndexBuffer, &m_IndexBufferAllocation, nullptr) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create device local index buffer!");
            throw std::runtime_error("Failed to create device local index buffer");
        }
        CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);
        vmaDestroyBuffer(m_Context->GetAllocator(), stagingBuffer, stagingBufferAllocation);
    }

    void LowLevelRenderer::CreateUniformBuffers()
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_UniformBuffersAllocations.resize(MAX_FRAMES_IN_FLIGHT);
        m_UniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            VkBufferCreateInfo uniformBufferInfo {};
            uniformBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            uniformBufferInfo.size = bufferSize;
            uniformBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            uniformBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocationInfo {};
            allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT 
                                 | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VmaAllocationInfo allocationResult;

            if(vmaCreateBuffer(m_Context->GetAllocator(), &uniformBufferInfo, &allocationInfo, &m_UniformBuffers[i], &m_UniformBuffersAllocations[i], &allocationResult) != VK_SUCCESS) 
            {
                AE_ENGINE_CRITICAL("Failed to create Uniform Buffer for frame {0}!", i);
                throw std::runtime_error("Failed to create Uniform Buffers");
            }

            m_UniformBuffersMapped[i] = allocationResult.pMappedData;
        }
    }

    void LowLevelRenderer::CreateDescriptorPool()
    {
        VkDescriptorPoolSize descriptorPoolSize {};
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorPoolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo descriptorPoolInfo {};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.poolSizeCount = 1;
        descriptorPoolInfo.pPoolSizes = &descriptorPoolSize;
        descriptorPoolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if(vkCreateDescriptorPool(m_Context->GetDevice(), &descriptorPoolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create Descriptor Pool!");
            throw std::runtime_error("Failed to create Descriptor Pool");
        }
    }

    void LowLevelRenderer::CreateDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);
        
        VkDescriptorSetAllocateInfo decriptorSetallocationInfo {};
        decriptorSetallocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        decriptorSetallocationInfo.descriptorPool = m_DescriptorPool;
        decriptorSetallocationInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        decriptorSetallocationInfo.pSetLayouts = layouts.data();

        m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        
        if(vkAllocateDescriptorSets(m_Context->GetDevice(), &decriptorSetallocationInfo, m_DescriptorSets.data()) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to allocate Descriptor Sets!");
            throw std::runtime_error("Failed to allocate Descriptor Sets");
        }

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            VkDescriptorBufferInfo descriptorBufferInfo {};
            descriptorBufferInfo.buffer = m_UniformBuffers[i];
            descriptorBufferInfo.offset = 0;
            descriptorBufferInfo.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet descriptorWriteSet {};
            descriptorWriteSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWriteSet.dstSet = m_DescriptorSets[i];
            descriptorWriteSet.dstBinding = 0;
            descriptorWriteSet.dstArrayElement = 0;
            descriptorWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWriteSet.descriptorCount = 1;
            descriptorWriteSet.pBufferInfo = &descriptorBufferInfo;

            vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &descriptorWriteSet, 0, nullptr);
        }
    }
}
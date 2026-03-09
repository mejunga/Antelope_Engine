#include <Engine/Renderer/LowLevelRenderer.hpp>
#include <Engine/Renderer/VulkanContext.hpp>
#include <Engine/Renderer/SwapChain.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Renderer/Camera.hpp>

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
        CreateStorageBuffers();
        AE_ENGINE_TRACE("SSBO Storage Buffers created and uploaded to GPU.");
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
            AE_ENGINE_TRACE("Descriptor Pool destroyed.");
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
            AE_ENGINE_TRACE("Uniform Buffers destroyed.");
        }

        if(m_PosBuffer != VK_NULL_HANDLE)
        { 
            vmaDestroyBuffer(m_Context->GetAllocator(), m_PosBuffer, m_PosBufferAllocation);
        }

        if(m_ColorBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(m_Context->GetAllocator(), m_ColorBuffer, m_ColorBufferAllocation);
        }

        if(m_NormalBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(m_Context->GetAllocator(), m_NormalBuffer, m_NormalBufferAllocation);
        }
        
        if(m_FaceBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(m_Context->GetAllocator(), m_FaceBuffer, m_FaceBufferAllocation);
            AE_ENGINE_TRACE("SSBO Storage Buffer destroyed.");
        }

        if(!m_InFlightFences.empty()) 
        {
            for(size_t i = 0; i < m_InFlightFences.size(); i++) 
            {
                vkDestroyFence(m_Context->GetDevice(), m_InFlightFences[i], nullptr);
            }
            m_InFlightFences.clear();
            AE_ENGINE_TRACE("In-Flight Fences destroyed.");
        }

        if(!m_RenderFinishedSemaphores.empty()) 
        {
            for(size_t i = 0; i < m_RenderFinishedSemaphores.size(); i++) 
            {
                vkDestroySemaphore(m_Context->GetDevice(), m_RenderFinishedSemaphores[i], nullptr);
            }
            m_RenderFinishedSemaphores.clear();
            AE_ENGINE_TRACE("Render Finished Semaphores destroyed.");
        }

        if(!m_ImageAvailableSemaphores.empty()) 
        {
            for(size_t i = 0; i < m_ImageAvailableSemaphores.size(); i++) 
            {
                vkDestroySemaphore(m_Context->GetDevice(), m_ImageAvailableSemaphores[i], nullptr);
            }
            m_ImageAvailableSemaphores.clear();
            AE_ENGINE_TRACE("Image Available Semaphores destroyed.");
        }

        if(m_CommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_Context->GetDevice(), m_CommandPool, nullptr);
            AE_ENGINE_TRACE("Command Pool destroyed.");
        }

        if(m_GraphicsPipeline != VK_NULL_HANDLE) 
        {
            vkDestroyPipeline(m_Context->GetDevice(), m_GraphicsPipeline, nullptr);
            AE_ENGINE_TRACE("Graphics Pipeline destroyed.");
        }
        
        if(m_PipelineLayout != VK_NULL_HANDLE) 
        {
            vkDestroyPipelineLayout(m_Context->GetDevice(), m_PipelineLayout, nullptr);
            AE_ENGINE_TRACE("Pipeline Layout destroyed.");
        }

        if(m_DescriptorSetLayout != VK_NULL_HANDLE) 
        {
            vkDestroyDescriptorSetLayout(m_Context->GetDevice(), m_DescriptorSetLayout, nullptr);
            AE_ENGINE_TRACE("Descriptor Set Layout destroyed.");
        }
    }

    void LowLevelRenderer::DrawFrame(const UniformBufferObject& cameraData, const std::vector<RenderCommand>& renderList)
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
        UpdateUniformBuffer(m_CurrentFrame, cameraData);
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
        vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[m_CurrentFrame], 0, nullptr);
        
        for(const auto& command : renderList) 
        {
            PushConstantData pushData {};
            pushData.model = command.transform;
            pushData.posOffset    = command.mesh.posOffset;
            pushData.colorOffset  = command.mesh.colorOffset;
            pushData.normalOffset = command.mesh.normalOffset;
            pushData.faceOffset   = command.mesh.faceOffset;
            
            vkCmdPushConstants(
                m_CommandBuffers[m_CurrentFrame],
                m_PipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(PushConstantData),
                &pushData
            );
            
            uint32_t vertexCount = command.mesh.faceCount * 3;
            vkCmdDraw(m_CommandBuffers[m_CurrentFrame], vertexCount, 1, 0, 0);
        }

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

    MeshHandle LowLevelRenderer::UploadMesh(const MeshData& meshData)
    {
        MeshHandle handle {};
        
        handle.posOffset    = m_CurrentPosOffset;
        handle.colorOffset  = m_CurrentColorOffset;
        handle.normalOffset = m_CurrentNormalOffset;
        handle.faceOffset   = m_CurrentFaceOffset;
        handle.faceCount    = static_cast<uint32_t>(meshData.faces.size());

        auto uploadToBuffer = [&](const void* data, VkDeviceSize size, VkBuffer dstBuffer, VkDeviceSize offset) 
        {
            if (size == 0) return;

            VkBuffer stagingBuffer; 
            VmaAllocation stagingBufferAllocation;
            CreateStagingBuffer(data, size, stagingBuffer, stagingBufferAllocation);

            VkCommandBufferAllocateInfo allocInfo {};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = m_CommandPool;
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer;
            vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, &commandBuffer);

            VkCommandBufferBeginInfo beginInfo {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            VkBufferCopy copyRegion {};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = offset;
            copyRegion.size = size;
            vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuffer, 1, &copyRegion);

            vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(m_Context->GetGraphicsQueue());

            vkFreeCommandBuffers(m_Context->GetDevice(), m_CommandPool, 1, &commandBuffer);
            vmaDestroyBuffer(m_Context->GetAllocator(), stagingBuffer, stagingBufferAllocation);
        };

        VkDeviceSize posSize    = sizeof(VertexPosition) * meshData.positions.size();
        VkDeviceSize colorSize  = sizeof(VertexColor)    * meshData.colors.size();
        VkDeviceSize normalSize = sizeof(VertexNormal)   * meshData.normals.size();
        VkDeviceSize faceSize   = sizeof(Face)           * meshData.faces.size();

        uploadToBuffer(meshData.positions.data(), posSize, m_PosBuffer, m_CurrentPosOffset * sizeof(VertexPosition));
        uploadToBuffer(meshData.colors.data(), colorSize, m_ColorBuffer, m_CurrentColorOffset * sizeof(VertexColor));
        uploadToBuffer(meshData.normals.data(), normalSize, m_NormalBuffer, m_CurrentNormalOffset * sizeof(VertexNormal));
        uploadToBuffer(meshData.faces.data(), faceSize, m_FaceBuffer, m_CurrentFaceOffset * sizeof(Face));

        m_CurrentPosOffset    += meshData.positions.size();
        m_CurrentColorOffset  += meshData.colors.size();
        m_CurrentNormalOffset += meshData.normals.size();
        m_CurrentFaceOffset   += meshData.faces.size();

        return handle;
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

    void LowLevelRenderer::UpdateUniformBuffer(uint32_t currentImage, const UniformBufferObject& cameraData)
    {
        memcpy(m_UniformBuffersMapped[currentImage], &cameraData, sizeof(cameraData));
    }

    void LowLevelRenderer::CreateDescriptorSetLayout()
    {
        std::array<VkDescriptorSetLayoutBinding, 5> bindings{};

        bindings[0].binding = 0; 
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; 
        bindings[0].descriptorCount = 1; 
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        for(int i = 1; i <= 4; i++) {
            bindings[i].binding = i; 
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; 
            bindings[i].descriptorCount = 1; 
            bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

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

        VkPipelineVertexInputStateCreateInfo vertexInputStateInfo {};
        vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateInfo.vertexBindingDescriptionCount = 0;
        vertexInputStateInfo.pVertexBindingDescriptions = nullptr;
        vertexInputStateInfo.vertexAttributeDescriptionCount = 0;
        vertexInputStateInfo.pVertexAttributeDescriptions = nullptr;

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
        rasterizationStateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstantData);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

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

    void LowLevelRenderer::CreateStorageBuffers()
    {
        const VkDeviceSize MEGA_BUFFER_SIZE = 10 * 1024 * 1024; 

        auto createEmptySSBO = [&](VkDeviceSize size, VkBuffer& outBuffer, VmaAllocation& outAllocation) 
        {
            VkBufferCreateInfo bufferInfo {}; 
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size; 
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            
            VmaAllocationCreateInfo allocationInfo {}; 
            allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
            
            if (vmaCreateBuffer(m_Context->GetAllocator(), &bufferInfo, &allocationInfo, &outBuffer, &outAllocation, nullptr) != VK_SUCCESS)
            {
                AE_ENGINE_CRITICAL("Failed to create Mega SSBO buffer!");
                throw std::runtime_error("Failed to create Mega SSBO buffer");
            }
        };

        createEmptySSBO(MEGA_BUFFER_SIZE, m_PosBuffer, m_PosBufferAllocation);
        createEmptySSBO(MEGA_BUFFER_SIZE, m_ColorBuffer, m_ColorBufferAllocation);
        createEmptySSBO(MEGA_BUFFER_SIZE, m_NormalBuffer, m_NormalBufferAllocation);
        createEmptySSBO(MEGA_BUFFER_SIZE, m_FaceBuffer, m_FaceBufferAllocation);
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
        std::array<VkDescriptorPoolSize, 2> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 4);

        VkDescriptorPoolCreateInfo descriptorPoolInfo {};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.poolSizeCount = 2;
        descriptorPoolInfo.pPoolSizes = poolSizes.data();
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
            VkDescriptorBufferInfo uboInfo { m_UniformBuffers[i], 0, sizeof(UniformBufferObject) };
            VkDescriptorBufferInfo posInfo { m_PosBuffer, 0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo colInfo { m_ColorBuffer, 0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo normInfo { m_NormalBuffer, 0, VK_WHOLE_SIZE };
            VkDescriptorBufferInfo faceInfo { m_FaceBuffer, 0, VK_WHOLE_SIZE };

            std::array<VkWriteDescriptorSet, 5> writes{};
            for(int j=0; j<5; j++) {
                writes[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[j].dstSet = m_DescriptorSets[i];
                writes[j].dstBinding = j;
                writes[j].dstArrayElement = 0;
                writes[j].descriptorCount = 1;
            }
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; writes[0].pBufferInfo = &uboInfo;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[1].pBufferInfo = &posInfo;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[2].pBufferInfo = &colInfo;
            writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[3].pBufferInfo = &normInfo;
            writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[4].pBufferInfo = &faceInfo;

            vkUpdateDescriptorSets(m_Context->GetDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }
}
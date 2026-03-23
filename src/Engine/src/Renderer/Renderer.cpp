#include <Engine/Renderer/Renderer.hpp>
#include <Engine/Renderer/VulkanContext.hpp>
#include <Engine/Renderer/SwapChain.hpp>
#include <Engine/Renderer/Camera.hpp>
#include <Engine/Renderer/TextureManager.hpp>
#include <Engine/Renderer/VulkanBuffer.hpp>
#include <Engine/Renderer/VulkanPipeline.hpp>
#include <Engine/Renderer/VulkanDescriptor.hpp>
#include <Engine/Renderer/GpuMemoryAllocator.hpp>
#include <Engine/Debug/Log.hpp>

#include <stdexcept>
#include <fstream>
#include <chrono>


namespace Antelope
{
    Renderer::Renderer(std::shared_ptr<VulkanContext> context, std::shared_ptr<SwapChain> swapChain)
        : m_Context(context), m_SwapChain(swapChain)
    {
        m_GpuAllocator = std::make_unique<GpuMemoryAllocator>(m_Context);
        m_GlobalDescriptorAllocator = std::make_unique<DescriptorAllocator>();
        std::vector<DescriptorAllocator::PoolSizeRatio> ratios = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6.0f },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024.0f }
        };
        m_GlobalDescriptorAllocator->Init(m_Context, MAX_FRAMES_IN_FLIGHT, ratios);

        CreateDescriptorSetLayout();
        AE_ENGINE_TRACE("Descriptor Set Layout created.");
        CreatePipelineLayout();
        AE_ENGINE_TRACE("Pipeline Layout created.");
        CreateGraphicsPipeline();
        AE_ENGINE_INFO("Graphics Pipeline created.");
        CreateCommandPool();
        AE_ENGINE_TRACE("Command Pool created.");
        CreateTransferCommandPool();
        AE_ENGINE_TRACE("Transfer Command Pool created.");
        CreateCommandBuffers();
        AE_ENGINE_TRACE("Command Buffers allocated for {0} frames.", MAX_FRAMES_IN_FLIGHT);
        CreateSyncObjects();
        AE_ENGINE_TRACE("Synchronization Objects created for {0} frames.", MAX_FRAMES_IN_FLIGHT);
        CreateUniformBuffers();
        AE_ENGINE_TRACE("Uniform buffers created and mapped for {0} frames.", MAX_FRAMES_IN_FLIGHT);
        CreateObjectBuffers();
        AE_ENGINE_TRACE("Object buffers created and mapped for {0} frames.", MAX_FRAMES_IN_FLIGHT);
        CreateIndirectBuffers();
        AE_ENGINE_TRACE("Indirect command buffers created and mapped for {0} frames.", MAX_FRAMES_IN_FLIGHT);
        CreateDescriptorPool();
        AE_ENGINE_TRACE("Descriptor pool created.");
        CreateDescriptorSets();
        AE_ENGINE_TRACE("Descriptor sets allocated and bound.");
    }

    Renderer::~Renderer()
    {
        if (m_Context && m_Context->GetDevice() != VK_NULL_HANDLE) 
        {
            vkDeviceWaitIdle(m_Context->GetDevice());
        }

        for (auto& transfer : m_PendingTransfers)
        {
            vkDestroyFence(m_Context->GetDevice(), transfer.fence, nullptr);
            vkFreeCommandBuffers(m_Context->GetDevice(), m_TransferCommandPool, 1, &transfer.commandBuffer);
            vmaDestroyBuffer(m_Context->GetAllocator(), transfer.stagingBuffer, transfer.stagingAllocation);
        }

        m_PendingTransfers.clear();
        m_PendingMeshOffsets.clear();

        if (m_DescriptorPool != VK_NULL_HANDLE) 
        {
            vkDestroyDescriptorPool(m_Context->GetDevice(), m_DescriptorPool, nullptr);
            AE_ENGINE_TRACE("Descriptor Pool destroyed.");
        }

        if (!m_InFlightFences.empty()) 
        {
            for (size_t i = 0; i < m_InFlightFences.size(); i++) 
            {
                vkDestroyFence(m_Context->GetDevice(), m_InFlightFences[i], nullptr);
            }

            m_InFlightFences.clear();
            AE_ENGINE_TRACE("In-Flight Fences destroyed.");
        }

        if (!m_RenderFinishedSemaphores.empty()) 
        {
            for (size_t i = 0; i < m_RenderFinishedSemaphores.size(); i++) 
            {
                vkDestroySemaphore(m_Context->GetDevice(), m_RenderFinishedSemaphores[i], nullptr);
            }

            m_RenderFinishedSemaphores.clear();
            AE_ENGINE_TRACE("Render Finished Semaphores destroyed.");
        }

        if (!m_ImageAvailableSemaphores.empty()) 
        {
            for (size_t i = 0; i < m_ImageAvailableSemaphores.size(); i++) 
            {
                vkDestroySemaphore(m_Context->GetDevice(), m_ImageAvailableSemaphores[i], nullptr);
            }

            m_ImageAvailableSemaphores.clear();
            AE_ENGINE_TRACE("Image Available Semaphores destroyed.");
        }

        if (m_TransferCommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_Context->GetDevice(), m_TransferCommandPool, nullptr);
            AE_ENGINE_TRACE("Transfer Command Pool destroyed.");
        }

        if (m_CommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_Context->GetDevice(), m_CommandPool, nullptr);
            AE_ENGINE_TRACE("Command Pool destroyed.");
        }
        
        if (m_PipelineLayout != VK_NULL_HANDLE) 
        {
            vkDestroyPipelineLayout(m_Context->GetDevice(), m_PipelineLayout, nullptr);
            AE_ENGINE_TRACE("Pipeline Layout destroyed.");
        }

        if (m_DescriptorSetLayout != VK_NULL_HANDLE) 
        {
            vkDestroyDescriptorSetLayout(m_Context->GetDevice(), m_DescriptorSetLayout, nullptr);
            AE_ENGINE_TRACE("Descriptor Set Layout destroyed.");
        }

        if (m_GlobalDescriptorAllocator)
        {
            m_GlobalDescriptorAllocator->DestroyAllocator();
        }
    }

    void Renderer::DrawFrame(const UniformBufferObject& cameraData, const std::vector<RenderCommand>& renderList)
    {
        VkCommandBuffer cmd = BeginFrame();
        if (cmd == VK_NULL_HANDLE) return;
        DrawObjects(cmd, cameraData, renderList);
        EndFrame(cmd);
    }

    MeshHandle Renderer::UploadMesh(const MeshData& meshData)
    {
       VkDeviceSize posSize    = sizeof(VertexPosition) * meshData.positions.size();
        VkDeviceSize colorSize  = sizeof(VertexColor)    * meshData.colors.size();
        VkDeviceSize normalSize = sizeof(VertexNormal)   * meshData.normals.size();
        VkDeviceSize uvSize     = sizeof(VertexUV)       * meshData.uvs.size();
        VkDeviceSize faceSize   = sizeof(Face)           * meshData.faces.size();
        VkDeviceSize totalSize  = posSize + colorSize + normalSize + faceSize + uvSize;

        MeshHandle handle {};

        if (totalSize == 0) { return handle; }

        handle.posAllocation = m_GpuAllocator->AllocatePosition(posSize);
        handle.colorAllocation = m_GpuAllocator->AllocateColor(colorSize);
        handle.normalAllocation = m_GpuAllocator->AllocateNormal(normalSize);
        handle.uvAllocation     = m_GpuAllocator->AllocateUV(uvSize);
        handle.faceAllocation = m_GpuAllocator->AllocateFace(faceSize);
        handle.faceCount = static_cast<uint32_t>(meshData.faces.size());

        std::vector<char> combinedData(totalSize);
        size_t offset = 0;

        if (posSize > 0) { memcpy(combinedData.data() + offset, meshData.positions.data(), posSize); offset += posSize; }
        if (colorSize > 0) { memcpy(combinedData.data() + offset, meshData.colors.data(), colorSize); offset += colorSize; }
        if (normalSize > 0) { memcpy(combinedData.data() + offset, meshData.normals.data(), normalSize); offset += normalSize; }
        if (uvSize > 0) { memcpy(combinedData.data() + offset, meshData.uvs.data(), uvSize); offset += uvSize; }
        if (faceSize > 0) { memcpy(combinedData.data() + offset, meshData.faces.data(), faceSize); }

        VkBuffer stagingBuffer; 
        VmaAllocation stagingAllocation;
        CreateStagingBuffer(combinedData.data(), totalSize, stagingBuffer, stagingAllocation);

        VkCommandBufferAllocateInfo allocationInfo {};
        allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocationInfo.commandPool = m_TransferCommandPool;
        allocationInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_Context->GetDevice(), &allocationInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion {};
        VkDeviceSize srcOffset = 0;
        
        auto addCopy = [&](VkBuffer dstBuf, VkDeviceSize dstOffset, VkDeviceSize size) {
            if (size > 0) {
                copyRegion.srcOffset = srcOffset;
                copyRegion.dstOffset = dstOffset;
                copyRegion.size = size;
                vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuf, 1, &copyRegion);
                srcOffset += size;
            }
        };

        VkBuffer targetPosBuffer = m_GpuAllocator->GetPosBuffer()->GetBuffer(handle.posAllocation.PageIndex);
        VkBuffer targetColBuffer = m_GpuAllocator->GetColorBuffer()->GetBuffer(handle.colorAllocation.PageIndex);
        VkBuffer targetNormBuffer = m_GpuAllocator->GetNormalBuffer()->GetBuffer(handle.normalAllocation.PageIndex);
        VkBuffer targetUvBuffer = m_GpuAllocator->GetUvBuffer()->GetBuffer(handle.uvAllocation.PageIndex);
        VkBuffer targetFaceBuffer = m_GpuAllocator->GetFaceBuffer()->GetBuffer(handle.faceAllocation.PageIndex);

        addCopy(targetPosBuffer, handle.posAllocation.Offset, posSize);
        addCopy(targetColBuffer, handle.colorAllocation.Offset, colorSize);
        addCopy(targetNormBuffer, handle.normalAllocation.Offset, normalSize);
        addCopy(targetUvBuffer, handle.uvAllocation.Offset, uvSize);
        addCopy(targetFaceBuffer, handle.faceAllocation.Offset, faceSize);

        vkEndCommandBuffer(commandBuffer);

        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence transferFence;
        vkCreateFence(m_Context->GetDevice(), &fenceInfo, nullptr, &transferFence);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_Context->GetTransferQueue(), 1, &submitInfo, transferFence);

        m_PendingTransfers.push_back({transferFence, commandBuffer, stagingBuffer, stagingAllocation, static_cast<uint32_t>(handle.posAllocation.Offset)});
        m_PendingMeshOffsets.insert(static_cast<uint32_t>(handle.posAllocation.Offset)); 

        return handle;
    }

    void Renderer::FreeMesh(const MeshHandle& handle)
    {
        m_GpuAllocator->FreePosition(handle.posAllocation);
        m_GpuAllocator->FreeColor(handle.colorAllocation);
        m_GpuAllocator->FreeNormal(handle.normalAllocation);
        m_GpuAllocator->FreeUV(handle.uvAllocation);
        m_GpuAllocator->FreeFace(handle.faceAllocation);
        AE_ENGINE_TRACE("Mesh freed from GPU Buffers. Space reclaimed.");
    }

    void Renderer::UpdateTextureDescriptors(const std::vector<Texture>& textures)
    {
        if (textures.empty()) return;

        std::vector<VkDescriptorImageInfo> imageInfos;
        for (const auto& tex : textures)
        {
            imageInfos.push_back({tex.Sampler, tex.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_DescriptorSets[i];
            descriptorWrite.dstBinding = 7;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = static_cast<uint32_t>(imageInfos.size());
            descriptorWrite.pImageInfo = imageInfos.data();

            vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &descriptorWrite, 0, nullptr);
        }
        AE_ENGINE_INFO("Global Texture Descriptor Array updated. Count: {0}", textures.size());
    }

    void Renderer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
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

    void Renderer::CreateStagingBuffer(const void* data, VkDeviceSize bufferSize, VkBuffer& outBuffer, VmaAllocation& outAllocation)
    {
        VkBufferCreateInfo stagingBufferInfo {};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; 
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if (vmaCreateBuffer(m_Context->GetAllocator(), &stagingBufferInfo, &allocationInfo, &outBuffer, &outAllocation, nullptr) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create staging buffer! Size: {0} bytes.", bufferSize);
            throw std::runtime_error("Failed to create staging buffer");
        }

        vmaCopyMemoryToAllocation(m_Context->GetAllocator(), data, outAllocation, 0, bufferSize);
    }

    void Renderer::UpdateUniformBuffer(uint32_t currentImage, const UniformBufferObject& cameraData)
    {
        m_UniformBuffers[currentImage]->WriteToBuffer((void*)&cameraData, sizeof(cameraData));
    }

    void Renderer::CreateDescriptorSetLayout() 
    {
        DescriptorLayoutBuilder builder;
        
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        
        for (int i = 1; i <= 6; ++i)
        {
            builder.AddBinding(i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        builder.AddBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1024);

        std::array<VkDescriptorBindingFlags, 8> bindingFlags {};
        for (int i = 0; i < 7; i++) 
        {
            bindingFlags[i] = 0;
        }
        bindingFlags[7] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo {};
        flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
        flagsInfo.pBindingFlags = bindingFlags.data();

        m_DescriptorSetLayout = builder.Build(m_Context, &flagsInfo, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }

    void Renderer::CreatePipelineLayout()
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(m_Context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create Pipeline Layout!");
            throw std::runtime_error("Failed to create Pipeline Layout");
        }
    }

    void Renderer::ProcessPendingTransfers()
    {
        for (auto it = m_PendingTransfers.begin(); it != m_PendingTransfers.end(); ) 
        {
            if (vkGetFenceStatus(m_Context->GetDevice(), it->fence) == VK_SUCCESS) 
            {
                vkDestroyFence(m_Context->GetDevice(), it->fence, nullptr);
                vkFreeCommandBuffers(m_Context->GetDevice(), m_TransferCommandPool, 1, &it->commandBuffer);
                vmaDestroyBuffer(m_Context->GetAllocator(), it->stagingBuffer, it->stagingAllocation);
                
                m_PendingMeshOffsets.erase(it->posOffset); 
                
                it = m_PendingTransfers.erase(it);
            }
            else 
            {
                ++it;
            }
        }
    }

    VkCommandBuffer Renderer::BeginFrame()
    {
        ProcessPendingTransfers();
        vkWaitForFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(m_Context->GetDevice(), m_SwapChain->GetSwapchain(), UINT64_MAX, 
                                                m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_CurrentImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_SwapChain->RecreateSwapchain();
            return VK_NULL_HANDLE; 
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            AE_ENGINE_ERROR("Failed to acquire swapchain image!");
            return VK_NULL_HANDLE;
        }

        vkResetFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);
        vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

        VkCommandBufferBeginInfo commandBufferInfo {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        if (vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &commandBufferInfo) != VK_SUCCESS) 
        {
            AE_ENGINE_ERROR("Failed to begin recording command buffer!");
            return VK_NULL_HANDLE;
        }

        return m_CommandBuffers[m_CurrentFrame];
    }

    void Renderer::DrawObjects(VkCommandBuffer cmd, const UniformBufferObject& cameraData, const std::vector<RenderCommand>& renderList)
    {
        UpdateUniformBuffer(m_CurrentFrame, cameraData);

        VkRenderPassBeginInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_SwapChain->GetRenderPass();
        renderPassInfo.framebuffer = m_SwapChain->GetFramebuffers()[m_CurrentImageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_SwapChain->GetExtent();

        std::array<VkClearValue, 2> clearValues {};
        clearValues[0].color = {{0.01f, 0.01f, 0.03f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        m_MainPipeline->Bind(cmd);

        VkViewport viewport {};
        viewport.x = 0.0f; viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_SwapChain->GetExtent().width);
        viewport.height = static_cast<float>(m_SwapChain->GetExtent().height);
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset = {0, 0}; scissor.extent = m_SwapChain->GetExtent();
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[m_CurrentFrame], 0, nullptr);
        
        ObjectData* objectDataMap = static_cast<ObjectData*>(m_ObjectBuffers[m_CurrentFrame]->GetMappedMemory());
        VkDrawIndirectCommand* indirectCommandsMap = static_cast<VkDrawIndirectCommand*>(m_IndirectBuffers[m_CurrentFrame]->GetMappedMemory());

        uint32_t objectCount = 0;

        for (size_t i = 0; i < renderList.size(); i++) 
        {
            const auto& command = renderList[i];
            if (m_PendingMeshOffsets.count(static_cast<uint32_t>(command.mesh.posAllocation.Offset)) > 0) { continue; }

            objectDataMap[objectCount].model = command.transform;
            objectDataMap[objectCount].posOffset = static_cast<uint32_t>(command.mesh.posAllocation.Offset / sizeof(VertexPosition));
            objectDataMap[objectCount].colorOffset = static_cast<uint32_t>(command.mesh.colorAllocation.Offset / sizeof(VertexColor));
            objectDataMap[objectCount].normalOffset = static_cast<uint32_t>(command.mesh.normalAllocation.Offset / sizeof(VertexNormal));
            objectDataMap[objectCount].faceOffset = static_cast<uint32_t>(command.mesh.faceAllocation.Offset / sizeof(Face));
            objectDataMap[objectCount].uvOffset = static_cast<uint32_t>(command.mesh.uvAllocation.Offset / sizeof(VertexUV));
            objectDataMap[objectCount].materialIndex = command.mesh.materialIndex;

            indirectCommandsMap[objectCount].vertexCount = command.mesh.faceCount * 3;
            indirectCommandsMap[objectCount].instanceCount = 1;
            indirectCommandsMap[objectCount].firstVertex = 0;
            indirectCommandsMap[objectCount].firstInstance = objectCount; 
            objectCount++;
        }

        if (objectCount > 0) {
            vkCmdDrawIndirect(cmd, m_IndirectBuffers[m_CurrentFrame]->GetBuffer(), 0, objectCount, sizeof(VkDrawIndirectCommand));
        }

        vkCmdEndRenderPass(cmd);
    }

    void Renderer::EndFrame(VkCommandBuffer cmd)
    {
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) 
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
        submitInfo.pCommandBuffers = &cmd;

        VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentImageIndex] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) 
        {
            AE_ENGINE_ERROR("Failed to submit draw command buffer!");
            return;
        }

        VkPresentInfoKHR presentInfo {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores; 

        VkSwapchainKHR swapchains[] {{ m_SwapChain->GetSwapchain() }};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &m_CurrentImageIndex;

        VkResult result = vkQueuePresentKHR(m_Context->GetPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_SwapChain->IsFramebufferResized())
        {
            m_SwapChain->SetFramebufferResized(false);
            m_SwapChain->RecreateSwapchain();
        }
        else if (result != VK_SUCCESS)
        {
            AE_ENGINE_ERROR("Failed to present swapchain image!");
        }
        
        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void Renderer::CreateGraphicsPipeline()
    {
        PipelineConfigInfo pipelineConfig {};
        VulkanPipeline::DefaultPipelineConfigInfo(pipelineConfig, m_Context);
        
        pipelineConfig.pipelineLayout = m_PipelineLayout;
        pipelineConfig.renderPass = m_SwapChain->GetRenderPass();
        
        m_MainPipeline = std::make_unique<VulkanPipeline>(
            m_Context,
            "Assets/Shaders/base.vert.spv",
            "Assets/Shaders/base.frag.spv",
            pipelineConfig
        );
    }

    void Renderer::CreateCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = m_Context->FindQueueFamilies(m_Context->GetPhysicalDevice());

        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = queueFamilyIndices.GraphicsFamily.value();

        if (vkCreateCommandPool(m_Context->GetDevice(), &commandPoolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Command Pool!");
            throw std::runtime_error("Failed to create Command Pool");
        }
    }

    void Renderer::CreateCommandBuffers()
    {
        m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocationInfo {};
        allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocationInfo.commandPool = m_CommandPool;
        allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocationInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkAllocateCommandBuffers(m_Context->GetDevice(), &allocationInfo, m_CommandBuffers.data()) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to allocate Command Buffers!");
            throw std::runtime_error("Failed to allocate Command Buffers");
        }
    }

    void Renderer::CreateSyncObjects() 
    {
        m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        uint32_t imageCount = static_cast<uint32_t>(m_SwapChain->GetFramebuffers().size());
        m_RenderFinishedSemaphores.resize(imageCount);

        VkSemaphoreCreateInfo semaphoreInfo {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            if (vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_Context->GetDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) 
            {
                AE_ENGINE_ERROR("Failed to create synchronization objects!");
                throw std::runtime_error("Failed to create synchronization objects");
            }
        }

        for (size_t i = 0; i < imageCount; i++)
        {
            if (vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS)
            {
                AE_ENGINE_ERROR("Failed to create RenderFinished semaphores!");
                throw std::runtime_error("Failed to create RenderFinished semaphores");
            }
        }
    }

    void Renderer::CreateUniformBuffers()
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);
        m_UniformBuffers.reserve(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            m_UniformBuffers.push_back(std::make_unique<VulkanBuffer>(
                m_Context, 
                bufferSize, 
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VMA_MEMORY_USAGE_AUTO, 
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            ));
        }
    }

    void Renderer::CreateObjectBuffers()
    {
        VkDeviceSize bufferSize = sizeof(ObjectData) * MAX_OBJECTS;
        m_ObjectBuffers.reserve(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            m_ObjectBuffers.push_back(std::make_unique<VulkanBuffer>(
                m_Context, 
                bufferSize, 
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
                VMA_MEMORY_USAGE_AUTO, 
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            ));
        }
    }

    void Renderer::CreateIndirectBuffers()
    {
        VkDeviceSize bufferSize = sizeof(VkDrawIndirectCommand) * MAX_OBJECTS;
        m_IndirectBuffers.reserve(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
        {
            m_IndirectBuffers.push_back(std::make_unique<VulkanBuffer>(
                m_Context, 
                bufferSize, 
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, 
                VMA_MEMORY_USAGE_AUTO, 
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            ));
        }
    }

    void Renderer::CreateDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 3> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 6);

        poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[2].descriptorCount = 1024 * MAX_FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo descriptorPoolInfo {};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT; 
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolInfo.pPoolSizes = poolSizes.data();
        descriptorPoolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(m_Context->GetDevice(), &descriptorPoolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create Descriptor Pool!");
            throw std::runtime_error("Failed to create Descriptor Pool");
        }
    }

    void Renderer::CreateDescriptorSets()
    {
        m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
        {
            m_DescriptorSets[i] = m_GlobalDescriptorAllocator->Allocate(m_DescriptorSetLayout);

            DescriptorWriter writer;
            writer.WriteBuffer(0, m_UniformBuffers[i]->GetBuffer(), sizeof(UniformBufferObject), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            writer.WriteBuffer(1, m_GpuAllocator->GetPosBuffer()->GetBuffer(0),    VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(2, m_GpuAllocator->GetColorBuffer()->GetBuffer(0),  VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(3, m_GpuAllocator->GetNormalBuffer()->GetBuffer(0), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(4, m_GpuAllocator->GetFaceBuffer()->GetBuffer(0),   VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(5, m_GpuAllocator->GetUvBuffer()->GetBuffer(0),     VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(6, m_ObjectBuffers[i]->GetBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.UpdateSet(m_Context, m_DescriptorSets[i]);
        }
    }

    void Renderer::CreateTransferCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = m_Context->FindQueueFamilies(m_Context->GetPhysicalDevice());

        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.TransferFamily.value(); 

        if (vkCreateCommandPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_TransferCommandPool) != VK_SUCCESS) {
            AE_ENGINE_CRITICAL("Failed to create Transfer Command Pool!");
            throw std::runtime_error("Failed to create Transfer Command Pool");
        }
    }
}
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Graphics/SkyRenderer.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp>
#include <Engine/Asset/TextureManager.hpp>
#include <Engine/Renderer/Vulkan/Buffer.hpp>
#include <Engine/Renderer/Vulkan/Pipeline.hpp>
#include <Engine/Renderer/Vulkan/Descriptor.hpp>
#include <Engine/Renderer/Vulkan/GpuMemoryAllocator.hpp>
#include <Engine/Debug/Log.hpp>
#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Renderer/UI/UIContext.hpp>
#include <Engine/Renderer/Graphics/GridRenderer.hpp>
#include <Engine/Renderer/Graphics/OutlineRenderer.hpp>
#endif

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>


namespace Antelope
{
    Renderer::Renderer(std::shared_ptr<VulkanContext> context, std::shared_ptr<SwapChain> swapChain, uint32_t framesInFlight)
        : m_Context(context), m_SwapChain(swapChain), m_MaxFramesInFlight(framesInFlight)
    {
    #ifdef ANTELOPE_EDITOR_MODE
        m_Maintenance1Supported = m_Context->IsSwapchainMaintenance1Supported();
        m_RenderTexture = std::make_shared<RenderTexture>(
            m_Context,
            m_SwapChain->GetExtent().width,
            m_SwapChain->GetExtent().height,
            m_SwapChain->GetImageFormat()
        );
    #endif

        m_LastUpdatedTextureCount.assign(m_MaxFramesInFlight, 0);
        m_GpuAllocator = std::make_unique<GpuMemoryAllocator>(m_Context);
        m_GlobalDescriptorAllocator = std::make_unique<DescriptorAllocator>();
        std::vector<DescriptorAllocator::PoolSizeRatio> ratios {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6.0f },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024.0f }
        };
        m_GlobalDescriptorAllocator->Init(m_Context, m_MaxFramesInFlight, ratios);

        CreateDescriptorSetLayout();
        AE_ENGINE_TRACE("Descriptor Set Layout created.");
        CreatePipelineLayout();
        AE_ENGINE_TRACE("Pipeline Layout created.");
        CreateGraphicsPipeline();
        AE_ENGINE_INFO("Graphics Pipeline created.");
    #ifdef ANTELOPE_EDITOR_MODE
        VkRenderPass sceneRenderPass { m_RenderTexture->GetRenderPass() };
    #else
        VkRenderPass sceneRenderPass { m_SwapChain->GetRenderPass() };
    #endif
        m_SkyRenderer = std::make_unique<SkyRenderer>(m_Context, m_PipelineLayout, sceneRenderPass);
        AE_ENGINE_TRACE("Sky Renderer created.");
    #ifdef ANTELOPE_EDITOR_MODE
        m_GridRenderer = std::make_unique<GridRenderer>(m_Context, m_PipelineLayout, sceneRenderPass);
        AE_ENGINE_TRACE("Grid Renderer created.");
        m_OutlineRenderer = std::make_unique<OutlineRenderer>(m_Context, m_PipelineLayout, m_RenderTexture);
        AE_ENGINE_TRACE("Outline Renderer created.");
    #endif
        CreateCommandPool();
        AE_ENGINE_TRACE("Command Pool created.");
        CreateTransferCommandPool();
        AE_ENGINE_TRACE("Transfer Command Pool created.");
        CreateCommandBuffers();
        AE_ENGINE_TRACE("Command Buffers allocated for {0} frames.", m_MaxFramesInFlight);
        CreateSyncObjects();
        AE_ENGINE_TRACE("Synchronization Objects created for {0} frames.", m_MaxFramesInFlight);
        CreateUniformBuffers();
        AE_ENGINE_TRACE("Uniform buffers created and mapped for {0} frames.", m_MaxFramesInFlight);
        CreateObjectBuffers();
        AE_ENGINE_TRACE("Object buffers created and mapped for {0} frames.", m_MaxFramesInFlight);
        CreateIndirectBuffers();
        AE_ENGINE_TRACE("Indirect command buffers created and mapped for {0} frames.", m_MaxFramesInFlight);
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
            VkCommandPool poolToUse { (transfer.meshID == 0) ? m_CommandPool : m_TransferCommandPool };
            vkFreeCommandBuffers(m_Context->GetDevice(), poolToUse, 1, &transfer.commandBuffer);
            vmaDestroyBuffer(m_Context->GetAllocator(), transfer.stagingBuffer, transfer.stagingAllocation);
        }

        m_PendingTransfers.clear();
        m_PendingMeshIDs.clear();

        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Context->GetDevice(), m_DescriptorPool, nullptr);
            AE_ENGINE_TRACE("Descriptor Pool destroyed.");
        }

        DestroySyncObjects();

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
        VkCommandBuffer cmd { BeginFrame() };
        if (cmd == VK_NULL_HANDLE) { return; }
        DrawObjects(cmd, cameraData, renderList);

    #ifdef ANTELOPE_EDITOR_MODE
        if (auto uiContext { m_UIContext.lock() })
        {
            uiContext->RecordCommands(cmd, m_CurrentImageIndex);
        }
    #endif

        EndFrame(cmd);
    }

    MeshHandle Renderer::UploadMesh(const MeshData& meshData)
    {
        VkDeviceSize posSize { sizeof(VertexPosition) * meshData.positions.size() };
        VkDeviceSize colorSize { sizeof(VertexColor) * meshData.colors.size() };
        VkDeviceSize normalSize { sizeof(VertexNormal) * meshData.normals.size() };
        VkDeviceSize uvSize { sizeof(VertexUV) * meshData.uvs.size() };
        VkDeviceSize faceSize { sizeof(Face) * meshData.faces.size() };
        VkDeviceSize totalSize { posSize + colorSize + normalSize + faceSize + uvSize };

        MeshHandle handle {};

        if (totalSize == 0) { return handle; }

        handle.MeshID = m_NextMeshID++;
        handle.faceCount = static_cast<uint32_t>(meshData.faces.size());

        MeshAllocationResult allocation { m_GpuAllocator->AllocateMesh(posSize, colorSize, normalSize, uvSize, faceSize, handle.MeshID) };
        handle.posOffset = allocation.posOffset;
        handle.colorOffset = allocation.colorOffset;
        handle.normalOffset = allocation.normalOffset;
        handle.uvOffset = allocation.uvOffset;
        handle.faceOffset = allocation.faceOffset;
        
        VkBuffer stagingBuffer { VK_NULL_HANDLE };
        VmaAllocation stagingAllocation { VK_NULL_HANDLE };

        VkBufferCreateInfo stagingInfo {};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size = totalSize;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo stagingAllocInfo {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo stagingAllocResult {};

        if (vmaCreateBuffer(m_Context->GetAllocator(), &stagingInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, &stagingAllocResult) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create mesh staging buffer! Size: {0} bytes.", totalSize);
            throw std::runtime_error("Failed to create mesh staging buffer");
        }

        char* dst { static_cast<char*>(stagingAllocResult.pMappedData) };
        size_t writeOffset { 0 };

        if (posSize > 0) { memcpy(dst + writeOffset, meshData.positions.data(), posSize); writeOffset += posSize; }
        if (colorSize > 0) { memcpy(dst + writeOffset, meshData.colors.data(), colorSize);  writeOffset += colorSize; }
        if (normalSize > 0) { memcpy(dst + writeOffset, meshData.normals.data(), normalSize); writeOffset += normalSize; }
        if (uvSize > 0) { memcpy(dst + writeOffset, meshData.uvs.data(), uvSize); writeOffset += uvSize; }
        if (faceSize > 0) { memcpy(dst + writeOffset, meshData.faces.data(), faceSize); }

        VkCommandBufferAllocateInfo allocationInfo {};
        allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocationInfo.commandPool = m_TransferCommandPool;
        allocationInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer { VK_NULL_HANDLE };
        vkAllocateCommandBuffers(m_Context->GetDevice(), &allocationInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion {};
        VkDeviceSize srcOffset { 0 };
        
        auto addCopy { [&](VkBuffer dstBuf, VkDeviceSize dstOffset, VkDeviceSize size)
            {
                if (size > 0)
                {
                    copyRegion.srcOffset = srcOffset;
                    copyRegion.dstOffset = dstOffset;
                    copyRegion.size = size;
                    vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuf, 1, &copyRegion);
                    srcOffset += size;
                }
            } };

        addCopy(allocation.pos.Buffer, allocation.pos.Offset, posSize);
        addCopy(allocation.color.Buffer, allocation.color.Offset, colorSize);
        addCopy(allocation.normal.Buffer, allocation.normal.Offset, normalSize);
        addCopy(allocation.uv.Buffer, allocation.uv.Offset, uvSize);
        addCopy(allocation.face.Buffer, allocation.face.Offset, faceSize);

        vkEndCommandBuffer(commandBuffer);

        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence transferFence { VK_NULL_HANDLE };
        vkCreateFence(m_Context->GetDevice(), &fenceInfo, nullptr, &transferFence);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_Context->GetTransferQueue(), 1, &submitInfo, transferFence);

        handle.MeshID = m_NextMeshID++;
        m_PendingTransfers.push_back({transferFence, commandBuffer, stagingBuffer, stagingAllocation, handle.MeshID, UINT32_MAX});
        m_PendingMeshIDs.insert(handle.MeshID);

        return handle;
    }

    void Renderer::FreeMesh(const MeshHandle& handle)
    {
        m_GpuAllocator->FreeMesh(handle.MeshID);
        AE_ENGINE_TRACE("Mesh freed from GPU Buffers.");
    }

    void Renderer::UpdateTextureDescriptors(const std::vector<Texture>& textures)
    {
        if (textures.empty()) { return; }

        m_GlobalImageInfos.clear();
        m_GlobalImageInfos.reserve(textures.size());
        
        for (const auto& tex : textures)
        {
            m_GlobalImageInfos.push_back({tex.Sampler, tex.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }

        std::fill(m_LastUpdatedTextureCount.begin(), m_LastUpdatedTextureCount.end(), 0);
    }

    VkCommandBuffer Renderer::BeginAsyncGraphicsCommand()
    {
        VkCommandBufferAllocateInfo allocationInfo {};
        allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocationInfo.commandPool = m_CommandPool;
        allocationInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_Context->GetDevice(), &allocationInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
        
        return cmd;
    }

    void Renderer::EndAndSubmitAsyncGraphicsCommand(VkCommandBuffer cmd, VkBuffer stagingBuffer, VmaAllocation stagingAllocation, uint32_t textureIndex)
    {
        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence fence;
        vkCreateFence(m_Context->GetDevice(), &fenceInfo, nullptr, &fence);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, fence);

        m_PendingTransfers.push_back({fence, cmd, stagingBuffer, stagingAllocation, 0, textureIndex});

        if (textureIndex != UINT32_MAX)
        {
            m_PendingTextureIndices.insert(textureIndex);
        }
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

#ifdef ANTELOPE_EDITOR_MODE
    void Renderer::ResizeRenderTexture(uint32_t width, uint32_t height)
    {
        vkDeviceWaitIdle(m_Context->GetDevice());
        m_RenderTexture->Resize(width, height);
        m_OutlineRenderer->RebuildResources(m_RenderTexture);
    }

    VkSemaphore Renderer::AcquireRenderFinishedSemaphore()
    {
        for (auto& slot : m_SemaphorePool)
        {
            if (!slot.pendingPresent) { return slot.semaphore; }

            if (vkGetFenceStatus(m_Context->GetDevice(), slot.presentFence) == VK_SUCCESS)
            {
                vkResetFences(m_Context->GetDevice(), 1, &slot.presentFence);
                slot.pendingPresent = false;
                return slot.semaphore;
            }
        }

        AE_ENGINE_WARN("Semaphore pool exhausted — growing by 1.");
        VkSemaphoreCreateInfo semInfo {};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenInfo {};
        fenInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        auto& newSlot { m_SemaphorePool.emplace_back() };
        vkCreateSemaphore(m_Context->GetDevice(), &semInfo, nullptr, &newSlot.semaphore);
        vkCreateFence(m_Context->GetDevice(), &fenInfo, nullptr, &newSlot.presentFence);
        return newSlot.semaphore;
    }

    void Renderer::MarkSemaphorePending(VkSemaphore semaphore, VkFence presentFence)
    {
        for (auto& slot : m_SemaphorePool)
        {
            if (slot.semaphore == semaphore)
            {
                slot.pendingPresent = true;
                return;
            }
        }
    }
#endif

    void Renderer::UpdateUniformBuffer(uint32_t currentImage, const UniformBufferObject& cameraData)
    {
        m_UniformBuffers[currentImage]->WriteToBuffer((void*)&cameraData, sizeof(cameraData));
    }

    void Renderer::CreateDescriptorSetLayout() 
    {
        DescriptorLayoutBuilder builder;
        
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        
        for (int i { 1 }; i <= 6; ++i)
        {
            builder.AddBinding(i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        builder.AddBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1024);

        std::array<VkDescriptorBindingFlags, 8> bindingFlags {};

        for (int i { 0 }; i < 7; ++i) 
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
        size_t i { 0 };

        while (i < m_PendingTransfers.size())
        {
            auto& transfer { m_PendingTransfers[i] };

            if (vkGetFenceStatus(m_Context->GetDevice(), transfer.fence) == VK_SUCCESS)
            {
                vkDestroyFence(m_Context->GetDevice(), transfer.fence, nullptr);
                VkCommandPool poolToUse { (transfer.meshID == 0) ? m_CommandPool : m_TransferCommandPool };
                vkFreeCommandBuffers(m_Context->GetDevice(), poolToUse, 1, &transfer.commandBuffer);
                vmaDestroyBuffer(m_Context->GetAllocator(), transfer.stagingBuffer, transfer.stagingAllocation);

                if (transfer.meshID != 0)
                {
                    m_PendingMeshIDs.erase(transfer.meshID);
                }

                if (transfer.textureIndex != UINT32_MAX)
                {
                    m_PendingTextureIndices.erase(transfer.textureIndex);
                }

                m_PendingTransfers[i] = std::move(m_PendingTransfers.back());
                m_PendingTransfers.pop_back();
            }
            else
            {
                ++i;
            }
        }
    }

    VkCommandBuffer Renderer::BeginFrame()
    {
        ProcessPendingTransfers();
        vkWaitForFences(m_Context->GetDevice(), 1, &m_FrameSync[m_CurrentFrame].inFlightFence, VK_TRUE, UINT64_MAX);

        VkResult result { vkAcquireNextImageKHR(
            m_Context->GetDevice(),
            m_SwapChain->GetSwapchain(),
            UINT64_MAX,
            m_FrameSync[m_CurrentFrame].imageAvailableSemaphore,
            VK_NULL_HANDLE,
            &m_CurrentImageIndex) };

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            vkDestroySemaphore(m_Context->GetDevice(), m_FrameSync[m_CurrentFrame].imageAvailableSemaphore, nullptr);
            
            VkSemaphoreCreateInfo semInfo {};
            semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            vkCreateSemaphore(m_Context->GetDevice(), &semInfo, nullptr, &m_FrameSync[m_CurrentFrame].imageAvailableSemaphore);
            m_SwapChain->RecreateSwapchain();
            return VK_NULL_HANDLE;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            AE_ENGINE_ERROR("Failed to acquire swapchain image!");
            return VK_NULL_HANDLE;
        }

        vkResetFences(m_Context->GetDevice(), 1, &m_FrameSync[m_CurrentFrame].inFlightFence);
        vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo) != VK_SUCCESS)
        {
            AE_ENGINE_ERROR("Failed to begin recording command buffer!");
            return VK_NULL_HANDLE;
        }

        return m_CommandBuffers[m_CurrentFrame];
    }

    void Renderer::DrawObjects(VkCommandBuffer cmd, const UniformBufferObject& cameraData, const std::vector<RenderCommand>& renderList)
    {
        uint32_t currentGlobalTextureCount { static_cast<uint32_t>(m_GlobalImageInfos.size()) };

        if (currentGlobalTextureCount > 0 && m_LastUpdatedTextureCount[m_CurrentFrame] < currentGlobalTextureCount)
        {
            VkWriteDescriptorSet descriptorWrite {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_DescriptorSets[m_CurrentFrame];
            descriptorWrite.dstBinding = 7;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = currentGlobalTextureCount;
            descriptorWrite.pImageInfo = m_GlobalImageInfos.data();

            vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &descriptorWrite, 0, nullptr);
            
            m_LastUpdatedTextureCount[m_CurrentFrame] = currentGlobalTextureCount;
            AE_ENGINE_TRACE("Descriptor Set for Frame {0} updated with {1} textures.", m_CurrentFrame, currentGlobalTextureCount);
        }

        UpdateUniformBuffer(m_CurrentFrame, cameraData);

        VkRenderPassBeginInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    #ifdef ANTELOPE_EDITOR_MODE
        renderPassInfo.renderPass  = m_RenderTexture->GetRenderPass();
        renderPassInfo.framebuffer = m_RenderTexture->GetFramebuffer();
        renderPassInfo.renderArea.extent = m_RenderTexture->GetExtent();
    #else
        renderPassInfo.renderPass = m_SwapChain->GetRenderPass();
        renderPassInfo.framebuffer = m_SwapChain->GetFramebuffers()[m_CurrentImageIndex];
        renderPassInfo.renderArea.extent = m_SwapChain->GetExtent();
    #endif
        renderPassInfo.renderArea.offset = {0, 0};
        
        std::array<VkClearValue, 2> clearValues {};
        clearValues[0] = {};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport {};
        viewport.x = 0.0f; viewport.y = 0.0f;
        #ifdef ANTELOPE_EDITOR_MODE
        viewport.width  = static_cast<float>(m_RenderTexture->GetExtent().width);
        viewport.height = static_cast<float>(m_RenderTexture->GetExtent().height);
        #else
        viewport.width = static_cast<float>(m_SwapChain->GetExtent().width);
        viewport.height = static_cast<float>(m_SwapChain->GetExtent().height);
        #endif
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset = {0, 0};
        #ifdef ANTELOPE_EDITOR_MODE
        scissor.extent = m_RenderTexture->GetExtent();
        #else
        scissor.extent = m_SwapChain->GetExtent();
        #endif
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        m_SkyRenderer->Draw(cmd, m_DescriptorSets[m_CurrentFrame]);
        m_MainPipeline->Bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_PipelineLayout, 0, 1, &m_DescriptorSets[m_CurrentFrame], 0, nullptr);

        ObjectData* objectDataMap { static_cast<ObjectData*>(m_ObjectBuffers[m_CurrentFrame]->GetMappedMemory()) };
        VkDrawIndirectCommand* indirectCommandsMap { static_cast<VkDrawIndirectCommand*>(m_IndirectBuffers[m_CurrentFrame]->GetMappedMemory()) };

        uint32_t objectCount { 0 };
        std::vector<uint32_t> selectedIndirectIndices;

        for (const auto& command : renderList)
        {
            if (m_PendingMeshIDs.count(command.mesh.MeshID) > 0) { continue; }
            if (m_PendingTextureIndices.count(command.materialIndex) > 0) { continue; }

            if (objectCount >= MAX_OBJECTS)
            {
                AE_ENGINE_WARN("Maximum object limit ({0}) reached! Remaining objects will not be drawn.", MAX_OBJECTS);
                break;
            }

            objectDataMap[objectCount].model = command.transform;
            objectDataMap[objectCount].normalMatrix = glm::mat4(command.normalMatrix);
            objectDataMap[objectCount].posOffset = command.mesh.posOffset;
            objectDataMap[objectCount].colorOffset = command.mesh.colorOffset;
            objectDataMap[objectCount].normalOffset = command.mesh.normalOffset;
            objectDataMap[objectCount].faceOffset = command.mesh.faceOffset;
            objectDataMap[objectCount].uvOffset = command.mesh.uvOffset;
            objectDataMap[objectCount].materialIndex = command.materialIndex;
        #ifdef ANTELOPE_EDITOR_MODE
            objectDataMap[objectCount].entityID = command.entityID;
        #endif

            indirectCommandsMap[objectCount].vertexCount = command.mesh.faceCount * 3;
            indirectCommandsMap[objectCount].instanceCount = 1;
            indirectCommandsMap[objectCount].firstVertex = 0;
            indirectCommandsMap[objectCount].firstInstance = objectCount;
        
        #ifdef ANTELOPE_EDITOR_MODE
            if (m_SelectedEntityIDs.count(command.entityID))
            {
                selectedIndirectIndices.push_back(objectCount);
            }
        #endif

            objectCount++;
        }

        if (objectCount > 0)
        {
            vkCmdDrawIndirect(cmd, m_IndirectBuffers[m_CurrentFrame]->GetBuffer(), 0, objectCount, sizeof(VkDrawIndirectCommand));
        }

    #ifdef ANTELOPE_EDITOR_MODE
        m_GridRenderer->Draw(cmd, m_DescriptorSets[m_CurrentFrame]);
    #endif

        vkCmdEndRenderPass(cmd);

    #ifdef ANTELOPE_EDITOR_MODE
        if (!selectedIndirectIndices.empty())
        {
            m_OutlineRenderer->DrawMask(cmd, selectedIndirectIndices, m_IndirectBuffers[m_CurrentFrame]->GetBuffer(), m_DescriptorSets[m_CurrentFrame], m_OutlineColor);
            m_OutlineRenderer->DrawComposite(cmd);
        }
    #endif
    }

    void Renderer::EndFrame(VkCommandBuffer cmd)
    {
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) { return; }

    #ifdef ANTELOPE_EDITOR_MODE
        VkSemaphore renderFinished { VK_NULL_HANDLE };

        if (m_Maintenance1Supported)
        {
            renderFinished = AcquireRenderFinishedSemaphore();
        }
        else
        {
            renderFinished = m_RenderFinishedRing[m_CurrentImageIndex];
        }

    #else
        VkSemaphore renderFinished { m_FrameSync[m_CurrentFrame].renderFinishedSemaphore };
    #endif

        VkSemaphore waitSemaphores[] { m_FrameSync[m_CurrentFrame].imageAvailableSemaphore };
        VkPipelineStageFlags waitStages[] { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinished;

        if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, m_FrameSync[m_CurrentFrame].inFlightFence) != VK_SUCCESS) { return; }

        VkPresentInfoKHR presentInfo {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinished;

        VkSwapchainKHR swapchains[] { m_SwapChain->GetSwapchain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &m_CurrentImageIndex;

    #ifdef ANTELOPE_EDITOR_MODE
        VkFence presentFence { VK_NULL_HANDLE };
        VkSwapchainPresentFenceInfoEXT presentFenceInfo {};

        if (m_Maintenance1Supported)
        {
            for (auto& slot : m_SemaphorePool)
            {
                if (slot.semaphore == renderFinished)
                {
                    presentFence = slot.presentFence;
                    break;
                }
            }

            presentFenceInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
            presentFenceInfo.swapchainCount = 1;
            presentFenceInfo.pFences = &presentFence;
            presentInfo.pNext = &presentFenceInfo;
        }
    #endif

        VkResult result { vkQueuePresentKHR(m_Context->GetPresentQueue(), &presentInfo) };

    #ifdef ANTELOPE_EDITOR_MODE
        if (m_Maintenance1Supported)
        {
            MarkSemaphorePending(renderFinished, presentFence);
        }
    #endif

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_SwapChain->IsFramebufferResized())
        {
            m_SwapChain->SetFramebufferResized(false);
            vkDeviceWaitIdle(m_Context->GetDevice());
            m_SwapChain->RecreateSwapchain();
    #ifdef ANTELOPE_EDITOR_MODE
            uint32_t newImageCount { static_cast<uint32_t>(m_SwapChain->GetFramebuffers().size()) };

            if (m_Maintenance1Supported)
            {
                for (auto& slot : m_SemaphorePool)
                {
                    vkResetFences(m_Context->GetDevice(), 1, &slot.presentFence);
                    slot.pendingPresent = false;
                }
            }
    #endif
        }

        m_CurrentFrame = (m_CurrentFrame + 1) % m_MaxFramesInFlight;
    }

    void Renderer::DestroySyncObjects()
    {
        for (auto& frame : m_FrameSync)
        {
            if (frame.imageAvailableSemaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_Context->GetDevice(), frame.imageAvailableSemaphore, nullptr);
            }
                
            #ifndef ANTELOPE_EDITOR_MODE
            if (frame.renderFinishedSemaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_Context->GetDevice(), frame.renderFinishedSemaphore, nullptr);
            }
            #endif

            if (frame.inFlightFence != VK_NULL_HANDLE)
            {
                vkDestroyFence(m_Context->GetDevice(), frame.inFlightFence, nullptr);
            }
        }

        m_FrameSync.clear();

    #ifdef ANTELOPE_EDITOR_MODE
        if (m_Maintenance1Supported)
        {
            for (auto& slot : m_SemaphorePool)
            {
                if (slot.semaphore != VK_NULL_HANDLE)
                {
                    vkDestroySemaphore(m_Context->GetDevice(), slot.semaphore, nullptr);
                }
                    
                if (slot.presentFence != VK_NULL_HANDLE)
                {
                    vkDestroyFence(m_Context->GetDevice(), slot.presentFence, nullptr);
                }
            }

            m_SemaphorePool.clear();
        }
        else
        {
            for (auto& sem : m_RenderFinishedRing)
            {
                if (sem != VK_NULL_HANDLE)
                {
                    vkDestroySemaphore(m_Context->GetDevice(), sem, nullptr);
                } 
            }
            m_RenderFinishedRing.clear();
        }
    #endif

        AE_ENGINE_TRACE("Sync objects destroyed.");
    }

    void Renderer::CreateGraphicsPipeline()
    {
        PipelineConfigInfo pipelineConfig {};
        Pipeline::DefaultPipelineConfigInfo(pipelineConfig, m_Context);
        pipelineConfig.pipelineLayout = m_PipelineLayout;
    #ifdef ANTELOPE_EDITOR_MODE
        pipelineConfig.renderPass = m_RenderTexture->GetRenderPass();
    #else
        pipelineConfig.renderPass = m_SwapChain->GetRenderPass();
    #endif
        
        m_MainPipeline = std::make_unique<Pipeline>(
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
        m_CommandBuffers.resize(m_MaxFramesInFlight);

        VkCommandBufferAllocateInfo allocationInfo {};
        allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocationInfo.commandPool = m_CommandPool;
        allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocationInfo.commandBufferCount = static_cast<uint32_t>(m_MaxFramesInFlight);

        if (vkAllocateCommandBuffers(m_Context->GetDevice(), &allocationInfo, m_CommandBuffers.data()) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to allocate Command Buffers!");
            throw std::runtime_error("Failed to allocate Command Buffers");
        }
    }

    void Renderer::CreateSyncObjects()
    {
        m_FrameSync.resize(m_MaxFramesInFlight);
    #ifdef ANTELOPE_EDITOR_MODE
        uint32_t imageCount { static_cast<uint32_t>(m_SwapChain->GetFramebuffers().size()) };
    #endif
        VkSemaphoreCreateInfo semaphoreInfo {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkFenceCreateInfo unsignaledFenceInfo {};
        unsignaledFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        for (size_t i { 0 }; i < m_MaxFramesInFlight; i++)
        {
            if (vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &m_FrameSync[i].imageAvailableSemaphore) != VK_SUCCESS ||
                #ifndef ANTELOPE_EDITOR_MODE
                    vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &m_FrameSync[i].renderFinishedSemaphore) != VK_SUCCESS ||
                #endif
                vkCreateFence(m_Context->GetDevice(), &fenceInfo, nullptr, &m_FrameSync[i].inFlightFence) != VK_SUCCESS)
            {
                AE_ENGINE_CRITICAL("Failed to create per-frame sync objects!");
                throw std::runtime_error("Failed to create per-frame sync objects");
            }
        }

    #ifdef ANTELOPE_EDITOR_MODE
        if (m_Maintenance1Supported)
        {
            uint32_t poolSize { imageCount + m_MaxFramesInFlight };
            m_SemaphorePool.resize(poolSize);

            for (auto& slot : m_SemaphorePool)
            {
                if (vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &slot.semaphore) != VK_SUCCESS ||
                    vkCreateFence(m_Context->GetDevice(), &unsignaledFenceInfo, nullptr, &slot.presentFence) != VK_SUCCESS)
                {
                    AE_ENGINE_CRITICAL("Failed to create maintenance1 semaphore pool!");
                    throw std::runtime_error("Failed to create maintenance1 semaphore pool");
                }
            }

            AE_ENGINE_TRACE("Sync objects created. {0} frame slots, {1} semaphore pool slots (maintenance1).", m_MaxFramesInFlight, poolSize);
        }
        else
        {
            uint32_t ringSize { imageCount };
            m_RenderFinishedRing.resize(ringSize);

            for (auto& sem : m_RenderFinishedRing)
            {
                if (vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &sem) != VK_SUCCESS)
                {
                    AE_ENGINE_CRITICAL("Failed to create semaphore ring!");
                    throw std::runtime_error("Failed to create semaphore ring");
                }
            }

            AE_ENGINE_TRACE("Sync objects created. {0} frame slots, ring size: {1} (fallback).", m_MaxFramesInFlight, ringSize);
        }
    #else
        AE_ENGINE_TRACE("Sync objects created. {0} frame slots.", m_MaxFramesInFlight);
    #endif
    }

    void Renderer::CreateUniformBuffers()
    {
        VkDeviceSize bufferSize { sizeof(UniformBufferObject) };
        m_UniformBuffers.reserve(m_MaxFramesInFlight);

        for (size_t i { 0 }; i < m_MaxFramesInFlight; i++) 
        {
            m_UniformBuffers.push_back(std::make_unique<Buffer>(
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
        m_ObjectBuffers.reserve(m_MaxFramesInFlight);

        for (size_t i { 0 }; i < m_MaxFramesInFlight; i++) 
        {
            m_ObjectBuffers.push_back(std::make_unique<Buffer>(
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
        VkDeviceSize bufferSize { sizeof(VkDrawIndirectCommand) * MAX_OBJECTS };
        m_IndirectBuffers.reserve(m_MaxFramesInFlight);

        for (size_t i { 0 }; i < m_MaxFramesInFlight; ++i) 
        {
            m_IndirectBuffers.push_back(std::make_unique<Buffer>(
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
        poolSizes[0].descriptorCount = static_cast<uint32_t>(m_MaxFramesInFlight);
        
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(m_MaxFramesInFlight * 6);

        poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[2].descriptorCount = 1024 * m_MaxFramesInFlight;

        VkDescriptorPoolCreateInfo descriptorPoolInfo {};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT; 
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolInfo.pPoolSizes = poolSizes.data();
        descriptorPoolInfo.maxSets = static_cast<uint32_t>(m_MaxFramesInFlight);

        if (vkCreateDescriptorPool(m_Context->GetDevice(), &descriptorPoolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create Descriptor Pool!");
            throw std::runtime_error("Failed to create Descriptor Pool");
        }
    }

    void Renderer::CreateDescriptorSets()
    {
        m_DescriptorSets.resize(m_MaxFramesInFlight);

        for (size_t i { 0 }; i < m_MaxFramesInFlight; ++i) 
        {
            m_DescriptorSets[i] = m_GlobalDescriptorAllocator->Allocate(m_DescriptorSetLayout);

            DescriptorWriter writer;
            writer.WriteBuffer(0, m_UniformBuffers[i]->GetBuffer(), sizeof(UniformBufferObject), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            writer.WriteBuffer(1, m_GpuAllocator->GetPosBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(2, m_GpuAllocator->GetColorBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(3, m_GpuAllocator->GetNormalBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(4, m_GpuAllocator->GetFaceBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(5, m_GpuAllocator->GetUvBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(6, m_ObjectBuffers[i]->GetBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.UpdateSet(m_Context, m_DescriptorSets[i]);
        }
    }

    void Renderer::CreateTransferCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices { m_Context->FindQueueFamilies(m_Context->GetPhysicalDevice()) };

        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.TransferFamily.value(); 

        if (vkCreateCommandPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_TransferCommandPool) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Transfer Command Pool!");
            throw std::runtime_error("Failed to create Transfer Command Pool");
        }
    }
}
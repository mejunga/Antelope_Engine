#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Graphics/SkyboxPass.hpp>
#include <Engine/Renderer/Graphics/ShadowPass.hpp>
#include <Engine/Renderer/Graphics/BloomPass.hpp>
#include <Engine/Renderer/Graphics/PostCompositePass.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp>
#include <Engine/Asset/TextureManager.hpp>
#include <Engine/Renderer/Vulkan/Buffer.hpp>
#include <Engine/Renderer/Vulkan/Pipeline.hpp>
#include <Engine/Renderer/Vulkan/Descriptor.hpp>
#include <Engine/Renderer/Vulkan/GpuMemoryAllocator.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Core/JobSystem.hpp>
#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Renderer/UI/UIContext.hpp>
#include <Engine/Renderer/Graphics/EditorGridPass.hpp>
#include <Engine/Renderer/Graphics/OutlinePass.hpp>
#endif

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <memory_resource>


namespace Antelope
{
    namespace {
        std::vector<JobHandle> s_DrawHandles;
    }

    Renderer::Renderer(std::shared_ptr<VulkanContext> context, std::shared_ptr<SwapChain> swapChain, uint32_t framesInFlight)
        : m_Context(context), m_SwapChain(swapChain), m_MaxFramesInFlight(framesInFlight)
    {
        m_SceneHDRTexture = std::make_shared<RenderTexture>(
            m_Context,
            m_SwapChain->GetExtent().width,
            m_SwapChain->GetExtent().height,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            true
        );

        VkRenderPass sceneRenderPass { m_SceneHDRTexture->GetRenderPass() };

    #ifdef ANTELOPE_EDITOR_MODE
        m_Maintenance1Supported = m_Context->IsSwapchainMaintenance1Supported();
        m_FinalLDRTexture = std::make_shared<RenderTexture>(
            m_Context,
            m_SwapChain->GetExtent().width,
            m_SwapChain->GetExtent().height,
            VK_FORMAT_B8G8R8A8_UNORM,
            false
        );
    #endif

        m_StagingArenas.resize(m_MaxFramesInFlight);

        for (auto& arena : m_StagingArenas)
        {
            VkBufferCreateInfo stagingInfo {};
            stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingInfo.size = arena.capacity;
            stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo stagingAllocInfo {};
            stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            
            VmaAllocationInfo result {};
            vmaCreateBuffer(m_Context->GetAllocator(), &stagingInfo, &stagingAllocInfo, &arena.buffer, &arena.allocation, &result);
            arena.mappedData = static_cast<char*>(result.pMappedData);
        }

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

        m_SkyBoxPass = std::make_unique<SkyboxPass>(m_Context, m_PipelineLayout, sceneRenderPass);
        AE_ENGINE_TRACE("Sky Renderer created.");
        
        m_ShadowPasses[0] = std::make_shared<ShadowPass>(m_Context, m_PipelineLayout, 4096, 4096);
        m_ShadowPasses[1] = std::make_shared<ShadowPass>(m_Context, m_PipelineLayout, 4096, 4096);
        AE_ENGINE_TRACE("Shadow Maps created.");

    #ifdef ANTELOPE_EDITOR_MODE
        m_EditorGridPass = std::make_unique<EditorGridPass>(m_Context, m_PipelineLayout, sceneRenderPass);
        AE_ENGINE_TRACE("Grid Renderer created.");

        m_OutlinePass = std::make_unique<OutlinePass>(m_Context, m_PipelineLayout, m_FinalLDRTexture);
        AE_ENGINE_TRACE("Outline Renderer created.");
        VkRenderPass postRenderPass = m_FinalLDRTexture->GetRenderPass();
    #else
        VkRenderPass postRenderPass = m_SwapChain->GetRenderPass();
    #endif

        m_BloomPass = std::make_unique<BloomPass>(m_Context, m_SceneHDRTexture, m_SceneHDRTexture->GetWidth(), m_SceneHDRTexture->GetHeight());
        AE_ENGINE_TRACE("BloomPass created.");

        m_PostCompositePass = std::make_unique<PostCompositePass>(m_Context, m_SceneHDRTexture, m_BloomPass->GetBloomTexture(), m_BloomPass->GetFlareTexture(), postRenderPass);
        AE_ENGINE_TRACE("PostCompositePass created.");

        CreateCommandPool();
        AE_ENGINE_TRACE("Command Pool created.");
        CreateTransferCommandPool();
        AE_ENGINE_TRACE("Transfer Command Pool created.");
        CreateCommandBuffers();
        AE_ENGINE_TRACE("Command Buffers allocated for {0} frames.", m_MaxFramesInFlight);
        CreateSyncObjects();
        CreateUniformBuffers();
        AE_ENGINE_TRACE("Uniform buffers created.");
        CreateObjectBuffers();
        AE_ENGINE_TRACE("Object buffers created.");
        CreateBoneBuffers();
        AE_ENGINE_TRACE("Bone buffers created.");
        CreateMaterialBuffers();
        AE_ENGINE_TRACE("Material buffers created.");
        CreateIndirectBuffers();
        AE_ENGINE_TRACE("Indirect command buffers created.");
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

        for (auto& arena : m_StagingArenas)
        {
            if (arena.buffer != VK_NULL_HANDLE)
            {
                vmaDestroyBuffer(m_Context->GetAllocator(), arena.buffer, arena.allocation);
            }
        }

        for (auto& transfer : m_PendingTransfers)
        {
            vkDestroyFence(m_Context->GetDevice(), transfer.fence, nullptr);
            VkCommandPool poolToUse { (transfer.meshID == 0) ? m_CommandPool : m_TransferCommandPool };
            vkFreeCommandBuffers(m_Context->GetDevice(), poolToUse, 1, &transfer.commandBuffer);

            if (transfer.stagingAllocation != VK_NULL_HANDLE)
            {
                vmaDestroyBuffer(m_Context->GetAllocator(), transfer.stagingBuffer, transfer.stagingAllocation);
            }
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

    void Renderer::DrawFrame(const UniformBufferObject& cameraData, std::span<const RenderCommand> renderList)
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
        VkDeviceSize tangentSize { sizeof(VertexTangent) * meshData.tangents.size() };
        VkDeviceSize faceSize { sizeof(Face) * meshData.faces.size() };
        VkDeviceSize jointSize { sizeof(VertexJointData) * meshData.joints.size() };
        VkDeviceSize totalSize { posSize + colorSize + normalSize + uvSize + tangentSize + faceSize + jointSize };

        MeshHandle handle {};

        if (totalSize == 0) { return handle; }

        handle.MeshID = m_NextMeshID++;
        handle.faceCount = static_cast<uint32_t>(meshData.faces.size());

        MeshAllocationResult allocation { m_GpuAllocator->AllocateMesh(posSize, colorSize, normalSize, uvSize, tangentSize, faceSize, jointSize, handle.MeshID) };
        handle.posOffset = allocation.posOffset;
        handle.colorOffset = allocation.colorOffset;
        handle.normalOffset = allocation.normalOffset;
        handle.uvOffset = allocation.uvOffset;
        handle.tangentOffset = allocation.tangentOffset;
        handle.faceOffset = allocation.faceOffset;
        handle.jointOffset = allocation.jointOffset;
        
        VkBuffer stagingBuffer { VK_NULL_HANDLE };
        VmaAllocation stagingAllocation { VK_NULL_HANDLE };
        VkDeviceSize stagingOffset { 0 };
        void* mappedData { nullptr };
        CreateStagingBuffer(nullptr, totalSize, stagingBuffer, stagingAllocation, stagingOffset, &mappedData);
        
        char* dst { static_cast<char*>(mappedData) };
        size_t writeOffset { 0 };

        if (posSize > 0) { memcpy(dst + writeOffset, meshData.positions.data(), posSize); writeOffset += posSize; }
        if (colorSize > 0) { memcpy(dst + writeOffset, meshData.colors.data(), colorSize);  writeOffset += colorSize; }
        if (normalSize > 0) { memcpy(dst + writeOffset, meshData.normals.data(), normalSize); writeOffset += normalSize; }
        if (uvSize > 0) { memcpy(dst + writeOffset, meshData.uvs.data(), uvSize); writeOffset += uvSize; }
        if (tangentSize > 0) { memcpy(dst + writeOffset, meshData.tangents.data(), tangentSize); writeOffset += tangentSize; }
        if (faceSize > 0) { memcpy(dst + writeOffset, meshData.faces.data(), faceSize); writeOffset += faceSize; }
        if (jointSize > 0) { memcpy(dst + writeOffset, meshData.joints.data(), jointSize); writeOffset += jointSize; }

        VkCommandBufferAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandPool = m_TransferCommandPool;
        allocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer { VK_NULL_HANDLE };
        vkAllocateCommandBuffers(m_Context->GetDevice(), &allocateInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion {};
        VkDeviceSize srcOffset { stagingOffset };

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
        }};

        addCopy(allocation.pos.Buffer, allocation.pos.Offset, posSize);
        addCopy(allocation.color.Buffer, allocation.color.Offset, colorSize);
        addCopy(allocation.normal.Buffer, allocation.normal.Offset, normalSize);
        addCopy(allocation.uv.Buffer, allocation.uv.Offset, uvSize);
        addCopy(allocation.tangent.Buffer, allocation.tangent.Offset, tangentSize);
        addCopy(allocation.face.Buffer, allocation.face.Offset, faceSize);
        addCopy(allocation.joint.Buffer, allocation.joint.Offset, jointSize);

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
        VkCommandBufferAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandPool = m_CommandPool;
        allocateInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_Context->GetDevice(), &allocateInfo, &cmd);

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

    void Renderer::CreateStagingBuffer(const void* data, VkDeviceSize bufferSize, VkBuffer& outBuffer, VmaAllocation& outAllocation, VkDeviceSize& outOffset, void** outMapped)
    {
        auto& arena { m_StagingArenas[m_CurrentFrame] };
        size_t alignment { 256 };
        size_t allocSize { (bufferSize + alignment - 1) & ~(alignment - 1) };
        size_t currentOffset { arena.offset.fetch_add(allocSize, std::memory_order_relaxed) };
        
        if (currentOffset + allocSize <= arena.capacity)
        {
            outBuffer = arena.buffer;
            outAllocation = VK_NULL_HANDLE;
            outOffset = currentOffset;
            
            if (data) { memcpy(arena.mappedData + currentOffset, data, bufferSize); }
            if (outMapped) { *outMapped = arena.mappedData + currentOffset; }

            return;
        }

        outOffset = 0;
        
        VkBufferCreateInfo stagingInfo {};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size = bufferSize;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo stagingAllocInfo {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        
        VmaAllocationInfo stagingAllocResult {};
        if (vmaCreateBuffer(m_Context->GetAllocator(), &stagingInfo, &stagingAllocInfo, &outBuffer, &outAllocation, &stagingAllocResult) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("UploadMesh: staging buffer allocation failed (size = {} bytes)", bufferSize);
            if (outMapped) { *outMapped = nullptr; }
            return;
        }
        
        if (outMapped) { *outMapped = stagingAllocResult.pMappedData; }
        if (data) { memcpy(stagingAllocResult.pMappedData, data, bufferSize); }
    }

    uint32_t Renderer::AddMaterial(const PBRMaterialData& material)
    {
        uint32_t index { static_cast<uint32_t>(m_GlobalMaterials.size()) };

        if (index >= MAX_MATERIALS)
        {
            AE_ENGINE_WARN("Max material limit reached! Overwriting last material.");
            return MAX_MATERIALS - 1;
        }

        m_GlobalMaterials.push_back(material);
        return index;
    }

    void Renderer::ClearMaterials()
    {
        m_GlobalMaterials.clear();
    }

#ifdef ANTELOPE_EDITOR_MODE
    void Renderer::ResizeRenderTexture(uint32_t width, uint32_t height)
    {
        vkDeviceWaitIdle(m_Context->GetDevice());
        m_SceneHDRTexture->Resize(width, height);
        m_FinalLDRTexture->Resize(width, height);
        m_OutlinePass->RebuildResources(m_FinalLDRTexture);
        m_BloomPass->Resize(width, height, m_SceneHDRTexture);
        m_PostCompositePass->UpdateDescriptorSet(m_SceneHDRTexture, m_BloomPass->GetBloomTexture(), m_BloomPass->GetFlareTexture());
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

        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

        for (int i { 1 }; i <= 6; ++i)
        {
            builder.AddBinding(i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        builder.AddBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1024);
        builder.AddBinding(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        builder.AddBinding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        builder.AddBinding(13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT); 

        std::array<VkDescriptorBindingFlags, 14> bindingFlags {};

        for (int i { 0 }; i < 14; ++i) 
        {
            bindingFlags[i] = 0;
        }

        bindingFlags[7] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
        bindingFlags[12] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        bindingFlags[13] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo {};
        flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flagsInfo.bindingCount = 14;
        flagsInfo.pBindingFlags = bindingFlags.data();

        m_DescriptorSetLayout = builder.Build(m_Context, &flagsInfo, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }

    void Renderer::CreatePipelineLayout()
    {
        VkPushConstantRange pushRange {};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(uint32_t);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;

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

                if (transfer.stagingAllocation != VK_NULL_HANDLE)
                {
                    vmaDestroyBuffer(m_Context->GetAllocator(), transfer.stagingBuffer, transfer.stagingAllocation);
                }
                
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
        m_StagingArenas[m_CurrentFrame].offset.store(0, std::memory_order_relaxed);

        VkResult result { vkAcquireNextImageKHR(
            m_Context->GetDevice(),
            m_SwapChain->GetSwapchain(),
            UINT64_MAX,
            m_FrameSync[m_CurrentFrame].imageAvailableSemaphore,
            VK_NULL_HANDLE,
            &m_CurrentImageIndex)};

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
            vkDestroySemaphore(m_Context->GetDevice(), m_FrameSync[m_CurrentFrame].imageAvailableSemaphore, nullptr);

            VkSemaphoreCreateInfo semInfo {};
            semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            vkCreateSemaphore(m_Context->GetDevice(), &semInfo, nullptr, &m_FrameSync[m_CurrentFrame].imageAvailableSemaphore);

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

    void Renderer::DrawObjects(VkCommandBuffer cmd, const UniformBufferObject& cameraData, std::span<const RenderCommand> renderList)
    {
        if (!m_GlobalMaterials.empty())
        {
            void* mappedData { m_MaterialBuffers[m_CurrentFrame]->GetMappedMemory() };
            memcpy(mappedData, m_GlobalMaterials.data(), m_GlobalMaterials.size() * sizeof(PBRMaterialData));
        }

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

        ObjectData* objectDataMap { static_cast<ObjectData*>(m_ObjectBuffers[m_CurrentFrame]->GetMappedMemory()) };
        VkDrawIndirectCommand* indirectCommandsMap { static_cast<VkDrawIndirectCommand*>(m_IndirectBuffers[m_CurrentFrame]->GetMappedMemory()) };
        glm::mat4* boneDataMap { static_cast<glm::mat4*>(m_BoneBuffers[m_CurrentFrame]->GetMappedMemory()) };
        std::atomic<uint32_t> currentGlobalBoneOffset { 0 };

        uint32_t objectCount { 0 };
        std::vector<uint32_t> selectedIndirectIndices;

        const bool anyPending { !m_PendingMeshIDs.empty() || !m_PendingTextureIndices.empty() };
        const uint32_t listSize { static_cast<uint32_t>(renderList.size()) };

        if (!anyPending && listSize <= MAX_OBJECTS)
        {
            objectCount = listSize;
            constexpr uint32_t k_BatchSize { 64 };
            auto& jobSystem { Application::Get().GetJobSystem() };

            if (objectCount <= k_BatchSize)
            {
                for (uint32_t i { 0 }; i < objectCount; ++i)
                {
                    const auto& command { renderList[i] };
                    objectDataMap[i].model = command.transform;
                    objectDataMap[i].normalMatrix = glm::mat4(command.normalMatrix);
                    objectDataMap[i].posOffset = command.mesh.posOffset;
                    objectDataMap[i].colorOffset = command.mesh.colorOffset;
                    objectDataMap[i].normalOffset = command.mesh.normalOffset;
                    objectDataMap[i].faceOffset = command.mesh.faceOffset;
                    objectDataMap[i].jointOffset = command.mesh.jointOffset;
                    objectDataMap[i].uvOffset = command.mesh.uvOffset;
                    objectDataMap[i].tangentOffset = command.mesh.tangentOffset;
                    objectDataMap[i].materialIndex = command.materialIndex;
                #ifdef ANTELOPE_EDITOR_MODE
                    objectDataMap[i].entityID = command.entityID;
                #endif
                    indirectCommandsMap[i].vertexCount = command.mesh.faceCount * 3;
                    indirectCommandsMap[i].instanceCount = 1;
                    indirectCommandsMap[i].firstVertex = 0;
                    indirectCommandsMap[i].firstInstance = i;

                    if (command.isAnimated && command.BoneMatrices)
                    {
                        uint32_t offset { currentGlobalBoneOffset.fetch_add(command.BoneCount) };
                        memcpy(boneDataMap + offset, command.BoneMatrices, command.BoneCount * sizeof(glm::mat4));

                        objectDataMap[i].boneOffset = offset;
                        objectDataMap[i].isAnimated = 1;
                    }
                    else
                    {
                        objectDataMap[i].boneOffset = 0;
                        objectDataMap[i].isAnimated = 0;
                    }
                }
            }
            else
            {
                const uint32_t numBatches { (objectCount + k_BatchSize - 1) / k_BatchSize };
                s_DrawHandles.clear();
                s_DrawHandles.reserve(numBatches);

                for (uint32_t b { 0 }; b < numBatches; ++b)
                {
                    uint32_t begin { b * k_BatchSize };
                    uint32_t end { objectCount < begin + k_BatchSize ? objectCount : begin + k_BatchSize };

                    s_DrawHandles.push_back(jobSystem.Submit("ObjFill", [begin, end, &renderList, objectDataMap, indirectCommandsMap, &currentGlobalBoneOffset, boneDataMap]()
                    {
                        for (uint32_t i { begin }; i < end; ++i)
                        {
                            const auto& command { renderList[i] };
                            objectDataMap[i].model = command.transform;
                            objectDataMap[i].normalMatrix = glm::mat4(command.normalMatrix);
                            objectDataMap[i].posOffset = command.mesh.posOffset;
                            objectDataMap[i].colorOffset = command.mesh.colorOffset;
                            objectDataMap[i].normalOffset = command.mesh.normalOffset;
                            objectDataMap[i].faceOffset = command.mesh.faceOffset;
                            objectDataMap[i].jointOffset = command.mesh.jointOffset;
                            objectDataMap[i].uvOffset = command.mesh.uvOffset;
                            objectDataMap[i].tangentOffset = command.mesh.tangentOffset;
                            objectDataMap[i].materialIndex = command.materialIndex;
                        #ifdef ANTELOPE_EDITOR_MODE
                            objectDataMap[i].entityID = command.entityID;
                        #endif
                            indirectCommandsMap[i].vertexCount = command.mesh.faceCount * 3;
                            indirectCommandsMap[i].instanceCount = 1;
                            indirectCommandsMap[i].firstVertex = 0;
                            indirectCommandsMap[i].firstInstance = i;

                            if (command.isAnimated && command.BoneMatrices)
                            {
                                uint32_t offset { currentGlobalBoneOffset.fetch_add(command.BoneCount) };
                                memcpy(boneDataMap + offset, command.BoneMatrices, command.BoneCount * sizeof(glm::mat4));
                                
                                objectDataMap[i].boneOffset = offset;
                                objectDataMap[i].isAnimated = 1;
                            }
                            else
                            {
                                objectDataMap[i].boneOffset = 0;
                                objectDataMap[i].isAnimated = 0;
                            }
                        }
                    }));
                }

                for (auto& h : s_DrawHandles) { h.wait(); }
            }

        #ifdef ANTELOPE_EDITOR_MODE
            for (uint32_t i { 0 }; i < objectCount; ++i)
            {
                if (m_SelectedEntityIDs.count(renderList[i].entityID))
                {
                    selectedIndirectIndices.push_back(i);
                }
            }
        #endif
        }
        else
        {
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
                objectDataMap[objectCount].tangentOffset = command.mesh.tangentOffset;
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
        }

        UpdateUniformBuffer(m_CurrentFrame, cameraData);

        for (uint32_t cascade { 0 }; cascade < 2; cascade++)
        {
            vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &cascade);
            m_ShadowPasses[cascade]->Draw(cmd, m_DescriptorSets[m_CurrentFrame], objectCount, m_IndirectBuffers[m_CurrentFrame]->GetBuffer());
        }

        VkRenderPassBeginInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_SceneHDRTexture->GetRenderPass();
        renderPassInfo.framebuffer = m_SceneHDRTexture->GetFramebuffer();
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_SceneHDRTexture->GetExtent();
        
        std::array<VkClearValue, 2> clearValues {};
        clearValues[0] = {};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport {};
        viewport.x = 0.0f; viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_SceneHDRTexture->GetExtent().width);
        viewport.height = static_cast<float>(m_SceneHDRTexture->GetExtent().height);
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset = {0, 0};
        scissor.extent = m_SceneHDRTexture->GetExtent();
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        
        m_MainPipeline->Bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[m_CurrentFrame], 0, nullptr);

        if (objectCount > 0)
        {
            vkCmdDrawIndirect(cmd, m_IndirectBuffers[m_CurrentFrame]->GetBuffer(), 0, objectCount, sizeof(VkDrawIndirectCommand));
        }

        m_SkyBoxPass->Draw(cmd, m_DescriptorSets[m_CurrentFrame]);

    #ifdef ANTELOPE_EDITOR_MODE
        m_EditorGridPass->Draw(cmd, m_DescriptorSets[m_CurrentFrame]);
    #endif
        
        vkCmdEndRenderPass(cmd);

        VkImageMemoryBarrier resolveBarrier {};
        resolveBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        resolveBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; 
        resolveBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        resolveBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        resolveBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        resolveBarrier.image = m_SceneHDRTexture->GetResolveImage(); 
        resolveBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &resolveBarrier);

        std::array<VkClearValue, 2> postClearValues {};
        postClearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        postClearValues[1].depthStencil = {1.0f, 0};
        
        VkRenderPassBeginInfo postPassInfo {};
        postPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

    #ifdef ANTELOPE_EDITOR_MODE
        postPassInfo.renderPass = m_FinalLDRTexture->GetRenderPass();
        postPassInfo.framebuffer = m_FinalLDRTexture->GetFramebuffer();
        postPassInfo.renderArea.extent = m_FinalLDRTexture->GetExtent();
    #else
        postPassInfo.renderPass = m_SwapChain->GetRenderPass();
        postPassInfo.framebuffer = m_SwapChain->GetFramebuffers()[m_CurrentImageIndex];
        postPassInfo.renderArea.extent = m_SwapChain->GetExtent();
    #endif
        postPassInfo.renderArea.offset = {0, 0};
        postPassInfo.clearValueCount = static_cast<uint32_t>(postClearValues.size());
        postPassInfo.pClearValues = postClearValues.data();

        auto projectToScreenUV { [&](glm::vec4 dirW) -> glm::vec2
        {
            glm::vec4 clip { cameraData.proj * cameraData.view * glm::vec4(glm::vec3(dirW) * 1000.0f, 1.0f) };

            if (clip.w <= 0.0f) { return glm::vec2(-2.0f); }
            
            glm::vec2 ndc { glm::vec2(clip.x, clip.y) / clip.w };
            return ndc * 0.5f + 0.5f;
        }};

        glm::vec2 sunUV { (cameraData.sunDirection.w  > 0.5f) ? projectToScreenUV(cameraData.sunDirection)  : glm::vec2(-2.0f) };
        glm::vec2 moonUV { (cameraData.moonDirection.w > 0.5f) ? projectToScreenUV(cameraData.moonDirection) : glm::vec2(-2.0f) };
        
        float nightFade = 1.0f - glm::smoothstep(0.0f, 0.35f, cameraData.sunDirection.y);
        m_BloomPass->Draw(cmd, m_SceneHDRTexture, 1.8f, 0.1f, sunUV, cameraData.sunColor.a, moonUV, cameraData.moonColor.a);

        vkCmdBeginRenderPass(cmd, &postPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        viewport.width = static_cast<float>(postPassInfo.renderArea.extent.width);
        viewport.height = static_cast<float>(postPassInfo.renderArea.extent.height);
        scissor.extent = postPassInfo.renderArea.extent;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        m_PostCompositePass->Draw(cmd, cameraData.time, 1.0f);

        vkCmdEndRenderPass(cmd);

    #ifdef ANTELOPE_EDITOR_MODE
        if (!selectedIndirectIndices.empty())
        {
            m_OutlinePass->DrawMask(cmd, selectedIndirectIndices, m_IndirectBuffers[m_CurrentFrame]->GetBuffer(), m_DescriptorSets[m_CurrentFrame], m_OutlineColor);
            m_OutlinePass->DrawComposite(cmd);
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

        AE_ENGINE_TRACE("Synchronization objects destroyed.");
    }

    void Renderer::CreateGraphicsPipeline()
    {
        PipelineConfigInfo pipelineConfig {};
        Pipeline::DefaultPipelineConfigInfo(pipelineConfig, m_Context);
        pipelineConfig.pipelineLayout = m_PipelineLayout;
    #ifdef ANTELOPE_EDITOR_MODE
        pipelineConfig.renderPass = m_SceneHDRTexture->GetRenderPass();
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
        QueueFamilyIndices queueFamilyIndices { m_Context->FindQueueFamilies(m_Context->GetPhysicalDevice()) };

        VkCommandPoolCreateInfo commandPoolInfo {};
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

        VkCommandBufferAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = m_CommandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = static_cast<uint32_t>(m_MaxFramesInFlight);

        if (vkAllocateCommandBuffers(m_Context->GetDevice(), &allocateInfo, m_CommandBuffers.data()) != VK_SUCCESS)
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
                AE_ENGINE_CRITICAL("Failed to create per-frame synchronization objects!");
                throw std::runtime_error("Failed to create per-frame synchronization objects");
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

            AE_ENGINE_TRACE("Synchronization objects created. {0} frame slots, {1} semaphore pool slots (maintenance1).", m_MaxFramesInFlight, poolSize);
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

            AE_ENGINE_TRACE("Synchronization objects created. {0} frame slots, ring size: {1} (fallback).", m_MaxFramesInFlight, ringSize);
        }
    #else
        AE_ENGINE_TRACE("Synchronization objects created. {0} frame slots.", m_MaxFramesInFlight);
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
        VkDeviceSize bufferSize { sizeof(ObjectData) * MAX_OBJECTS };
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

    void Renderer::CreateBoneBuffers()
    {
        VkDeviceSize bufferSize { sizeof(glm::mat4) * 2000 };
        m_BoneBuffers.reserve(m_MaxFramesInFlight);
        for (size_t i { 0 }; i < m_MaxFramesInFlight; i++)
        {
            m_BoneBuffers.push_back(std::make_unique<Buffer>(
                m_Context, 
                bufferSize, 
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
                VMA_MEMORY_USAGE_AUTO, 
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            ));
        }
    }

    void Renderer::CreateMaterialBuffers()
    {
        VkDeviceSize bufferSize { sizeof(PBRMaterialData) * MAX_MATERIALS };
        m_MaterialBuffers.reserve(m_MaxFramesInFlight);

        for (size_t i { 0 }; i < m_MaxFramesInFlight; i++) 
        {
            m_MaterialBuffers.push_back(std::make_unique<Buffer>(
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
        poolSizes[1].descriptorCount = static_cast<uint32_t>(m_MaxFramesInFlight * 7);

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
            writer.WriteBuffer(8, m_MaterialBuffers[i]->GetBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(9, m_GpuAllocator->GetTangentBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteImage(10, m_ShadowPasses[0]->GetDepthImageView(), m_ShadowPasses[0]->GetSampler(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            writer.WriteImage(11, m_ShadowPasses[1]->GetDepthImageView(), m_ShadowPasses[1]->GetSampler(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            writer.WriteBuffer(12, m_GpuAllocator->GetJointBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            writer.WriteBuffer(13, m_BoneBuffers[i]->GetBuffer(), VK_WHOLE_SIZE, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
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
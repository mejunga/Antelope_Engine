#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/UI/ScenePicker.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Vulkan/VulkanBuffer.hpp>
#include <Engine/Debug/Log.hpp>

#include <array>
#include <stdexcept>


namespace Antelope
{
    ScenePicker::ScenePicker(std::shared_ptr<VulkanContext> context, uint32_t width, uint32_t height)
        : m_Context(context), m_Width(width), m_Height(height)
    {
        CreateRenderPass();
        CreateResources(m_Width, m_Height);
        CreatePipeline();
        CreateStagingBuffer();
    }

    ScenePicker::~ScenePicker()
    {
        DestroyResources();
        
        auto device { m_Context->GetDevice() };
        auto allocator { m_Context->GetAllocator() };

        if (m_Pipeline) { m_Pipeline.reset(); }
        if (m_RenderPass) { vkDestroyRenderPass(device, m_RenderPass, nullptr); }
        if (m_StagingBuffer) { vmaDestroyBuffer(allocator, m_StagingBuffer, m_StagingAlloc); }
    }

    void ScenePicker::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0 || (m_Width == width && m_Height == height)) { return; }
        
        m_Width = width;
        m_Height = height;
        
        vkDeviceWaitIdle(m_Context->GetDevice());
        DestroyResources();
        CreateResources(m_Width, m_Height);
    }

    void ScenePicker::DestroyResources()
    {
        auto device { m_Context->GetDevice() };
        auto allocator { m_Context->GetAllocator() };

        if (m_Framebuffer) { vkDestroyFramebuffer(device, m_Framebuffer, nullptr); }
        if (m_ImageView) { vkDestroyImageView(device, m_ImageView, nullptr); }
        if (m_Image) { vmaDestroyImage(allocator, m_Image, m_ImageAlloc); }
        if (m_DepthImageView) { vkDestroyImageView(device, m_DepthImageView, nullptr); }
        if (m_DepthImage) { vmaDestroyImage(allocator, m_DepthImage, m_DepthAlloc); }

        m_Framebuffer = VK_NULL_HANDLE;
        m_ImageView = VK_NULL_HANDLE;
        m_Image = VK_NULL_HANDLE;
        m_DepthImageView = VK_NULL_HANDLE;
        m_DepthImage = VK_NULL_HANDLE;
    }

    void ScenePicker::CreateRenderPass()
    {
        VkAttachmentDescription colorAttachment {};
        colorAttachment.flags = 0;
        colorAttachment.format = VK_FORMAT_R32_UINT;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; 

        VkAttachmentDescription depthAttachment {};
        depthAttachment.flags = 0;
        depthAttachment.format = m_Context->FindSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, 
            VK_IMAGE_TILING_OPTIMAL, 
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef {};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef {};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.flags = 0;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = &depthRef;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;

        VkSubpassDependency dependency {};
        dependency.srcSubpass = 0;
        dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        std::array<VkAttachmentDescription, 2> attachments { colorAttachment, depthAttachment };
        
        VkRenderPassCreateInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.pNext = nullptr;
        renderPassInfo.flags = 0;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_Context->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create picking render pass!");
            throw std::runtime_error("Failed to create picking render pass");
        }
    }

    void ScenePicker::CreateResources(uint32_t width, uint32_t height)
    {
        auto allocator { m_Context->GetAllocator() };

        VkImageCreateInfo imgInfo {};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.pNext = nullptr;
        imgInfo.flags = 0;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.extent.width = width;
        imgInfo.extent.height = height;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.format = VK_FORMAT_R32_UINT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(allocator, &imgInfo, &allocationInfo, &m_Image, &m_ImageAlloc, nullptr);

        VkImageViewCreateInfo viewInfo {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.pNext = nullptr;
        viewInfo.flags = 0;
        viewInfo.image = m_Image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R32_UINT;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_ImageView);

        imgInfo.format = m_Context->FindSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, 
            VK_IMAGE_TILING_OPTIMAL, 
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
        imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        vmaCreateImage(allocator, &imgInfo, &allocationInfo, &m_DepthImage, &m_DepthAlloc, nullptr);
        
        viewInfo.image = m_DepthImage;
        viewInfo.format = imgInfo.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_DepthImageView);

        std::array<VkImageView, 2> fbAttachments { m_ImageView, m_DepthImageView };
        VkFramebufferCreateInfo fbInfo {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.pNext = nullptr;
        fbInfo.flags = 0;
        fbInfo.renderPass = m_RenderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(fbAttachments.size());
        fbInfo.pAttachments = fbAttachments.data();
        fbInfo.width = width;
        fbInfo.height = height;
        fbInfo.layers = 1;
        vkCreateFramebuffer(m_Context->GetDevice(), &fbInfo, nullptr, &m_Framebuffer);
    }

    void ScenePicker::CreatePipeline()
    {
        auto renderer { Application::Get().GetRenderer() };
        PipelineConfigInfo config {};
        Pipeline::DefaultPipelineConfigInfo(config, m_Context);
        
        config.renderPass = m_RenderPass;
        config.pipelineLayout = renderer->m_PipelineLayout;
        config.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        m_Pipeline = std::make_unique<Pipeline>(m_Context, "Assets/Shaders/editor_picking.vert.spv", "Assets/Shaders/editor_picking.frag.spv", config);
    }

    void ScenePicker::CreateStagingBuffer()
    {
        VkBufferCreateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.pNext = nullptr;
        bufferInfo.flags = 0;
        bufferInfo.size = sizeof(uint32_t);
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocationInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;

        vmaCreateBuffer(m_Context->GetAllocator(), &bufferInfo, &allocationInfo, &m_StagingBuffer, &m_StagingAlloc, nullptr);
    }

    uint32_t ScenePicker::GetEntityIDAtPixel(uint32_t x, uint32_t y, const EditorCamera& camera, const std::vector<RenderCommand>& renderList)
    {
        if (x >= m_Width || y >= m_Height || renderList.empty()) return static_cast<uint32_t>(entt::null);

        auto renderer { Application::Get().GetRenderer() };
        uint32_t frameIndex { renderer->m_CurrentFrame };

        UniformBufferObject cameraData {};
        cameraData.view = camera.GetViewMatrix();
        cameraData.proj = camera.GetProjectionMatrix(static_cast<float>(m_Width), static_cast<float>(m_Height));
        renderer->UpdateUniformBuffer(frameIndex, cameraData);

        VkCommandBuffer cmd { renderer->BeginAsyncGraphicsCommand() };

        VkRenderPassBeginInfo rpInfo {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.pNext = nullptr;
        rpInfo.renderPass = m_RenderPass; 
        rpInfo.framebuffer = m_Framebuffer;
        rpInfo.renderArea.offset.x = 0;
        rpInfo.renderArea.offset.y = 0;
        rpInfo.renderArea.extent.width = m_Width;
        rpInfo.renderArea.extent.height = m_Height;
        
        std::array<VkClearValue, 2> clearValues {};
        clearValues[0].color.uint32[0] = static_cast<uint32_t>(entt::null);
        clearValues[0].color.uint32[1] = 0;
        clearValues[0].color.uint32[2] = 0;
        clearValues[0].color.uint32[3] = 0;
        clearValues[1].depthStencil.depth = 1.0f;
        clearValues[1].depthStencil.stencil = 0;

        rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size()); 
        rpInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        m_Pipeline->Bind(cmd);

        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_Width);
        viewport.height = static_cast<float>(m_Height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset.x = static_cast<int32_t>(x);
        scissor.offset.y = static_cast<int32_t>(y);
        scissor.extent.width = 1;
        scissor.extent.height = 1;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->m_PipelineLayout, 0, 1, &renderer->m_DescriptorSets[frameIndex], 0, nullptr);
        
        ObjectData* objectDataMap { static_cast<ObjectData*>(renderer->m_ObjectBuffers[frameIndex]->GetMappedMemory()) };
        VkDrawIndirectCommand* indirectCommandsMap { static_cast<VkDrawIndirectCommand*>(renderer->m_IndirectBuffers[frameIndex]->GetMappedMemory()) };
        
        uint32_t objectCount { 0 };
        for (const auto& command : renderList) 
        {
            objectDataMap[objectCount].model = command.transform;
            objectDataMap[objectCount].posOffset = command.mesh.posAllocation.Offset / sizeof(VertexPosition);
            objectDataMap[objectCount].faceOffset = command.mesh.faceAllocation.Offset / sizeof(Face);
            objectDataMap[objectCount].entityID = command.entityID;
            
            indirectCommandsMap[objectCount].vertexCount = command.mesh.faceCount * 3;
            indirectCommandsMap[objectCount].instanceCount = 1;
            indirectCommandsMap[objectCount].firstVertex = 0;
            indirectCommandsMap[objectCount].firstInstance = objectCount;
            objectCount++;
        }

        vkCmdDrawIndirect(cmd, renderer->m_IndirectBuffers[frameIndex]->GetBuffer(), 0, objectCount, sizeof(VkDrawIndirectCommand));
        vkCmdEndRenderPass(cmd);

        VkBufferImageCopy region {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset.x = static_cast<int32_t>(x);
        region.imageOffset.y = static_cast<int32_t>(y);
        region.imageOffset.z = 0;
        region.imageExtent.width = 1;
        region.imageExtent.height = 1;
        region.imageExtent.depth = 1;
        
        vkCmdCopyImageToBuffer(cmd, m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_StagingBuffer, 1, &region);
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = nullptr;
        submitInfo.commandBufferCount = 1; 
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;

        vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_Context->GetGraphicsQueue()); 
        vkFreeCommandBuffers(m_Context->GetDevice(), renderer->m_CommandPool, 1, &cmd);
        vmaInvalidateAllocation(m_Context->GetAllocator(), m_StagingAlloc, 0, sizeof(uint32_t));

        uint32_t entityID { static_cast<uint32_t>(entt::null) };

        VmaAllocationInfo allocationInfo {};
        vmaGetAllocationInfo(m_Context->GetAllocator(), m_StagingAlloc, &allocationInfo);
        memcpy(&entityID, allocationInfo.pMappedData, sizeof(uint32_t));

        return entityID;
    }
}
#endif
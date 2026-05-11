#include <Engine/Renderer/Vulkan/GpuMemoryAllocator.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Graphics/Mesh.hpp>
#include <Engine/Debug/Log.hpp>

#include <stdexcept>


namespace Antelope
{
    PagedVirtualBuffer::PagedVirtualBuffer(std::shared_ptr<VulkanContext> context, VkDeviceSize pageSize, VkBufferUsageFlags usage, bool hostVisible, const std::string& name)
        : m_Context(context), m_PageSize(pageSize), m_Usage(usage), m_HostVisible(hostVisible), m_Name(name)
    {
        CreateNewPage(); 
    }

    PagedVirtualBuffer::~PagedVirtualBuffer()
    {
        for (auto& page : m_Pages)
        {
            if (page.VirtualBlock != VK_NULL_HANDLE)
            {
                vmaClearVirtualBlock(page.VirtualBlock);
                vmaDestroyVirtualBlock(page.VirtualBlock);
            }

            if (page.Buffer != VK_NULL_HANDLE)
            {
                vmaDestroyBuffer(m_Context->GetAllocator(), page.Buffer, page.Memory);
            }
        }

        m_Pages.clear();
    }

    VirtualAllocation PagedVirtualBuffer::Allocate(VkDeviceSize size, VkDeviceSize alignment)
    {
        if (size == 0) return {};

        VmaVirtualAllocationCreateInfo allocationInfo {};
        allocationInfo.size = size;
        allocationInfo.alignment = alignment;

        VirtualAllocation result {};
        result.Size = size;

        for (size_t i { 0 }; i < m_Pages.size(); ++i)
        {
            VkResult res { vmaVirtualAllocate(m_Pages[i].VirtualBlock, &allocationInfo, &result.Handle, &result.Offset) };

            if (res == VK_SUCCESS)
            {
                result.PageIndex = static_cast<uint32_t>(i);
                return result;
            }
        }

        AE_ENGINE_WARN("Buffer '{0}' is full! Allocating a new Page", m_Name);
        CreateNewPage();

        uint32_t newPageIndex { static_cast<uint32_t>(m_Pages.size() - 1) };

        if (vmaVirtualAllocate(m_Pages[newPageIndex].VirtualBlock, &allocationInfo, &result.Handle, &result.Offset) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to allocate {0} bytes in the newly created page of '{1}'!", size, m_Name);
            throw std::runtime_error("Virtual Allocation failed even after creating a new page");
        }

        result.PageIndex = newPageIndex;
        return result;
    }

    void PagedVirtualBuffer::Free(const VirtualAllocation& allocation)
    {
        if (allocation.Handle != VK_NULL_HANDLE)
        {
            vmaVirtualFree(m_Pages[allocation.PageIndex].VirtualBlock, allocation.Handle);
        }
    }

    void PagedVirtualBuffer::CreateNewPage()
    {
        BufferPage newPage {};

        VkBufferCreateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = m_PageSize;
        bufferInfo.usage = m_Usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo memoryInfo {};
        memoryInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if (m_HostVisible)
        {
            memoryInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        VmaAllocationInfo allocationResult;

        if (vmaCreateBuffer(m_Context->GetAllocator(), &bufferInfo, &memoryInfo, &newPage.Buffer, &newPage.Memory, &allocationResult) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create physical memory page for '{0}'", m_Name);
            throw std::runtime_error("VMA Buffer Creation failed");
        }

        newPage.MappedData = allocationResult.pMappedData;

        VmaVirtualBlockCreateInfo virtualBlockInfo {};
        virtualBlockInfo.size = m_PageSize;

        if (vmaCreateVirtualBlock(&virtualBlockInfo, &newPage.VirtualBlock) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Virtual Block for '{0}'", m_Name);
            throw std::runtime_error("VMA Virtual Block Creation failed");
        }

        m_Pages.push_back(newPage);
        AE_ENGINE_TRACE("Created new page (Index: {0}) for '{1}' - Size: {2} MB", m_Pages.size() - 1, m_Name, m_PageSize / (1024 * 1024));
    }

    GpuMemoryAllocator::GpuMemoryAllocator(std::shared_ptr<VulkanContext> context)
        : m_Context(context)
    {
        VkBufferUsageFlags geomUsage { VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT };

        m_PosBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "PositionBuffer");
        m_ColorBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "ColorBuffer");
        m_NormalBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "NormalBuffer");
        m_UvBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "UvBuffer");
        m_TangentBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "TangentBuffer");
        m_FaceBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "FaceBuffer");

        AE_ENGINE_INFO("GpuMemoryAllocator initialized with 128 MB page size.");
    }

    MeshAllocationResult GpuMemoryAllocator::AllocateMesh(VkDeviceSize posSize, VkDeviceSize colorSize, VkDeviceSize normalSize, 
                                                          VkDeviceSize uvSize, VkDeviceSize tangentSize, VkDeviceSize faceSize, uint32_t meshID)
    {
        VirtualAllocation posAlloc { m_PosBuffer->Allocate(posSize) };
        VirtualAllocation colorAlloc { m_ColorBuffer->Allocate(colorSize) };
        VirtualAllocation normalAlloc { m_NormalBuffer->Allocate(normalSize) };
        VirtualAllocation uvAlloc { m_UvBuffer->Allocate(uvSize) };
        VirtualAllocation tangentAlloc { m_TangentBuffer->Allocate(tangentSize) };
        VirtualAllocation faceAlloc { m_FaceBuffer->Allocate(faceSize) };

        m_FreeDataMap[meshID] = { posAlloc, colorAlloc, normalAlloc, uvAlloc, tangentAlloc, faceAlloc };


        MeshAllocationResult result {};
        result.MeshID = meshID;
        result.posOffset = static_cast<uint32_t>(posAlloc.Offset / sizeof(VertexPosition));
        result.colorOffset = static_cast<uint32_t>(colorAlloc.Offset / sizeof(VertexColor));
        result.normalOffset = static_cast<uint32_t>(normalAlloc.Offset / sizeof(VertexNormal));
        result.uvOffset = static_cast<uint32_t>(uvAlloc.Offset / sizeof(VertexUV));
        result.tangentOffset = static_cast<uint32_t>(tangentAlloc.Offset / sizeof(VertexTangent));
        result.faceOffset = static_cast<uint32_t>(faceAlloc.Offset / sizeof(Face));

        result.pos = { m_PosBuffer->GetBuffer(posAlloc.PageIndex), posAlloc.Offset };
        result.color = { m_ColorBuffer->GetBuffer(colorAlloc.PageIndex), colorAlloc.Offset };
        result.normal = { m_NormalBuffer->GetBuffer(normalAlloc.PageIndex), normalAlloc.Offset };
        result.uv = { m_UvBuffer->GetBuffer(uvAlloc.PageIndex), uvAlloc.Offset };
        result.tangent = { m_TangentBuffer->GetBuffer(tangentAlloc.PageIndex), tangentAlloc.Offset };
        result.face = { m_FaceBuffer->GetBuffer(faceAlloc.PageIndex), faceAlloc.Offset };

        return result;
    }

    void GpuMemoryAllocator::FreeMesh(uint32_t meshID)
    {
        auto it { m_FreeDataMap.find(meshID) };
        if (it == m_FreeDataMap.end()) { return; }

        const MeshFreeData& data { it->second };
        m_PosBuffer->Free(data.pos);
        m_ColorBuffer->Free(data.color);
        m_NormalBuffer->Free(data.normal);
        m_UvBuffer->Free(data.uv);
        m_TangentBuffer->Free(data.tangent);
        m_FaceBuffer->Free(data.face);

        m_FreeDataMap.erase(it);
    }
}
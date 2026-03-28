#include <Engine/Renderer/Vulkan/GpuMemoryAllocator.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
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
        AE_ENGINE_TRACE("PagedVirtualBuffer '{0}' destroyed.", m_Name);
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
        AE_ENGINE_INFO("Created new page (Index: {0}) for '{1}' - Size: {2} MB", m_Pages.size() - 1, m_Name, m_PageSize / (1024 * 1024));
    }

    GpuMemoryAllocator::GpuMemoryAllocator(std::shared_ptr<VulkanContext> context)
        : m_Context(context)
    {
        VkBufferUsageFlags geomUsage { VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT };

        m_PosBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "PositionBuffer");
        m_ColorBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "ColorBuffer");
        m_NormalBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "NormalBuffer");
        m_UvBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "UvBuffer");
        m_FaceBuffer = std::make_shared<PagedVirtualBuffer>(context, PAGE_SIZE, geomUsage, false, "FaceBuffer");

        AE_ENGINE_INFO("GpuMemoryAllocator initialized with 128 MB page size.");
    }

    VirtualAllocation GpuMemoryAllocator::AllocatePosition(VkDeviceSize size) { return m_PosBuffer->Allocate(size); }
    VirtualAllocation GpuMemoryAllocator::AllocateColor(VkDeviceSize size) { return m_ColorBuffer->Allocate(size); }
    VirtualAllocation GpuMemoryAllocator::AllocateNormal(VkDeviceSize size) { return m_NormalBuffer->Allocate(size); }
    VirtualAllocation GpuMemoryAllocator::AllocateUV(VkDeviceSize size) { return m_UvBuffer->Allocate(size); }
    VirtualAllocation GpuMemoryAllocator::AllocateFace(VkDeviceSize size) { return m_FaceBuffer->Allocate(size); }

    void GpuMemoryAllocator::FreePosition(const VirtualAllocation& allocation) { m_PosBuffer->Free(allocation); }
    void GpuMemoryAllocator::FreeColor(const VirtualAllocation& allocation) { m_ColorBuffer->Free(allocation); }
    void GpuMemoryAllocator::FreeNormal(const VirtualAllocation& allocation) { m_NormalBuffer->Free(allocation); }
    void GpuMemoryAllocator::FreeUV(const VirtualAllocation& allocation) { m_UvBuffer->Free(allocation); }
    void GpuMemoryAllocator::FreeFace(const VirtualAllocation& allocation) { m_FaceBuffer->Free(allocation); }
}
#include <Engine/Renderer/VulkanBuffer.hpp>
#include <Engine/Renderer/VulkanContext.hpp>
#include <Engine/Debug/Log.hpp>

#include <stdexcept>
#include <cstring>


namespace Antelope
{
    VulkanBuffer::VulkanBuffer(std::shared_ptr<VulkanContext> context, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags)
        : m_Context(context), m_BufferSize(size)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;
        allocInfo.flags = flags;

        VmaAllocationInfo allocationResult;
        if (vmaCreateBuffer(m_Context->GetAllocator(), &bufferInfo, &allocInfo, &m_Buffer, &m_Allocation, &allocationResult) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Vulkan buffer! Size: {0}", size);
            throw std::runtime_error("Failed to create Vulkan buffer");
        }

        if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)
        {
            m_MappedData = allocationResult.pMappedData;
        }
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (m_Buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(m_Context->GetAllocator(), m_Buffer, m_Allocation);
        }
    }

    void VulkanBuffer::WriteToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        if (m_MappedData == nullptr) { return; }

        if (size == VK_WHOLE_SIZE)
        {
            memcpy(m_MappedData, data, m_BufferSize);
        }
        else
        {
            char* memOffset = (char*)m_MappedData;
            memOffset += offset;
            memcpy(memOffset, data, size);
        }
    }
}
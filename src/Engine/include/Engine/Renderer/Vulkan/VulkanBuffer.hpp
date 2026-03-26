#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <memory>


namespace Antelope
{
    class VulkanContext;
    
    class VulkanBuffer
    {
        public:
            VulkanBuffer(std::shared_ptr<VulkanContext> context, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags = 0);
            ~VulkanBuffer();

            VulkanBuffer(const VulkanBuffer&) = delete;
            VulkanBuffer& operator=(const VulkanBuffer&) = delete;

            void WriteToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

            VkBuffer GetBuffer() const { return m_Buffer; }
            void* GetMappedMemory() const { return m_MappedData; }
            VkDeviceSize GetSize() const { return m_BufferSize; }
            
        private:
            std::shared_ptr<VulkanContext> m_Context;
            void* m_MappedData { nullptr };

            VkBuffer m_Buffer { VK_NULL_HANDLE };
            VmaAllocation m_Allocation { VK_NULL_HANDLE };
            
            VkDeviceSize m_BufferSize;
    };
}
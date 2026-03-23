#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <memory>
#include <string>

namespace Antelope
{
    class VulkanContext;

    struct VirtualAllocation 
    {
        uint32_t PageIndex { 0 };
        VmaVirtualAllocation Handle { VK_NULL_HANDLE };
        VkDeviceSize Offset { 0 };
        VkDeviceSize Size { 0 };
    };

    struct BufferPage 
    {
        VkBuffer Buffer { VK_NULL_HANDLE };
        VmaAllocation Memory { VK_NULL_HANDLE };
        VmaVirtualBlock VirtualBlock { VK_NULL_HANDLE };
        void* MappedData { nullptr };
    };

    class PagedVirtualBuffer 
    {
        public:
            PagedVirtualBuffer(std::shared_ptr<VulkanContext> context, VkDeviceSize pageSize, VkBufferUsageFlags usage, bool hostVisible, const std::string& name);
            ~PagedVirtualBuffer();

            VirtualAllocation Allocate(VkDeviceSize size, VkDeviceSize alignment = 16);
            void Free(const VirtualAllocation& allocation);

            inline VkBuffer GetBuffer(uint32_t pageIndex) const { return m_Pages[pageIndex].Buffer; }
            inline size_t GetPageCount() const { return m_Pages.size(); }

        private:
            void CreateNewPage();

        private:
            std::shared_ptr<VulkanContext> m_Context;
            VkDeviceSize m_PageSize;
            VkBufferUsageFlags m_Usage;
            bool m_HostVisible;
            std::string m_Name;

            std::vector<BufferPage> m_Pages;
    };

    class GpuMemoryAllocator 
    {
        public:
            GpuMemoryAllocator(std::shared_ptr<VulkanContext> context);
            ~GpuMemoryAllocator() = default;

            VirtualAllocation AllocatePosition(VkDeviceSize size);
            VirtualAllocation AllocateColor(VkDeviceSize size);
            VirtualAllocation AllocateNormal(VkDeviceSize size);
            VirtualAllocation AllocateUV(VkDeviceSize size);
            VirtualAllocation AllocateFace(VkDeviceSize size);

            void FreePosition(const VirtualAllocation& allocation);
            void FreeColor(const VirtualAllocation& allocation);
            void FreeNormal(const VirtualAllocation& allocation);
            void FreeUV(const VirtualAllocation& allocation);
            void FreeFace(const VirtualAllocation& allocation);

            inline std::shared_ptr<PagedVirtualBuffer> GetPosBuffer() const { return m_PosBuffer; }
            inline std::shared_ptr<PagedVirtualBuffer> GetColorBuffer() const { return m_ColorBuffer; }
            inline std::shared_ptr<PagedVirtualBuffer> GetNormalBuffer() const { return m_NormalBuffer; }
            inline std::shared_ptr<PagedVirtualBuffer> GetUvBuffer() const { return m_UvBuffer; }
            inline std::shared_ptr<PagedVirtualBuffer> GetFaceBuffer() const { return m_FaceBuffer; }

        private:
            std::shared_ptr<VulkanContext> m_Context;

            const VkDeviceSize PAGE_SIZE = 128 * 1024 * 1024;

            std::shared_ptr<PagedVirtualBuffer> m_PosBuffer;
            std::shared_ptr<PagedVirtualBuffer> m_ColorBuffer;
            std::shared_ptr<PagedVirtualBuffer> m_NormalBuffer;
            std::shared_ptr<PagedVirtualBuffer> m_UvBuffer;
            std::shared_ptr<PagedVirtualBuffer> m_FaceBuffer;
    };
}
#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>


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

    struct MeshFreeData
    {
        VirtualAllocation pos, color, normal, uv, tangent, face, joint;
    };

    struct MeshAllocationResult
    {
        struct AttribCopyInfo
        {
            VkBuffer Buffer { VK_NULL_HANDLE };
            VkDeviceSize Offset { 0 };
        };

        uint32_t MeshID { 0 };
        uint32_t posOffset { 0 };
        uint32_t colorOffset { 0 };
        uint32_t normalOffset { 0 };
        uint32_t uvOffset { 0 };
        uint32_t tangentOffset { 0 };
        uint32_t faceOffset { 0 };
        uint32_t jointOffset { 0 };

        AttribCopyInfo pos;
        AttribCopyInfo color;
        AttribCopyInfo normal;
        AttribCopyInfo uv;
        AttribCopyInfo tangent;
        AttribCopyInfo face;
        AttribCopyInfo joint;
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

            MeshAllocationResult AllocateMesh(VkDeviceSize posSize, VkDeviceSize colorSize, VkDeviceSize normalSize, VkDeviceSize uvSize, 
                                              VkDeviceSize tangentSize, VkDeviceSize faceSize, VkDeviceSize jointSize, uint32_t meshID);
            void FreeMesh(uint32_t meshID);

            inline VkBuffer GetPosBuffer(uint32_t pageIndex = 0) const { return m_PosBuffer->GetBuffer(pageIndex); }
            inline VkBuffer GetColorBuffer(uint32_t pageIndex = 0) const { return m_ColorBuffer->GetBuffer(pageIndex); }
            inline VkBuffer GetNormalBuffer(uint32_t pageIndex = 0) const { return m_NormalBuffer->GetBuffer(pageIndex); }
            inline VkBuffer GetUvBuffer(uint32_t pageIndex = 0) const { return m_UvBuffer->GetBuffer(pageIndex); }
            inline VkBuffer GetTangentBuffer(uint32_t pageIndex = 0) const { return m_TangentBuffer->GetBuffer(pageIndex); }
            inline VkBuffer GetFaceBuffer(uint32_t pageIndex = 0) const { return m_FaceBuffer->GetBuffer(pageIndex); }
            inline VkBuffer GetJointBuffer(uint32_t pageIndex = 0) const { return m_JointBuffer->GetBuffer(pageIndex); }
            
        private:
            std::shared_ptr<VulkanContext> m_Context;

            const VkDeviceSize PAGE_SIZE { 128 * 1024 * 1024 };

            std::shared_ptr<PagedVirtualBuffer> m_PosBuffer;
            std::shared_ptr<PagedVirtualBuffer> m_ColorBuffer;
            std::shared_ptr<PagedVirtualBuffer> m_NormalBuffer;
            std::shared_ptr<PagedVirtualBuffer> m_UvBuffer;
            std::shared_ptr<PagedVirtualBuffer> m_TangentBuffer;
            std::shared_ptr<PagedVirtualBuffer> m_FaceBuffer;
            std::shared_ptr<PagedVirtualBuffer> m_JointBuffer;

            std::unordered_map<uint32_t, MeshFreeData> m_FreeDataMap;
    };
}
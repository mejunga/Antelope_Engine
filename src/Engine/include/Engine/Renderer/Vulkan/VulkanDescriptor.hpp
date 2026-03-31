#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>
#include <deque>


namespace Antelope
{
    class VulkanContext;

    class DescriptorLayoutBuilder
    {
        public:
            void AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t count = 1);
            void Clear();
            VkDescriptorSetLayout Build(std::shared_ptr<VulkanContext> context, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);

        private:
            std::vector<VkDescriptorSetLayoutBinding> m_Bindings;
    };

    class DescriptorAllocator
    {
        public:
            struct PoolSizeRatio
            {
                VkDescriptorType type;
                float ratio;
            };

            void Init(std::shared_ptr<VulkanContext> context, uint32_t initialSets, std::vector<PoolSizeRatio> poolRatios);
            void DestroyAllocator();

            VkDescriptorSet Allocate(VkDescriptorSetLayout layout);

        private:
            VkDescriptorPool GetPool();
            VkDescriptorPool CreatePool(uint32_t setCount, std::vector<PoolSizeRatio> poolRatios);

        private:
            std::shared_ptr<VulkanContext> m_Context;
            
            uint32_t m_SetsPerPool;

            std::vector<PoolSizeRatio> m_Ratios;
            std::vector<VkDescriptorPool> m_FullPools;
            std::vector<VkDescriptorPool> m_ReadyPools;
    };

    class DescriptorWriter
    {
        public:
            void WriteBuffer(uint32_t binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);
            void WriteImage(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, VkDescriptorType type, uint32_t count = 1);
            void Clear();
            void UpdateSet(std::shared_ptr<VulkanContext> context, VkDescriptorSet set);

        private:
            std::deque<VkDescriptorBufferInfo> m_BufferInfos;
            std::deque<VkDescriptorImageInfo> m_ImageInfos;
            std::vector<VkWriteDescriptorSet> m_Writes;
    };
}
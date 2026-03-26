#include <Engine/Renderer/Vulkan/VulkanDescriptor.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Debug/Log.hpp>

#include <stdexcept>


namespace Antelope
{
    void DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t count)
    {
        VkDescriptorSetLayoutBinding newBind {};
        newBind.binding = binding;
        newBind.descriptorCount = count;
        newBind.descriptorType = type;
        newBind.stageFlags = stageFlags;
        m_Bindings.push_back(newBind);
    }

    void DescriptorLayoutBuilder::Clear() { m_Bindings.clear(); }

    VkDescriptorSetLayout DescriptorLayoutBuilder::Build(std::shared_ptr<VulkanContext> context, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
    {
        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = pNext;
        layoutInfo.pBindings = m_Bindings.data();
        layoutInfo.bindingCount = static_cast<uint32_t>(m_Bindings.size());
        layoutInfo.flags = flags;

        VkDescriptorSetLayout layout;
        if (vkCreateDescriptorSetLayout(context->GetDevice(), &layoutInfo, nullptr, &layout) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create descriptor set layout!");
            throw std::runtime_error("Failed to create descriptor set layout");
        }
        return layout;
    }

    void DescriptorAllocator::Init(std::shared_ptr<VulkanContext> context, uint32_t initialSets, std::vector<PoolSizeRatio> poolRatios)
    {
        m_Context = context;
        m_Ratios = poolRatios;
        m_SetsPerPool = initialSets;
        VkDescriptorPool newPool { CreatePool(initialSets, poolRatios) };
        m_ReadyPools.push_back(newPool);
    }

    VkDescriptorPool DescriptorAllocator::CreatePool(uint32_t setCount, std::vector<PoolSizeRatio> poolRatios)
    {
        std::vector<VkDescriptorPoolSize> poolSizes;

        for (auto r : poolRatios)
        {
            poolSizes.push_back({ r.type, static_cast<uint32_t>(r.ratio * setCount) });
        }

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = setCount;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        VkDescriptorPool newPool;
        vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &newPool);
        return newPool;
    }

    VkDescriptorSet DescriptorAllocator::Allocate(VkDescriptorSetLayout layout)
    {
        VkDescriptorPool pool { GetPool() };
        VkDescriptorSetAllocateInfo allocationInfo {};
        allocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocationInfo.descriptorPool = pool;
        allocationInfo.descriptorSetCount = 1;
        allocationInfo.pSetLayouts = &layout;

        VkDescriptorSet set;

        if (vkAllocateDescriptorSets(m_Context->GetDevice(), &allocationInfo, &set) != VK_SUCCESS)
        {
            m_FullPools.push_back(pool);
            pool = GetPool();
            allocationInfo.descriptorPool = pool;
            vkAllocateDescriptorSets(m_Context->GetDevice(), &allocationInfo, &set);
        }

        m_ReadyPools.push_back(pool);
        return set;
    }

    VkDescriptorPool DescriptorAllocator::GetPool()
    {
        VkDescriptorPool pool;
        
        if (!m_ReadyPools.empty())
        {
            pool = m_ReadyPools.back();
            m_ReadyPools.pop_back();
        }
        else
        {
            pool = CreatePool(m_SetsPerPool, m_Ratios);
        }
        
        return pool;
    }

    void DescriptorAllocator::DestroyAllocator()
    {
        for (auto p : m_ReadyPools) { vkDestroyDescriptorPool(m_Context->GetDevice(), p, nullptr); }
        for (auto p : m_FullPools) { vkDestroyDescriptorPool(m_Context->GetDevice(), p, nullptr); }
    }

    void DescriptorWriter::WriteBuffer(uint32_t binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
    {
        VkDescriptorBufferInfo& bufferInfo = m_BufferInfos.emplace_back(VkDescriptorBufferInfo{buffer, offset, size});
        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pBufferInfo = &bufferInfo;
        m_Writes.push_back(write);
    }

    void DescriptorWriter::WriteImage(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, VkDescriptorType type, uint32_t count)
    {
        VkDescriptorImageInfo& imageInfo { m_ImageInfos.emplace_back(VkDescriptorImageInfo{sampler, imageView, layout}) };
        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = binding;
        write.descriptorCount = count;
        write.descriptorType = type;
        write.pImageInfo = &imageInfo;
        m_Writes.push_back(write);
    }

    void DescriptorWriter::UpdateSet(std::shared_ptr<VulkanContext> context, VkDescriptorSet set)
    {
        for (auto& w : m_Writes) { w.dstSet = set; }
        vkUpdateDescriptorSets(context->GetDevice(), (uint32_t)m_Writes.size(), m_Writes.data(), 0, nullptr);
    }

    void DescriptorWriter::Clear()
    {
        m_Writes.clear(); m_BufferInfos.clear(); m_ImageInfos.clear();
    }
}
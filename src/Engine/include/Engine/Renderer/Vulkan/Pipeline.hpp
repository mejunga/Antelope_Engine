#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>


namespace Antelope
{
    class VulkanContext;

    struct PipelineConfigInfo
    {
        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisampleInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        uint32_t subpass = 0;
    };

    class Pipeline
    {
        public:
            Pipeline(std::shared_ptr<VulkanContext> context,
                           const std::string& vertFilepath,
                           const std::string& fragFilepath,
                           const PipelineConfigInfo& configInfo);
            
            ~Pipeline();

            Pipeline(const Pipeline&) = delete;
            Pipeline& operator=(const Pipeline&) = delete;

            void Bind(VkCommandBuffer commandBuffer);
            static void DefaultPipelineConfigInfo(PipelineConfigInfo& configInfo, std::shared_ptr<VulkanContext> context);

        private:
            static std::vector<char> ReadFile(const std::string& filepath);
            void CreateGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo);
            VkShaderModule CreateShaderModule(const std::vector<char>& code);

        private:
            std::shared_ptr<VulkanContext> m_Context;

            VkPipeline m_GraphicsPipeline;
            VkShaderModule m_VertShaderModule;
            VkShaderModule m_FragShaderModule;
    };
}
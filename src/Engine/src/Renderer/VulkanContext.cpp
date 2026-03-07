#define VMA_IMPLEMENTATION

#include <Engine/Renderer/VulkanContext.hpp>
#include <Engine/Debug/Log.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <limits>
#include <set>
#include <fstream>
#include <cstring>


namespace Antelope
{
    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance, 
        const VkDebugUtilsMessengerCreateInfoEXT *pMessengerInfo, 
        const VkAllocationCallbacks *pAllocator,
        VkDebugUtilsMessengerEXT *pMessenger
    )
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

        if(func != nullptr)
        {
            return(func(instance, pMessengerInfo, pAllocator, pMessenger));
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT messenger,
        const VkAllocationCallbacks *pAllocator
    )
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

        if(func != nullptr)
        {
            func(instance, messenger, pAllocator);
        }
    }

    VulkanContext::VulkanContext() {}
    VulkanContext::~VulkanContext() 
    {
        if (m_Device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_Device);
        }

        if (m_DescriptorPool != VK_NULL_HANDLE) 
        {
            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
            AE_ENGINE_TRACE("Vulkan Descriptor Pool destroyed.");
        }

        if (!m_UniformBuffers.empty()) 
        {
            for (size_t i = 0; i < m_UniformBuffers.size(); i++) 
            {
                if (m_UniformBuffers[i] != VK_NULL_HANDLE) 
                {
                    vmaDestroyBuffer(m_Allocator, m_UniformBuffers[i], m_UniformBuffersAllocations[i]);
                }
            }
            m_UniformBuffers.clear();
            m_UniformBuffersAllocations.clear();
            m_UniformBuffersMapped.clear();
            AE_ENGINE_TRACE("Vulkan Uniform Buffers destroyed.");
        }

        if (m_IndexBuffer != VK_NULL_HANDLE) 
        {
            vmaDestroyBuffer(m_Allocator, m_IndexBuffer, m_IndexBufferAllocation);
            AE_ENGINE_TRACE("Vulkan Index Buffer destroyed.");
        }

        if (m_VertexBuffer != VK_NULL_HANDLE) 
        {
            vmaDestroyBuffer(m_Allocator, m_VertexBuffer, m_VertexBufferAllocation);
            AE_ENGINE_TRACE("Vulkan Vertex Buffer destroyed.");
        }
   
        if (m_Allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(m_Allocator);
            AE_ENGINE_TRACE("VMA Allocator destroyed.");
        }

        if (!m_InFlightFences.empty()) 
        {
            for (size_t i = 0; i < m_InFlightFences.size(); i++) 
            {
                vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
            }
            m_InFlightFences.clear();
            AE_ENGINE_TRACE("Vulkan In-Flight Fences destroyed.");
        }

        if (!m_RenderFinishedSemaphores.empty()) 
        {
            for (size_t i = 0; i < m_RenderFinishedSemaphores.size(); i++) 
            {
                vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
            }
            m_RenderFinishedSemaphores.clear();
            AE_ENGINE_TRACE("Vulkan Render Finished Semaphores destroyed.");
        }

        if (!m_ImageAvailableSemaphores.empty()) 
        {
            for (size_t i = 0; i < m_ImageAvailableSemaphores.size(); i++) 
            {
                vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
            }
            m_ImageAvailableSemaphores.clear();
            AE_ENGINE_TRACE("Vulkan Image Available Semaphores destroyed.");
        }

        if(m_CommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
            AE_ENGINE_TRACE("Vulkan Command Pool destroyed.");
        }

        if (!m_SwapchainFramebuffers.empty()) 
        {
            for (auto framebuffer : m_SwapchainFramebuffers) 
            {
                vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
            }
            m_SwapchainFramebuffers.clear();
            AE_ENGINE_TRACE("Vulkan Framebuffers destroyed.");
        }

        if(m_GraphicsPipeline != VK_NULL_HANDLE) 
        {
            vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
            AE_ENGINE_TRACE("Vulkan Graphics Pipeline destroyed.");
        }
        if(m_PipelineLayout != VK_NULL_HANDLE) 
        {
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
            AE_ENGINE_TRACE("Vulkan Pipeline Layout destroyed.");
        }

        if (m_DescriptorSetLayout != VK_NULL_HANDLE) 
        {
            vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
            AE_ENGINE_TRACE("Vulkan Descriptor Set Layout destroyed.");
        }

        if(m_RenderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
            AE_ENGINE_TRACE("Vulkan Render Pass destroyed.");
        }

        if (!m_SwapchainImageViews.empty()) 
        {
            for (auto imageView : m_SwapchainImageViews) 
            {
                vkDestroyImageView(m_Device, imageView, nullptr);
            }
            m_SwapchainImageViews.clear(); 
            AE_ENGINE_TRACE("Vulkan Swapchain Image Views destroyed.");
        }

        if(m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
            AE_ENGINE_TRACE("Vulkan Swapchain destroyed.");
        }

        if(m_Device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_Device, nullptr);
            AE_ENGINE_TRACE("Vulkan Logical Device destroyed.");
        }

        if(m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            AE_ENGINE_TRACE("Vulkan Surface destroyed.");
        }

        if(m_EnableValidationLayers && m_DebugMessenger != VK_NULL_HANDLE)
        {
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
            AE_ENGINE_TRACE("Vulkan Debug Messenger destroyed.");
        }

        if(m_Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_Instance, nullptr);
            AE_ENGINE_TRACE("Vulkan Instance destroyed.");
        }
    }

    void VulkanContext::Init(GLFWwindow* windowHandle)
    {
        m_WindowHandle = windowHandle;

        CreateInstance();
        SetupDebugMessenger();
        CreateSurface();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapchain();
        CreateImageViews();
        CreateRenderPass();
        CreateDescriptorSetLayout();
        CreateGraphicsPipeline();
        CreateFramebuffers();
        CreateCommandPool();
        CreateCommandBuffers();
        CreateSyncObjects();
        CreateMemoryAllocator();
        CreateVertexBuffer();
        CreateIndexBuffer();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
    }

    void VulkanContext::DrawFrame()
    {
        vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, 
                                                m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return; 
        }

        vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);
        UpdateUniformBuffer(m_CurrentFrame);
        vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

        VkCommandBufferBeginInfo commandBufferInfo {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        if (vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &commandBufferInfo) != VK_SUCCESS) {
            AE_ENGINE_ERROR("Failed to begin recording command buffer!");
            return;
        }

        VkRenderPassBeginInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_RenderPass;
        renderPassInfo.framebuffer = m_SwapchainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_SwapchainExtent;

        VkClearValue clearColor = {{{0.01f, 0.01f, 0.03f, 1.0f}}}; 
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(m_CommandBuffers[m_CurrentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_SwapchainExtent.width);
        viewport.height = static_cast<float>(m_SwapchainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(m_CommandBuffers[m_CurrentFrame], 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset = {0, 0};
        scissor.extent = m_SwapchainExtent;
        vkCmdSetScissor(m_CommandBuffers[m_CurrentFrame], 0, 1, &scissor);

        VkBuffer vertexBuffers[] = { m_VertexBuffer };
        VkDeviceSize offsets[] = { 0 };

        vkCmdBindVertexBuffers(m_CommandBuffers[m_CurrentFrame], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(m_CommandBuffers[m_CurrentFrame], m_IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[m_CurrentFrame], 0, nullptr);
        vkCmdDrawIndexed(m_CommandBuffers[m_CurrentFrame], 36, 1, 0, 0, 0);
        vkCmdEndRenderPass(m_CommandBuffers[m_CurrentFrame]);

        if (vkEndCommandBuffer(m_CommandBuffers[m_CurrentFrame]) != VK_SUCCESS) {
            AE_ENGINE_ERROR("Failed to record command buffer!");
            return;
        }

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] { m_ImageAvailableSemaphores[m_CurrentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

        VkSemaphore signalSemaphores[] { m_RenderFinishedSemaphores[m_CurrentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
            AE_ENGINE_ERROR("Failed to submit draw command buffer!");
            return;
        }

        VkPresentInfoKHR presentInfo {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores; 

        VkSwapchainKHR swapchains[] { m_Swapchain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(m_PresentQueue, &presentInfo);
        vkQueueWaitIdle(m_PresentQueue);

        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    bool VulkanContext::CheckValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
        for(const char* layerName : m_ValidationLayers)
        {
            bool layerFound { false };

            for(const auto& layerProperties : availableLayers)
            {
                if(strcmp(layerName, layerProperties.layerName) == 0) 
                {
                    layerFound = true;
                    break;
                }
            }

            if(!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData
    )
    {
        if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            AE_ENGINE_ERROR("VULKAN VALIDATION: {0}", pCallbackData->pMessage);
        }
        else if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            AE_ENGINE_WARN("VULKAN VALIDATION: {0}", pCallbackData->pMessage);
        }
        else if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        {
            AE_ENGINE_TRACE("VULKAN VALIDATION: {0}", pCallbackData->pMessage);
        }

        return VK_FALSE;
    }

    bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        for (const char* requiredExt : m_DeviceExtensions)
        {
            bool found = false;

            for (const auto& availableExt : availableExtensions)
            {
                if (strcmp(requiredExt, availableExt.extensionName) == 0)
                {
                    found = true;
                    break;
                }
            }
            
            if (!found) { return false; }
        }

        return true;
    }

    QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount { 0 };
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i { 0 };
        for(const auto& queueFamily : queueFamilies)
        {
            if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.GraphicsFamily = i;
            }

            VkBool32 presentSupport { false };
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
            
            if(presentSupport)
            {
                indices.PresentFamily = i;
            }

            if(indices.IsComplete()) { break; }
        
            i++;
        }
        return indices;
    }

    SwapchainSupportDetails VulkanContext::QuerySwapChainSupport(VkPhysicalDevice device)
    {
        SwapchainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.Capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

        if(formatCount != 0)
        {
            details.Formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.Formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

        if(presentModeCount != 0)
        {
            details.PresentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.PresentModes.data());
        }

        return details;
    }

    bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        QueueFamilyIndices indices { FindQueueFamilies(device) };

        bool extensionsSupported { CheckDeviceExtensionSupport(device) };
        bool swapChainAdequate { false };

        if(extensionsSupported)
        {
            SwapchainSupportDetails swapChainSupport { QuerySwapChainSupport(device) };
            swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
        }
        
        return indices.IsComplete() &&
               extensionsSupported &&
               swapChainAdequate &&
               deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
               deviceFeatures.geometryShader;
    }

    VkSurfaceFormatKHR VulkanContext::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for(const auto& availableFormat : availableFormats)
        {
            if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                AE_ENGINE_TRACE("Swapchain Format: B8G8R8A8_SRGB");
                return availableFormat;
            }
        }

        AE_ENGINE_TRACE("Swapchain Format: Default (ID: {0})", (int)availableFormats[0].format);
        return availableFormats[0];
    }

    VkPresentModeKHR VulkanContext::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        for(const auto& availablePresentMode : availablePresentModes)
        {
            if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                AE_ENGINE_TRACE("Swapchain Present Mode: MAILBOX");
                return availablePresentMode;
            }
        }

        AE_ENGINE_TRACE("Swapchain Present Mode: FIFO (V-Sync - Capped FPS)");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VulkanContext::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* windowHandle)
    {
        if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
        { 
            AE_ENGINE_TRACE("Swapchain Extent: {0}x{1}", capabilities.currentExtent.width, capabilities.currentExtent.height);
            return capabilities.currentExtent; 
        }

        int width, height;
        glfwGetFramebufferSize(windowHandle, &width, &height);

        VkExtent2D actualExtent { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        AE_ENGINE_TRACE("Swapchain Extent: {0}x{1}", actualExtent.width, actualExtent.height);
        return actualExtent;
    }

    std::vector<char> VulkanContext::ReadFile(const std::string& fileName)
    {
        std::ifstream file(fileName, std::ios::ate | std::ios::binary);
        
        if(!file.is_open())
        {
            AE_ENGINE_CRITICAL("Failed to open file: {0}", fileName);
            throw std::runtime_error("Failed to open file!");
        }

        size_t fileSize { (size_t)file.tellg() };
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    VkShaderModule VulkanContext::CreateShaderModule(const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo shaderModuleInfo {};
        shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleInfo.codeSize = code.size();
        shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        
        if(vkCreateShaderModule(m_Device, &shaderModuleInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Shader Module!");
        }
        return shaderModule;
    }

    VkVertexInputBindingDescription VulkanContext::GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    std::array<VkVertexInputAttributeDescription, 2> VulkanContext::GetAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        return attributeDescriptions;
    }

    void VulkanContext::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
    {
        VkCommandBufferAllocateInfo allocationInfo {};
        allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocationInfo.commandPool = m_CommandPool;
        allocationInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_Device, &allocationInfo, &commandBuffer);

        VkCommandBufferBeginInfo commandBufferBeginInfo {};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

        VkBufferCopy copyRegion {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = bufferSize;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo,VK_NULL_HANDLE);
        vkQueueWaitIdle(m_GraphicsQueue);
        vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer); 
    }

    void VulkanContext::CreateStagingBuffer(const void* data, VkDeviceSize bufferSize, VkBuffer& outBuffer, VmaAllocation& outAllocation)
    {
        VkBufferCreateInfo stagingBufferInfo {};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; 
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if (vmaCreateBuffer(m_Allocator, &stagingBufferInfo, &allocationInfo, &outBuffer, &outAllocation, nullptr) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create staging buffer! Size: {0} bytes.", bufferSize);
            return;
        }
        AE_ENGINE_TRACE("Staging buffer created. Size: {0} bytes.", bufferSize);
        
        vmaCopyMemoryToAllocation(m_Allocator, data, outAllocation, 0, bufferSize);
        
        AE_ENGINE_TRACE("Data mapped and copied to staging buffer.");
    }

    void VulkanContext::UpdateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), m_SwapchainExtent.width / (float)m_SwapchainExtent.height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void VulkanContext::CreateInstance()
    {
        if(m_EnableValidationLayers && !CheckValidationLayerSupport())
        {
            AE_ENGINE_CRITICAL("Validation layers requested, but not available!");
            return;
        }

        VkApplicationInfo appInfo {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Antelope Editor";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Antelope Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions { glfwGetRequiredInstanceExtensions(&glfwExtensionCount) };
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if(m_EnableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        
        VkInstanceCreateInfo instanceInfo {};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT messengerInfo {};

        if(m_EnableValidationLayers)
        {
            instanceInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            instanceInfo.ppEnabledLayerNames = m_ValidationLayers.data();
            AE_ENGINE_TRACE("Vulkan Validation Layers enabled.");

            messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            messengerInfo.pfnUserCallback = DebugCallback;
            instanceInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &messengerInfo;
        }
        else
        {
            instanceInfo.enabledLayerCount = 0;
            instanceInfo.pNext = nullptr;
            AE_ENGINE_INFO("Vulkan Validation Layers disabled (Release Mode).");
        }

        VkResult result { vkCreateInstance(&instanceInfo, nullptr, &m_Instance) };

        if(result != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Vulkan Instance! Error Code: {0}", (int)result);
            return;
        }
        else
        {
            AE_ENGINE_INFO("Vulkan Instance created successfully!");
        }
    }

    void VulkanContext::SetupDebugMessenger()
    {
        if(!m_EnableValidationLayers) { return; }

        VkDebugUtilsMessengerCreateInfoEXT messengerInfo {};
        messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messengerInfo.pfnUserCallback = DebugCallback;

        if(CreateDebugUtilsMessengerEXT(m_Instance, &messengerInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to setup debug messenger!");
        }
        else
        {
            AE_ENGINE_TRACE("Vulkan Debug Messenger connected to Engine Logger.");
        }
    }

    void VulkanContext::CreateSurface()
    {
        if(glfwCreateWindowSurface(m_Instance, m_WindowHandle, nullptr, &m_Surface) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Window Surface!");
            return;
        }
        else
        {
            AE_ENGINE_TRACE("Vulkan Window Surface created successfully.");
        }
    }

    void VulkanContext::PickPhysicalDevice()
    {
        uint32_t deviceCount { 0 };
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

        if(deviceCount == 0)
        {
            AE_ENGINE_CRITICAL("Failed to find GPUs with Vulkan support!");
            return;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        for(const auto device : devices)
        {
            if(IsDeviceSuitable(device))
            {
                m_PhysicalDevice = device;
                break;
            }
        }

        if(m_PhysicalDevice == VK_NULL_HANDLE)
        {
            AE_ENGINE_CRITICAL("Failed to find a suitable GPU");
        }
        else
        {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
            AE_ENGINE_INFO("Physical Device Selected: {0} (ID: {1})", properties.deviceName, properties.deviceID);
        }
    }

    void VulkanContext::CreateLogicalDevice()
    {
        QueueFamilyIndices indices { FindQueueFamilies(m_PhysicalDevice) };

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

        float queuePriority { 0.1f };

        for(uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo deviceQueueInfo {};
            deviceQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            deviceQueueInfo.queueFamilyIndex = queueFamily;
            deviceQueueInfo.queueCount = 1;
            deviceQueueInfo.pQueuePriorities = &queuePriority;

            queueCreateInfos.push_back(deviceQueueInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures {};

        VkDeviceCreateInfo deviceInfo {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceInfo.pEnabledFeatures = &deviceFeatures;
        deviceInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

        if(m_EnableValidationLayers)
        {
            deviceInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            deviceInfo.ppEnabledLayerNames = m_ValidationLayers.data();
        }
        else
        {
            deviceInfo.enabledLayerCount = 0;
        }

        if(vkCreateDevice(m_PhysicalDevice, &deviceInfo, nullptr, &m_Device) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Logical Device!");
            return;
        }
        else
        {
            AE_ENGINE_INFO("Vulkan Logical Device created successfully.");
        }

        vkGetDeviceQueue(m_Device, indices.GraphicsFamily.value(), 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, indices.PresentFamily.value(), 0, &m_PresentQueue);
    }

    void VulkanContext::CreateMemoryAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo {};
    allocatorInfo.physicalDevice = m_PhysicalDevice;
    allocatorInfo.device = m_Device;
    allocatorInfo.instance = m_Instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    // allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    if (vmaCreateAllocator(&allocatorInfo, &m_Allocator) != VK_SUCCESS) {
        AE_ENGINE_CRITICAL("Failed to create VMA Allocator!");
    } else {
        AE_ENGINE_TRACE("VMA Allocator created successfully.");
    }
}

    void VulkanContext::CreateSwapchain()
    {
        SwapchainSupportDetails swapchainSupport { QuerySwapChainSupport(m_PhysicalDevice) };
        VkSurfaceFormatKHR surfaceFormat { ChooseSwapSurfaceFormat(swapchainSupport.Formats) };
        VkPresentModeKHR presentMode { ChooseSwapPresentMode(swapchainSupport.PresentModes) };
        VkExtent2D extent { ChooseSwapExtent(swapchainSupport.Capabilities, m_WindowHandle) };
        uint32_t imageCount { swapchainSupport.Capabilities.minImageCount + 1 };

        if(swapchainSupport.Capabilities.maxImageCount > 0 && imageCount > swapchainSupport.Capabilities.maxImageCount)
        {
            imageCount = swapchainSupport.Capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR swapchainInfo {};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = m_Surface;
        swapchainInfo.minImageCount = imageCount;
        swapchainInfo.imageFormat = surfaceFormat.format;
        swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainInfo.imageExtent = extent;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices { FindQueueFamilies(m_PhysicalDevice) };
        uint32_t queueFamilyIndices[] { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

        if(indices.GraphicsFamily != indices.PresentFamily)
        {
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchainInfo.queueFamilyIndexCount = 2;
            swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchainInfo.queueFamilyIndexCount = 0;
            swapchainInfo.pQueueFamilyIndices = nullptr;
        }

        swapchainInfo.preTransform = swapchainSupport.Capabilities.currentTransform;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = presentMode;
        swapchainInfo.clipped = VK_TRUE;
        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

        if(vkCreateSwapchainKHR(m_Device, &swapchainInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Swapchain!");
            return;
        }

        AE_ENGINE_INFO("Vulkan Swapchain created successfully.");

        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
        m_SwapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

        m_SwapchainImageFormat = surfaceFormat.format;
        m_SwapchainExtent = extent;

        AE_ENGINE_TRACE("Swapchain images retrieved. Count: {0}", imageCount);
    }

    void VulkanContext::CreateImageViews()
    {
        uint32_t imageViewsSize = m_SwapchainImages.size();
        m_SwapchainImageViews.resize(imageViewsSize);

        for(size_t i = 0; i < imageViewsSize; ++i)
        {
            VkImageViewCreateInfo ImageViewInfo {};
            ImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ImageViewInfo.image = m_SwapchainImages[i];
            ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ImageViewInfo.format = m_SwapchainImageFormat;
            ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ImageViewInfo.subresourceRange.baseMipLevel = 0;
            ImageViewInfo.subresourceRange.levelCount = 1;
            ImageViewInfo.subresourceRange.baseArrayLayer = 0;
            ImageViewInfo.subresourceRange.layerCount = 1;

            if(vkCreateImageView(m_Device, &ImageViewInfo, nullptr, &m_SwapchainImageViews[i]) != VK_SUCCESS)
            {
                AE_ENGINE_CRITICAL("Failed to create Swapchain Image Views!");
                return;
            }
        }
        AE_ENGINE_INFO("Vulkan Swapchain Image Views created successfully.");
    }

    void VulkanContext::CreateRenderPass()
    {
        VkAttachmentDescription colorAttachment {};
        colorAttachment.format = m_SwapchainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if(vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Render Pass!");
            return;
        }

        AE_ENGINE_INFO("Vulkan Render Pass created successfully.");
    }

    void VulkanContext::CreateDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding {};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1; 
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; 
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Descriptor Set Layout!");
            return;
        }
        AE_ENGINE_TRACE("Vulkan Descriptor Set Layout created.");
    }

    void VulkanContext::CreateGraphicsPipeline()
    {
        auto vertShaderCode {ReadFile("Shaders/base.vert.spv")};
        auto fragShaderCode {ReadFile("Shaders/base.frag.spv")};

        AE_ENGINE_TRACE("Vertex Shader size: {0} bytes", vertShaderCode.size());
        AE_ENGINE_TRACE("Fragment Shader size: {0} bytes", fragShaderCode.size());

        VkShaderModule vertShaderModule { CreateShaderModule(vertShaderCode) }; 
        VkShaderModule fragShaderModule { CreateShaderModule(fragShaderCode) };

        VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] {vertShaderStageInfo, fragShaderStageInfo};

        auto bindingDescription = GetBindingDescription();
        auto attributeDescriptions = GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputStateInfo {};
        vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputStateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputStateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo {};
        inputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

        std::vector<VkDynamicState> dynamicStates {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicStateInfo {};
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateInfo.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportStateInfo {};
        viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateInfo.viewportCount = 1;
        viewportStateInfo.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizationStateInfo {};
        rasterizationStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateInfo.depthClampEnable = VK_FALSE;
        rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateInfo.lineWidth = 1.0f;
        rasterizationStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationStateInfo.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampleStateInfo{};
        multisampleStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleStateInfo.sampleShadingEnable = VK_FALSE;
        multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachmentStateInfo{};
        colorBlendAttachmentStateInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT 
                                            | VK_COLOR_COMPONENT_G_BIT 
                                            | VK_COLOR_COMPONENT_B_BIT 
                                            | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachmentStateInfo.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachmentStateInfo;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create Pipeline Layout!");
            return;
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputStateInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyStateInfo;
        pipelineInfo.pViewportState = &viewportStateInfo;
        pipelineInfo.pRasterizationState = &rasterizationStateInfo;
        pipelineInfo.pMultisampleState = &multisampleStateInfo;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicStateInfo;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = m_RenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create Graphics Pipeline!");
            return;
        }

        vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
        vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);

        AE_ENGINE_INFO("Vulkan Graphics Pipeline created successfully.");
    }

    void VulkanContext::CreateFramebuffers()
    {
        m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());

        for (size_t i = 0; i < m_SwapchainImageViews.size(); i++) 
        {
            VkImageView attachments[] = { m_SwapchainImageViews[i] };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_SwapchainExtent.width;
            framebufferInfo.height = m_SwapchainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapchainFramebuffers[i]) != VK_SUCCESS) 
            {
                AE_ENGINE_CRITICAL("Failed to create Framebuffer!");
                return;
            }
        }
        AE_ENGINE_INFO("Vulkan Framebuffers created successfully.");
    }

    void VulkanContext::CreateCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = queueFamilyIndices.GraphicsFamily.value();

        if (vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Command Pool!");
            return;
        }
        AE_ENGINE_TRACE("Vulkan Command Pool created successfully.");
    }

    void VulkanContext::CreateCommandBuffers()
    {
        m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo commandBufferAllocationInfo {};
        commandBufferAllocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocationInfo.commandPool = m_CommandPool;
        commandBufferAllocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocationInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkAllocateCommandBuffers(m_Device, &commandBufferAllocationInfo, m_CommandBuffers.data()) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to allocate Command Buffers!");
            return;
        }
        AE_ENGINE_TRACE("Vulkan Command Buffers allocated for {0} frames.", MAX_FRAMES_IN_FLIGHT);
    }

    void VulkanContext::CreateSyncObjects() 
    {
        m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {
                AE_ENGINE_ERROR("Failed to create synchronization objects for frame {0}", i);
            }
        }
        AE_ENGINE_TRACE("Vulkan Synchronization Objects created for {0} frames.", MAX_FRAMES_IN_FLIGHT);
    }

    void VulkanContext::CreateVertexBuffer()
    {
        const std::array<Vertex, 8> vertices = 
    {{
        
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
        
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}}
    }};

        VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        CreateStagingBuffer(vertices.data(), bufferSize, stagingBuffer, stagingBufferAllocation);

        VkBufferCreateInfo vertexBufferInfo {};
        vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertexBufferInfo.size = bufferSize;
        vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vertexAllocInfo {};
        vertexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if(vmaCreateBuffer(m_Allocator, &vertexBufferInfo, &vertexAllocInfo, &m_VertexBuffer, &m_VertexBufferAllocation, nullptr) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create device local vertex buffer!");
            return;
        }
        AE_ENGINE_TRACE("Device local vertex buffer created. Size: {0} bytes.", bufferSize);

        CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);
        AE_ENGINE_TRACE("Data copied from staging buffer to vertex buffer.");

        vmaDestroyBuffer(m_Allocator, stagingBuffer, stagingBufferAllocation);
        AE_ENGINE_TRACE("Staging buffer destroyed.");
    }

    void VulkanContext::CreateIndexBuffer()
    {
        const std::array<uint16_t, 36> indices = 
        {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4,
            0, 1, 5, 5, 4, 0,
            2, 3, 7, 7, 6, 2,
            1, 2, 6, 6, 5, 1,
            3, 0, 4, 4, 7, 3
        };

        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        CreateStagingBuffer(indices.data(), bufferSize, stagingBuffer, stagingBufferAllocation);

        VkBufferCreateInfo indexBufferInfo {};
        indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        indexBufferInfo.size = bufferSize;
        indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo indexAllocInfo {};
        indexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if(vmaCreateBuffer(m_Allocator, &indexBufferInfo, &indexAllocInfo, &m_IndexBuffer, &m_IndexBufferAllocation, nullptr) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create device local index buffer!");
            return;
        }
        AE_ENGINE_TRACE("Device local index buffer created. Size: {0} bytes.", bufferSize);

        CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);
        AE_ENGINE_TRACE("Data copied from staging buffer to index buffer.");

        vmaDestroyBuffer(m_Allocator, stagingBuffer, stagingBufferAllocation);
        AE_ENGINE_TRACE("Index staging buffer destroyed.");
    }

    void VulkanContext::CreateUniformBuffers()
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_UniformBuffersAllocations.resize(MAX_FRAMES_IN_FLIGHT);
        m_UniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            VkBufferCreateInfo uniformBufferInfo {};
            uniformBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            uniformBufferInfo.size = bufferSize;
            uniformBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            uniformBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocationInfo {};
            allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                   VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VmaAllocationInfo allocationResult;

            if (vmaCreateBuffer(m_Allocator, &uniformBufferInfo, &allocationInfo, &m_UniformBuffers[i], &m_UniformBuffersAllocations[i], &allocationResult) != VK_SUCCESS) 
            {
                AE_ENGINE_CRITICAL("Failed to create Uniform Buffer for frame {0}!", i);
                return;
            }

            m_UniformBuffersMapped[i] = allocationResult.pMappedData;
        }
        AE_ENGINE_TRACE("Uniform buffers created and mapped for {0} frames. Total size: {1} bytes.", MAX_FRAMES_IN_FLIGHT, bufferSize * MAX_FRAMES_IN_FLIGHT);
    }

    void VulkanContext::CreateDescriptorPool()
    {
        VkDescriptorPoolSize descriptorPoolSize {};
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorPoolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo descriptorPoolInfo {};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.poolSizeCount = 1;
        descriptorPoolInfo.pPoolSizes = &descriptorPoolSize;
        descriptorPoolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(m_Device, &descriptorPoolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create Descriptor Pool!");
            return;
        }
        AE_ENGINE_TRACE("Descriptor pool created.");
    }

    void VulkanContext::CreateDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);
        
        VkDescriptorSetAllocateInfo decriptorSetallocationInfo {};
        decriptorSetallocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        decriptorSetallocationInfo.descriptorPool = m_DescriptorPool;
        decriptorSetallocationInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        decriptorSetallocationInfo.pSetLayouts = layouts.data();

        m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        
        if (vkAllocateDescriptorSets(m_Device, &decriptorSetallocationInfo, m_DescriptorSets.data()) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to allocate Descriptor Sets!");
            return;
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            VkDescriptorBufferInfo descriptorBufferInfo {};
            descriptorBufferInfo.buffer = m_UniformBuffers[i];
            descriptorBufferInfo.offset = 0;
            descriptorBufferInfo.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet descriptorWriteSet {};
            descriptorWriteSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWriteSet.dstSet = m_DescriptorSets[i];
            descriptorWriteSet.dstBinding = 0;
            descriptorWriteSet.dstArrayElement = 0;
            descriptorWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWriteSet.descriptorCount = 1;
            descriptorWriteSet.pBufferInfo = &descriptorBufferInfo;

            vkUpdateDescriptorSets(m_Device, 1, &descriptorWriteSet, 0, nullptr);
        }
        AE_ENGINE_TRACE("Descriptor sets allocated and bound.");
    }
}
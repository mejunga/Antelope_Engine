#include <Engine/Debug/Log.hpp>
#include <Engine/Renderer/VulkanContext.hpp>
#include <GLFW/glfw3.h>
#include <cstring>
#include <algorithm>
#include <limits>
#include <set>
#include <fstream>

namespace Antelope
{
    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance, 
        const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, 
        const VkAllocationCallbacks *pAllocator,
        VkDebugUtilsMessengerEXT *pDebugMessenger
    )
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

        if(func != nullptr)
        {
            return(func(instance, pCreateInfo, pAllocator, pDebugMessenger));
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks *pAllocator
    )
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

        if(func != nullptr)
        {
            func(instance, debugMessenger, pAllocator);
        }
    }

    VulkanContext::VulkanContext() {}
    VulkanContext::~VulkanContext() 
    {
        if (m_Device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_Device);
        }

        if (m_VertexBuffer != VK_NULL_HANDLE) 
        {
            vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
            AE_ENGINE_TRACE("Vulkan Vertex Buffer destroyed.");
        }

        if (m_VertexBufferMemory != VK_NULL_HANDLE) 
        {
            vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);
            AE_ENGINE_TRACE("Vulkan Vertex Buffer Memory freed.");
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
        CreateGraphicsPipeline();
        CreateFramebuffers();
        CreateCommandPool();
        CreateCommandBuffer();
        CreateSyncObjects();
        CreateVertexBuffer();
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
        vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        if (vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo) != VK_SUCCESS) {
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

        vkCmdDraw(m_CommandBuffers[m_CurrentFrame], 3, 1, 0, 0);

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
        VkShaderModuleCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        
        if(vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
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
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        return attributeDescriptions;
    }

    uint32_t VulkanContext::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memoryProperties);

        for(uint32_t i { 0 }; i < memoryProperties.memoryTypeCount; ++i)
        {
            if((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
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
        
        VkInstanceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};

        if(m_EnableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
            AE_ENGINE_TRACE("Vulkan Validation Layers enabled.");

            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = DebugCallback;
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
            AE_ENGINE_INFO("Vulkan Validation Layers disabled (Release Mode).");
        }

        VkResult result { vkCreateInstance(&createInfo, nullptr, &m_Instance) };

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

        VkDebugUtilsMessengerCreateInfoEXT createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;

        if(CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
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
            VkDeviceQueueCreateInfo queueCreateInfo {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures {};

        VkDeviceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

        if(m_EnableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        if(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
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

        VkSwapchainCreateInfoKHR createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_Surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices { FindQueueFamilies(m_PhysicalDevice) };
        uint32_t queueFamilyIndices[] { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

        if(indices.GraphicsFamily != indices.PresentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapchainSupport.Capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if(vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
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
            VkImageViewCreateInfo createInfo {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_SwapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = m_SwapchainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if(vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapchainImageViews[i]) != VK_SUCCESS)
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

        VkRenderPassCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &colorAttachment;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;

        if(vkCreateRenderPass(m_Device, &createInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Render Pass!");
            return;
        }

        AE_ENGINE_INFO("Vulkan Render Pass created successfully.");
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

        VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        std::vector<VkDynamicState> dynamicStates {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT 
                                            | VK_COLOR_COMPONENT_G_BIT 
                                            | VK_COLOR_COMPONENT_B_BIT 
                                            | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
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
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
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

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.GraphicsFamily.value();

        if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Command Pool!");
            return;
        }
        AE_ENGINE_TRACE("Vulkan Command Pool created successfully.");
    }

    void VulkanContext::CreateCommandBuffer()
    {
        m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
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
        const std::array<Vertex, 3> vertices = 
        {{
            {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}} 
        }};

        VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

        VkBufferCreateInfo bufferCreateInfo {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = bufferSize;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferCreateInfo.sharingMode= VK_SHARING_MODE_EXCLUSIVE;

        if(vkCreateBuffer(m_Device, &bufferCreateInfo, nullptr, &m_VertexBuffer) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create vertex buffer!");
            return;
        }

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(m_Device, m_VertexBuffer, &memoryRequirements);

        VkMemoryAllocateInfo allocationInfo {};
        allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocationInfo.allocationSize = memoryRequirements.size;
        allocationInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 
                                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if(vkAllocateMemory(m_Device, &allocationInfo, nullptr, &m_VertexBufferMemory) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to allocate vertex buffer memory!");
            return;
        }
        
        vkBindBufferMemory(m_Device, m_VertexBuffer, m_VertexBufferMemory, 0);
        
        void* data;
        vkMapMemory(m_Device, m_VertexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t)bufferSize);
        vkUnmapMemory(m_Device, m_VertexBufferMemory);

        AE_ENGINE_TRACE("Vertex Buffer created and data uploaded to GPU.");
    }
}
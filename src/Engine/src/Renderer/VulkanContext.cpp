#include <Engine/Debug/Log.hpp>
#include <Engine/Renderer/VulkanContext.hpp>
#include <GLFW/glfw3.h>
#include <cstring>
#include <algorithm>
#include <limits>
#include <set>

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

        if(m_EnableValidationLayers)
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

    void VulkanContext::Init(GLFWwindow* windowHandle) 
    {
        m_WindowHandle = windowHandle;

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

        SetupDebugMessenger();

        if(glfwCreateWindowSurface(m_Instance, windowHandle, nullptr, &m_Surface) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Window Surface!");
            return;
        }
        else
        {
            AE_ENGINE_TRACE("Vulkan Window Surface created successfully.");
        }

        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapchain();
    }
}